"""Bounded Xbox owned-corpse save/load check; no campaign playthrough or images."""
import datetime
import json
import struct
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SYMBOLS = {'rf_scene_live_corpses': 8, 'rf_scene_corpse_checkpoint': 8,
           'rf_scene_live_death_audio': 4, 'rf_scene_script_slays': 6,
           'rf_scene_npc_checkpoint_reject_state': 6,
           'rf_scene_world_load_reject': 3, 'rf_scene_profile_stage': 4,
           'rf_scene_setup_result': 4}


def dead_record(payload, uid):
    offset, length = struct.unpack_from('<II', payload, 128+12+4)
    blob = payload[offset:offset+length]
    if blob[:4] != b'RFNC' or struct.unpack_from('<I', blob, 4)[0] != 10:
        raise RuntimeError('Expected RFNC10 NPC component')
    count = struct.unpack_from('<I', blob, 16)[0]
    at, result = 64, None
    for _ in range(count):
        actor = struct.unpack_from('<I', blob, at)[0]
        animation = struct.unpack_from('<I', blob, at+540)[0]
        move, combat = struct.unpack_from('<II', blob, at+564)
        shots = struct.unpack_from('<I', blob, at+588)[0]
        if actor == uid:
            dead, flags, action = struct.unpack_from('<IIi', blob, at+552)
            result = {'dead': dead, 'flags': flags, 'action': action,
                      'animation_bytes': animation}
            if animation != 120:
                raise RuntimeError(f'Expected one settled death clip (120 bytes), got {animation}')
            pose_at = at+600+move+combat+24*shots
            result['active_clips'] = struct.unpack_from('<I', blob, pose_at+16)[0]
            result['frozen'] = struct.unpack_from('<I', blob, pose_at+32)[0]
            result['motion'], result['tick'] = struct.unpack_from('<ii', blob, pose_at+108)
            if result['active_clips'] != 1 or result['frozen'] != 1:
                raise RuntimeError(f'Death pose has not settled: {result}')
        at += 600+move+combat+24*shots+animation
    if at != len(blob) or result is None:
        raise RuntimeError('Malformed NPC component or missing corpse actor')
    if result['dead'] != 1 or not result['flags'] & 1 or not 5 <= result['action'] <= 16 or not result['animation_bytes']:
        raise RuntimeError(f'Missing saved corpse pose: {result}')
    return result


def main():
    require_no_project_xemu(ROOT)
    hdd = prepare(ROOT, ROOT / 'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT / 'artifacts/xemu' / ('corpse-save-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | {'player-control.flag'}
    original = {n: (DISC / n).read_bytes() if (DISC / n).exists() else None for n in names}
    report = {'result': 'FAIL', 'scope': 'L1S1 authored miner death and ordinary Xbox corpse save/load', 'phases': {}}
    try:
        for n in names:
            (DISC / n).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(b'levels1.vpp'.ljust(64, b'\0') + b'L1S1.rfl'.ljust(64, b'\0'))
        # Slay_Object is type1, allowed by the setup whitelist, with zero authored delay.
        (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<I', 9362))
        (DISC / 'campaign-actor.bin').write_bytes(struct.pack('<I', 8432))
        (DISC / 'world-hdd-save.flag').write_bytes(b'1')
        # tech01_death_leg_R.rfa spans ticks160..6560:80 simulation frames.
        # 180 allows death, the whole selected clip and ordinary settled save.
        (DISC / 'player-replay.bin').write_bytes(b'RFI6' + struct.pack('<I', 48) + bytes(180*48))
        build(folder, 'save')
        saved = run_guest(folder, 'save', hdd, 180, 360, capture_world=True, extra_symbols=SYMBOLS, allow_guest_error=True)
        report['phases']['save'] = saved
        corpses = saved['extra']['rf_scene_live_corpses']
        state = saved['checkpoint_state']
        if saved['extra']['rf_scene_setup_result'] != [1, 9362, 1, 0]:
            raise RuntimeError(f'Authored Slay setup rejected: {saved["extra"]["rf_scene_setup_result"]}')
        if state[9] != 1 or state[3] or corpses[0] != 1 or corpses[1] != 1 or any(corpses[5:]):
            raise RuntimeError(f'Authored corpse/save failed: {corpses}, {state}')
        report['saved_corpse'] = dead_record((folder / 'save/xbox-world.rfwc').read_bytes(), 8432)
        for n in ('campaign-setup.bin', 'world-hdd-save.flag'):
            (DISC / n).unlink()
        (DISC / 'world-hdd-load.flag').write_bytes(b'1')
        (DISC / 'player-replay.bin').write_bytes(b'RFI6' + struct.pack('<I', 48) + bytes(32*48))
        build(folder, 'load')
        loaded = run_guest(folder, 'load', hdd, 32, 360, snapshot=True, extra_symbols=SYMBOLS)
        report['phases']['load'] = loaded
        restored = loaded['extra']['rf_scene_corpse_checkpoint']
        corpses = loaded['extra']['rf_scene_live_corpses']
        audio = loaded['extra']['rf_scene_live_death_audio']
        state = loaded['checkpoint_state']
        if state[8] != 1 or state[0] or restored != [1, 1, 1, 0, 8432, 0, 1, 1]:
            raise RuntimeError(f'Corpse pose/owner did not survive load: {restored}, {state}')
        if corpses[0] != 1 or corpses[1] != 1 or not corpses[4] or any(corpses[5:]) or any(audio):
            raise RuntimeError(f'Loaded corpse failed updates or replayed death audio: {corpses}, {audio}')
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
