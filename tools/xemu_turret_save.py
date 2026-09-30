"""Bounded stock64MiB turret destruction and ordinary save/fresh-load check.

Uses the existing enemy-free CTF06/authored turret fixture. Source60 frames
stage ordinary contact damage; fresh load30 frames omit the damage fixture.
No images, host input, PC build or campaign playthrough. Parent runs serially.
"""
import argparse
import datetime
import json
import math
from pathlib import Path
import struct
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare
from xemu_turret_combat import UID, U, f, prepare_level, SYMBOLS as CONTACT_SYMBOLS

SAVE_FRAMES, LOAD_FRAMES = 60, 30
SYMBOLS = dict(CONTACT_SYMBOLS, rf_scene_turret_checkpoint=6,
               rf_scene_turret_combat=10, rf_scene_world_load_reject=3,
               rf_scene_npc_checkpoint_reject_state=6, rf_scene_turret_death_effects=8)


def dead_row(payload):
    if payload[:4] != b'RFWC' or len(payload) < 320:
        raise RuntimeError('Missing ordinary world checkpoint')
    kind, offset, size = struct.unpack_from('<III', payload, 128+10*12)
    if kind != 11 or offset+size > len(payload):
        raise RuntimeError('Invalid vehicle component')
    block = payload[offset:offset+size]
    if len(block) != 200 or block[:4] != b'RFTU' or struct.unpack_from('<III', block, 4) != (1, 0, 1):
        raise RuntimeError('Expected RFTU1: empty vehicle inner payload, one turret')
    row = block[16:]
    uid, cls = struct.unpack_from('<II', row)
    health, armor = struct.unpack_from('<ff', row, 8)
    dead, flags, flags810 = struct.unpack_from('<III', row, 16)
    action = struct.unpack_from('<i', row, 36)[0]
    target, remaining, burst = struct.unpack_from('<III', row, 40)
    if uid != UID or dead != 1 or not flags810 & 1 or not math.isfinite(health) or health > 0 or not math.isfinite(armor) or armor < 0:
        raise RuntimeError(f'Invalid dead turret vitals: {(uid, health, armor, dead, flags810)}')
    if action != 1 or (target, remaining, burst) != (0xffffffff, 0, 0):
        raise RuntimeError('Dead turret retained active combat state')
    return dict(uid=uid, class_index=cls, health=health, armor=armor, dead=dead,
                flags=flags, flags810=flags810, action=action, target_uid=target,
                remaining=remaining, burst=burst)


def validate_source(saved, payload):
    state = saved['checkpoint_state']; x = saved['extra']
    t = x['rf_scene_turret_test']; owners = x['rf_scene_turret_owners']
    draw = x['rf_scene_turret_draw']; combat = x['rf_scene_turret_combat']
    if saved['guest_phase'] != 5 or saved['frames'] != SAVE_FRAMES or saved['memory_bytes'] != 64*1024*1024 or saved['free_pages'] <= 0:
        raise RuntimeError('Missing bounded stock64MiB source run')
    if state[9] != 1 or state[3] or x['rf_scene_turret_checkpoint'] != [1, 0, 0, 0, 0, 0]:
        raise RuntimeError(f'Turret save failed: {state}, {x}')
    if t[:2] != [UID, 1] or not t[2] or t[2] != t[3] or t[3] != t[5] or t[4] or t[6] != 1 or t[20] or t[21] != 1:
        raise RuntimeError(f'Contact fixture failed: {t}')
    if owners[0] != 1 or owners[2] != t[5] or owners[3] != 1 or owners[7] or combat[3] or combat[9]:
        raise RuntimeError(f'Source owner/combat failed: {owners}, {combat}')
    if not draw[0] or not draw[1] or draw[2] or draw[3] != UID or t[11] == t[12] or t[13] != t[12]:
        raise RuntimeError(f'Source live/dead model selection failed: {draw}, {t}')
    effects=x["rf_scene_turret_death_effects"]
    if effects[0]!=1 or effects[1]<1 or effects[2]!=1 or any(effects[3:6]) or effects[6]!=UID or effects[7]:
        raise RuntimeError(f"Death presentation scheduling failed: {effects}")
    row = dead_row(payload)
    if row['health'] != f(t[8]) or row['armor'] != f(t[10]) or f(t[7]) <= 0:
        raise RuntimeError('Saved vitals differ from damaged owner')
    return row


def validate(saved, loaded, payload):
    row = validate_source(saved, payload)
    state = loaded['checkpoint_state']; x = loaded['extra']
    checkpoint = x['rf_scene_turret_checkpoint']; owners = x['rf_scene_turret_owners']
    draw = x['rf_scene_turret_draw']; combat = x['rf_scene_turret_combat']
    if loaded['guest_phase'] != 5 or loaded['frames'] != LOAD_FRAMES or loaded['memory_bytes'] != 64*1024*1024 or loaded['free_pages'] <= 0:
        raise RuntimeError('Missing bounded stock64MiB fresh load')
    if state[8] != 1 or state[0] or state[1] != saved['checkpoint_state'][4] or checkpoint != [0, 1, 1, 1, 0, 0]:
        raise RuntimeError(f'Dead turret restore failed: {state}, {checkpoint}')
    if any(x['rf_scene_turret_test']) or owners[0] != 1 or owners[2] or owners[3] or owners[7]:
        raise RuntimeError(f'Damage fixture/death replayed on load: {owners}')
    if draw[0] or not draw[1] or draw[2] or draw[3] != UID:
        raise RuntimeError(f'Fresh load did not exclusively submit the dead model: {draw}')
    if not combat[0] or combat[1] or combat[3] or combat[9]:
        raise RuntimeError(f'Dead turret acquired/fired after restore: {combat}')
    if any(x["rf_scene_turret_death_effects"]):
        raise RuntimeError(f"Death presentation replayed/unavailable on load: {x['rf_scene_turret_death_effects']}")
    if any(x['rf_scene_world_load_reject']):
        raise RuntimeError(f'World load admission failed: {x["rf_scene_world_load_reject"]}')
    return dict(result='PASS', saved_turret=row,
                source_hits=saved['extra']['rf_scene_turret_test'][5],
                source_model_submissions=saved['extra']['rf_scene_turret_draw'][:2],
                loaded_model_submissions=draw[:2], restored_count=checkpoint[2],
                restored_dead=checkpoint[3], loaded_shots=combat[3],
                free_pages=min(saved['free_pages'], loaded['free_pages']),
                limitations='Ordinary dead-owner restore and model submission only; appearance/audio, live aiming/cadence saves and NPC Attack-to-turret persistence remain unverified.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validate-existing', type=Path, help='Read recorded evidence only; no build or emulator')
    args = parser.parse_args()
    if args.validate_existing:
        folder = args.validate_existing
        if not json.loads((folder/'report.json').read_text()).get('disc_restored'):
            raise RuntimeError('Disc restoration not confirmed')
        result = validate(json.loads((folder/'save/result.json').read_text()),
                          json.loads((folder/'load/result.json').read_text()),
                          (folder/'save/xbox-world.rfwc').read_bytes())
        print(json.dumps(result, indent=2)); return
    require_no_project_xemu(ROOT)
    hdd = prepare(ROOT, ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT/'artifacts/xemu'/('turret-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    archive = prepare_level(folder/'level')
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | {'player-control.flag', 'scene-fixture.vpp', 'campaign-turret-test.bin'}
    original = {n: (DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report = dict(result='FAIL', scope=__doc__, phases={})
    try:
        for n in names: (DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(archive.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'')
        (DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-turret-test.bin').write_bytes(U(UID))
        (DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(SAVE_FRAMES*48))
        build(folder, 'save')
        saved = run_guest(folder, 'save', hdd, SAVE_FRAMES, 420, capture_world=True, extra_symbols=SYMBOLS)
        report['phases']['save'] = saved
        payload = (folder/'save/xbox-world.rfwc').read_bytes()
        report['saved_turret'] = validate_source(saved, payload)
        # Crucial: fresh boot must not rerun the fixture's initially-alive assertion.
        (DISC/'campaign-turret-test.bin').unlink()
        (DISC/'world-hdd-save.flag').unlink()
        (DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(LOAD_FRAMES*48))
        build(folder, 'load')
        loaded = run_guest(folder, 'load', hdd, LOAD_FRAMES, 420, snapshot=True, extra_symbols=SYMBOLS)
        report['phases']['load'] = loaded
        report.update(validate(saved, loaded, payload))
    finally:
        for n, data in original.items():
            if data is None: (DISC/n).unlink(missing_ok=True)
            else: (DISC/n).write_bytes(data)
        try: build(folder, 'restore')
        finally:
            report['disc_restored'] = all(((DISC/n).read_bytes() if (DISC/n).exists() else None) == data for n, data in original.items())
            if not report['disc_restored']: report['result'] = 'FAIL'
            (folder/'report.json').write_text(json.dumps(report, indent=2)+'\n')
            print(folder, report['result'], flush=True)
        if not report['disc_restored']: raise RuntimeError('Disc restoration failed')


if __name__ == '__main__': main()
