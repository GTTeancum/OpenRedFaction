"""Xbox authored AI mode publication and ordinary save/reload consistency."""
import datetime
import json
import struct
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SYMBOLS = {'rf_scene_ai_mode_publication': 4, 'rf_scene_ai_mode_restored': 4,
           'rf_scene_world_load_reject': 3, 'rf_scene_enemy_combat': 8,
           'rf_scene_setup_result': 4, 'rf_scene_profile_stage': 4}


def main():
    require_no_project_xemu(ROOT)
    hdd = prepare(ROOT, ROOT / 'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT / 'artifacts/xemu' / ('ai-mode-save-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | {'player-control.flag'}
    original = {n: (DISC / n).read_bytes() if (DISC / n).exists() else None for n in names}
    report = {'result': 'FAIL', 'scope': 'L8S4 Set_AI_Mode10327 and ordinary Xbox save/load', 'phases': {}}
    try:
        for n in names:
            (DISC / n).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(b'levels2.vpp'.ljust(64, b'\0') + b'L8S4.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<I', 10327))
        (DISC / 'world-hdd-save.flag').write_bytes(b'1')
        (DISC / 'player-replay.bin').write_bytes(b'RFI6' + struct.pack('<I', 48) + bytes(64*48))
        build(folder, 'save')
        saved = run_guest(folder, 'save', hdd, 64, 360, capture_world=True, extra_symbols=SYMBOLS, allow_guest_error=True)
        report['phases']['save'] = saved
        mode = saved['extra']['rf_scene_ai_mode_publication']
        state = saved['checkpoint_state']
        if state[9] != 1 or state[3] or mode[1] < 6 or mode[2:] != [1, 1]:
            raise RuntimeError(f'Authored catatonic state/save failed: {mode}, {state}')
        for n in ('campaign-setup.bin', 'world-hdd-save.flag'):
            (DISC / n).unlink()
        (DISC / 'world-hdd-load.flag').write_bytes(b'1')
        (DISC / 'player-replay.bin').write_bytes(b'RFI6' + struct.pack('<I', 48) + bytes(32*48))
        build(folder, 'load')
        loaded = run_guest(folder, 'load', hdd, 32, 360, snapshot=True, extra_symbols=SYMBOLS)
        report['phases']['load'] = loaded
        mode = loaded['extra']['rf_scene_ai_mode_restored']
        state = loaded['checkpoint_state']
        if state[8] != 1 or state[0] or mode[0] < 6 or mode[1] < 6 or mode[2:] != [0, 0]:
            raise RuntimeError(f'AI scheduler/registered mode did not survive load: {mode}, {state}')
        report['result'] = 'PASS'
    finally:
        for n, data in original.items():
            if data is None:
                (DISC / n).unlink(missing_ok=True)
            else:
                (DISC / n).write_bytes(data)
        build(folder, 'restore')
        report['disc_restored'] = all(((DISC / n).read_bytes() if (DISC / n).exists() else None) == data for n, data in original.items())
        (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(folder, report['result'], flush=True)
        assert report['disc_restored']


if __name__ == '__main__':
    main()
