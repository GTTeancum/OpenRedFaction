"""Report real ordinary-level checkpoint capture blockers without altering assets.
This is a diagnostic replay, not save/load acceptance. No host input or UI control.
"""
import argparse, json, os, re, struct, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

SECTION_NAMES = ('player', 'npc', 'mover', 'events', 'triggers', 'goals', 'pickups',
                 'clutter', 'weapon_modes', 'remote', 'vehicle', 'destruction', 'switches',
                 'startup', 'campaign_history')
HISTORY_OMISSIONS = {1: 'player campaign carry', 2: 'weapon-mode campaign sidecars',
                     4: 'NPC shield history', 8: 'NPC AI-mode history'}

def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & 0xffffffff
    return h

def inspect_slot(path):
    raw = path.read_bytes()
    if len(raw) < 24 or raw[:4] != b'RFSG':
        raise ValueError('invalid RFSG header')
    version, generation, size, checksum, header_checksum = struct.unpack_from('<5I', raw, 4)
    if version != 1 or not generation or not 0 < size <= 110524 or len(raw) != size + 24:
        raise ValueError('invalid RFSG version/generation/length')
    payload = raw[24:]
    if fnv(raw[:20]) != header_checksum or fnv(payload) != checksum:
        raise ValueError('RFSG checksum mismatch')
    if len(payload) < 308 or payload[:4] != b'RFWC':
        raise ValueError('missing RFWC envelope')
    version, total, checksum, count, reserved = struct.unpack_from('<5I', payload, 4)
    if version != 1 or total != size or count != 15 or reserved or any(payload[120:128]):
        raise ValueError('invalid RFWC header')
    if fnv(payload[:12] + bytes(4) + payload[16:]) != checksum:
        raise ValueError('RFWC checksum mismatch')
    level = payload[56:120]
    if not level[0] or b'\0' not in level or any(level[level.index(0):]):
        raise ValueError('invalid RFWC level name')
    sections = {}; at = 308
    for i, name in enumerate(SECTION_NAMES):
        kind, offset, length = struct.unpack_from('<3I', payload, 128 + 12 * i)
        if kind != i + 1 or offset != at or length > size - at:
            raise ValueError('invalid RFWC directory: ' + name)
        section = payload[at:at + length]
        sections[name] = dict(bytes=length, present=bool(length),
                              magic=section[:4].decode('ascii', errors='replace') if length else None)
        at += length
    if at != size:
        raise ValueError('unclaimed RFWC bytes')
    for name in ('player', 'npc', 'mover', 'triggers', 'goals', 'pickups', 'clutter',
                 'weapon_modes', 'startup', 'campaign_history'):
        if not sections[name]['present']:
            raise ValueError('required section absent: ' + name)
    return dict(path=str(path), generation=generation, bytes=size, level=level.split(b'\0')[0].decode('ascii'),
                identity=payload[24:56].hex(), transport_verified=True, envelope_verified=True,
                component_semantics_verified=False, sections=sections)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot', action='store_true', help='Capture an ordinary-world snapshot; does not reload it.')
    parser.add_argument("--restore-probe", action="store_true", help="Stage real NPC/mover/prop candidates without publishing; requires --snapshot.")
    args = parser.parse_args()
    if args.restore_probe and not args.snapshot:
        parser.error("--restore-probe requires --snapshot")
    folder = ROOT / ('artifacts/ordinary-save-snapshot' if args.snapshot else 'artifacts/ordinary-save-readiness')
    folder.mkdir(parents=True, exist_ok=True)
    for name in ('report.json', 'frame.ppm'):
        (folder / name).unlink(missing_ok=True)
    frame = struct.pack('<5f7I', *([0] * 12))
    (folder / 'input.bin').write_bytes(b'RFI6' + struct.pack('<I', 48) + frame * 120)
    env = {k: v for k, v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
    env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_ARCHIVE='levels1.vpp', RF_REPLAY_WORLD_CHECKPOINT_PROBE='1')
    snapshot_base = folder / 'world.snapshot'
    if args.snapshot:
        for slot in range(2):
            Path(str(snapshot_base) + '.' + str(slot)).unlink(missing_ok=True)
        env['RF_REPLAY_WORLD_SNAPSHOT_OUT'] = str(snapshot_base)
        if args.restore_probe:
            env['RF_REPLAY_WORLD_RESTORE_PROBE'] = '1'
    with (folder / 'run.log').open('wb') as log:
        run = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
            str(ROOT / 'Installed_Game'), str(folder / 'input.bin'), str(folder / 'frame.ppm')],
            cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=240)
    text = (folder / 'run.log').read_text(errors='replace')
    components = {name: dict(status=int(status), bytes=int(size)) for name, status, size in
        re.findall(r'WORLD_CHECKPOINT_PROBE (\w+) status(-?\d+) bytes(\d+)', text)}
    details = [line for line in text.splitlines() if line.startswith(('WORLD_CHECKPOINT_NPC ', 'WORLD_CHECKPOINT_NPC_ADMISSION ', 'WORLD_CHECKPOINT_NPC_SCRIPT ', 'WORLD_CHECKPOINT_EVENT ', 'WORLD_CHECKPOINT_PLAYER '))]
    reached = 'WORLD_CHECKPOINT_PROBE_ONLY no_save_written' in text
    result = dict(status='STARTUP_OR_REPLAY_FAILED' if run.returncode or not reached else
        'CAPTURE_BLOCKED' if details or any(v['status'] for v in components.values()) else 'COMPONENT_CAPTURE_READY_RESTORE_UNIMPLEMENTED',
        exit_code=run.returncode, level='Installed_Game/levels1.vpp:L1S1.rfl', frames=120,
        component_capture=components, details=details, save_written=False, reload_verified=False,
        visual_content_verified=False, native_memory_verified=False)
    if args.snapshot:
        stored = re.search(r'WORLD_SNAPSHOT_STORED bytes(\d+) slot(\d+) generation(\d+) history_omissions(\d+) restore_available0', text)
        captured = {name: int(size) for name, size in re.findall(r'WORLD_SNAPSHOT_COMPONENT (\w+) (\d+)', text)}
        slots = {}; errors = []
        for slot in range(2):
            path = Path(str(snapshot_base) + '.' + str(slot))
            if path.exists():
                try:
                    slots[str(slot)] = inspect_slot(path)
                except (ValueError, OSError, struct.error) as exc:
                    errors.append(f'slot {slot}: {exc}')
        omissions = int(stored[4]) if stored else None
        if stored:
            selected = slots.get(stored[2])
            if not selected or selected['generation'] != int(stored[3]) or selected['bytes'] != int(stored[1]):
                errors.append('stored diagnostic does not match persisted slot')
            elif any(selected['sections'][name]['bytes'] != size for name, size in captured.items() if name in SECTION_NAMES):
                errors.append('captured component sizes do not match persisted directory')
        verified = bool(stored and not errors and not run.returncode)
        result.update(status=('SNAPSHOT_CAPTURED_WITH_HISTORY_OMISSIONS' if omissions else 'SNAPSHOT_CAPTURED_NOT_RELOADED')
                      if verified else 'SNAPSHOT_CAPTURE_FAILED', capture_only=True, save_written=verified,
                      snapshot_components=captured, persisted_slots=slots, snapshot_errors=errors,
                      snapshot_rejections=re.findall(r'WORLD_SNAPSHOT_REJECT ([^\n]+)', text),
                      history_omission_mask=omissions,
                      history_omissions=[label for bit, label in HISTORY_OMISSIONS.items() if omissions is not None and omissions & bit],
                      limitation='Transport/envelope capture only; gameplay reload and history restoration are not verified.')
    result['restore_probe'] = [line for line in text.splitlines() if line.startswith(('WORLD_RESTORE_',))]
    (folder / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
