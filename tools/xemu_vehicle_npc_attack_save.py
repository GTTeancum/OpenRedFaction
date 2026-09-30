"""Check ordinary Xbox save/reload of an NPC Attack against a passive Fighter.

The source encounter is process-local; save/load uses the normal stock-64-MiB
world checkpoint and the private harness HDD. No desktop input or PC run.
"""

import datetime
import json
import struct

from xemu_native_world_save import FLAGS, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_vehicle_player_shot import ROOT, DISC, build
from xemu_world_hdd import prepare


SYMBOLS = {'rf_scene_vehicle_attack_probe': 8,
           'rf_scene_vehicle_attack_live': 8,
           'rf_scene_script_attack': 12,
           'rf_scene_npc_checkpoint_reject_state': 6,
           'rf_scene_passive_damage': 8}


def as_float(word):
    return struct.unpack('<f', struct.pack('<I', word))[0]


def main():
    require_no_project_xemu(ROOT)
    base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not base.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    hdd = prepare(ROOT, base)
    folder = ROOT / 'artifacts/xemu' / ('vehicle-npc-attack-save-' +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()}
    names |= {'campaign-vehicle-shot.bin'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'Xbox ordinary save/reload of passive Fighter Attack',
              'phases': {}}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels2.vpp'.ljust(64, b'\0') + b'L20S2.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-actor.bin').write_bytes(struct.pack('<I', 4717))
        (DISC / 'campaign-vehicle-shot.bin').write_bytes(struct.pack('<2I', 4801, 1))
        (DISC / 'world-hdd-save.flag').write_bytes(b'1')
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI5' + struct.pack('<I', 44) + bytes(110 * 44))
        build(folder, 'save')
        saved = run_guest(folder, 'save', hdd, 110, 360, capture_world=True,
                          extra_symbols=SYMBOLS, allow_guest_error=True)
        report['phases']['save'] = saved
        state = saved['checkpoint_state']
        live = saved['extra']['rf_scene_vehicle_attack_live']
        if saved['guest_phase'] & 0x80000000 or state[9] != 1 or state[3] or \
           state[4] < 320 or live[0] != 1 or live[1] != 4726 or live[2] != 4801:
            raise RuntimeError(f'Active Fighter Attack did not save: {state}, {live}, '
                               f'reject {saved["extra"]["rf_scene_npc_checkpoint_reject_state"]}')
        saved_health = as_float(live[7])
        report['saved_health'] = saved_health
        (DISC / 'campaign-vehicle-shot.bin').unlink()
        (DISC / 'world-hdd-save.flag').unlink()
        (DISC / 'world-hdd-load.flag').write_bytes(b'1')
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI5' + struct.pack('<I', 44) + bytes(20 * 44))
        build(folder, 'load')
        loaded = run_guest(folder, 'load', hdd, 20, 360, snapshot=True,
                           extra_symbols=SYMBOLS, allow_guest_error=True)
        report['phases']['load'] = loaded
        load_state = loaded['checkpoint_state']
        resumed = loaded['extra']['rf_scene_vehicle_attack_live']
        if loaded['guest_phase'] & 0x80000000 or load_state[8] != 1 or \
           load_state[0] or load_state[1] != state[4] or resumed[0] != 1 or \
           resumed[1] != live[1] or resumed[2] != live[2] or \
           as_float(resumed[7]) > saved_health:
            raise RuntimeError(f'Active Fighter Attack did not resume: {load_state}, {resumed}')
        report['result'] = 'PASS'
    finally:
        for name, data in original.items():
            if data is None:
                (DISC / name).unlink(missing_ok=True)
            else:
                (DISC / name).write_bytes(data)
        build(folder, 'restore')
        report['disc_restored'] = all(
            ((DISC / name).read_bytes() if (DISC / name).exists() else None) == data
            for name, data in original.items())
        if not report['disc_restored']:
            report['result'] = 'FAIL'
        (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(folder, report['result'], flush=True)
        if not report['disc_restored']:
            raise RuntimeError('Xbox test disc flags were not restored')


if __name__ == '__main__':
    main()
