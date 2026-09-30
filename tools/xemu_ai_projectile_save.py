"""Xbox-only bounded save/load of controlled NPC and player projectile flights.

The original game is not run. A two-actor L1S1 asset fixture is prepared from
the installed archive; guest input is process-local and neutral. No image,
desktop control, campaign playthrough or persistent user HDD is involved.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import struct

from check_ai_projectile_ordinary import prepare as prepare_encounter
from check_rocket_pickup import prepare as prepare_player_rocket
from xemu_native_world_save import DISC, FLAGS, ROOT, build, run_guest
from xemu_session_guard import require_no_project_xemu


def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def fixture(payload):
    header = bytearray(struct.pack('<4s4I', b'RFSG', 1, 1, len(payload), checksum(payload)))
    header += struct.pack('<I', checksum(header))
    return bytes(header) + payload


def projectile_count(payload, kind):
    if payload[:4] != b'RFWC' or struct.unpack_from('<I', payload, 4)[0] != 3:
        raise RuntimeError('Expected RFWC3 with active projectile state')
    section, offset, size = struct.unpack_from('<3I', payload, 128 + 16 * 12)
    if section != 17 or offset + size != len(payload) or size < 16:
        raise RuntimeError('Invalid projectile section directory')
    part = payload[offset:]
    magic, version, count, reserved = struct.unpack_from('<4s3I', part)
    if (magic, version, reserved) != (b'RFAP', 1, 0) or len(part) != 16 + 112 * count:
        raise RuntimeError('Invalid projectile component')
    records = [struct.unpack_from('<I', part, 16 + 112 * index)[0]
               for index in range(count)]
    wanted = {'grenade': 1, 'rocket': 2, 'player-rocket': 3}[kind]
    if not records or any(value != wanted for value in records):
        raise RuntimeError('Unexpected mixed/empty projectile fixture')
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kind', choices=('grenade', 'rocket', 'player-rocket'), default='grenade')
    parser.add_argument('--save-frames', type=int)
    parser.add_argument('--load-frames', type=int, default=60)
    parser.add_argument('--seconds', type=int, default=360)
    parser.add_argument('--load-world', type=Path,
                        help='Retry an existing Xbox RFWC3 payload without running the encounter save')
    args = parser.parse_args()
    save_frames = args.save_frames or {'grenade': 200, 'rocket': 34, 'player-rocket': 122}[args.kind]
    if not 30 <= save_frames <= 1200 or not 1 <= args.load_frames <= 300:
        parser.error('Save frames 30..1200 and load frames 1..300 required')
    require_no_project_xemu(ROOT)
    base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not base.is_file():
        raise RuntimeError('Missing isolated XEMU base HDD')
    run = ROOT / 'artifacts/xemu' / ('npc-projectile-save-' +
          datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    run.mkdir(parents=True)
    if args.kind == 'player-rocket':
        recipe = prepare_player_rocket(run / 'encounter', 1)
        source = run / 'encounter/rocket/game/levelsm.vpp'
    else:
        recipe = prepare_encounter(run / 'encounter', save_frames)
        source = run / 'encounter' / args.kind / 'game' / 'levels1.vpp'
    flags = set(FLAGS) | {'scene-fixture.vpp'} | {
        p.name for p in DISC.glob('campaign-*') if p.is_file()}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(flags) if name != 'scene-fixture.vpp'}
    if (DISC / 'scene-fixture.vpp').exists():
        raise RuntimeError('Existing scene-fixture.vpp; inspect before staging another fixture')
    report = dict(result='FAIL', scope='Xbox-only active ' + args.kind +
                  ' ordinary save and reload', save_frames=save_frames,
                  load_frames=args.load_frames, fixture_recipe=recipe['scope'], phases={})
    try:
        for name in original:
            (DISC / name).unlink(missing_ok=True)
        shutil.copyfile(source, DISC / 'scene-fixture.vpp')
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'scene-fixture.vpp'.ljust(64, b'\0') +
            (b'ctf06.rfl' if args.kind == 'player-rocket' else b'L1S1.rfl').ljust(64, b'\0'))
        if args.kind == 'player-rocket':
            (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<I', 910510))
        else:
            (DISC / 'campaign-actor.bin').write_bytes(struct.pack('<I', 8456))
        key = {'grenade': 'rf_scene_ai_grenades', 'rocket': 'rf_scene_ai_rockets',
               'player-rocket': 'rf_scene_rockets'}[args.kind]
        counters = {key: 8 if args.kind == 'player-rocket' else 5,
                    'rf_scene_npc_checkpoint_reject_state': 6,
                    'rf_scene_world_load_reject': 3,
                    'rf_scene_world_player_probe': 3,
                    'rf_scene_checkpoint_world_reject': 9}
        if args.kind == 'rocket':
            counters['rf_scene_ai_rocket_timing'] = 3
        if args.kind == 'player-rocket':
            counters['combat_trigger'] = 4
            counters['rf_scene_combat'] = 8
        if args.load_world:
            payload = args.load_world.read_bytes()
            report['existing_xbox_world'] = str(args.load_world.resolve())
        else:
            if args.kind == 'player-rocket':
                replay = b''.join(struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0, 0,
                    int(frame == 120), 0, int(frame == 30), 0)
                    for frame in range(save_frames))
            else:
                replay = bytes(save_frames * 48)
            (DISC / 'player-replay.bin').write_bytes(b'RFI6' + struct.pack('<I', 48) + replay)
            (DISC / 'world-hdd-save.flag').write_bytes(b'1')
            build(run, 'save')
            saved = run_guest(run, 'save', base, save_frames, args.seconds,
                              snapshot=True, extra_symbols=counters, capture_world=True)
            report['phases']['save'] = saved
            payload = (run / 'save/xbox-world.rfwc').read_bytes()
        count = projectile_count(payload, args.kind)
        report['saved_flights'] = count
        report['world_sha256'] = hashlib.sha256(payload).hexdigest()
        (DISC / 'world-hdd-save.flag').unlink(missing_ok=True)
        (DISC / 'world-fixture-load.flag').write_bytes(b'1')
        (DISC / 'world-fixture.0').write_bytes(fixture(payload))
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI6' + struct.pack('<I', 48) + bytes(args.load_frames * 48))
        build(run, 'load')
        loaded = run_guest(run, 'load', base, args.load_frames, args.seconds,
                           snapshot=True, extra_symbols=counters)
        report['phases']['load'] = loaded
        state = loaded['checkpoint_state']
        if state[8] != 1 or state[0] != 0 or state[1] != len(payload):
            raise RuntimeError('Xbox did not load the same ordinary checkpoint')
        values = loaded['extra'][key]
        launches, contacts, terminal, live = values[:4]
        refusals = values[6] if args.kind == 'player-rocket' else values[4]
        accounted = terminal + live + (contacts if args.kind != 'grenade' else 0)
        # The restored NPC may legitimately fire again during the continuation.
        # Every new launch adds a flight; the saved flights must remain accounted
        # for by the active/terminal counts at the bounded endpoint.
        if refusals or accounted < count + launches:
            raise RuntimeError(f'Saved flights disappeared during continuation: {loaded["extra"][key]}')
        report['result'] = 'PASS'
    finally:
        for name, data in original.items():
            if data is None:
                (DISC / name).unlink(missing_ok=True)
            else:
                (DISC / name).write_bytes(data)
        (DISC / 'scene-fixture.vpp').unlink(missing_ok=True)
        build(run, 'restore')
        report['disc_restored'] = all(
            ((DISC / name).read_bytes() if (DISC / name).exists() else None) == data
            for name, data in original.items()) and not (DISC / 'scene-fixture.vpp').exists()
        if not report['disc_restored']:
            report['result'] = 'FAIL'
        (run / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(run, report['result'], flush=True)
        if not report['disc_restored']:
            raise RuntimeError('Xbox test disc files were not restored')


if __name__ == '__main__':
    main()
