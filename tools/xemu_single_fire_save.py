"""Check ordinary Xbox save/reload of an authored queued NPC shot.

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


SYMBOLS = {'rf_scene_script_fire_no_anim':6,'rf_scene_script_shoot_once':6,
           'rf_scene_single_fire_restored':4,'rf_scene_single_fire_trace':49,
           'rf_scene_enemy_fire':6,'rf_scene_enemy_combat':8,
           'rf_scene_npc_checkpoint_reject_state':6,'rf_scene_world_load_reject':3,
           'rf_scene_world_snapshot_event_probe':6}


def saved_shot(payload, uid):
    offset,length=struct.unpack_from('<II',payload,128+12+4)
    blob=payload[offset:offset+length]
    if blob[:4]!=b'RFNC' or struct.unpack_from('<I',blob,4)[0]!=10:
        raise RuntimeError('Expected RFNC10 NPC component')
    count=struct.unpack_from('<I',blob,16)[0];at=64;result=None
    for _ in range(count):
        actor=struct.unpack_from('<I',blob,at)[0]
        animation=struct.unpack_from('<I',blob,at+540)[0]
        move,combat=struct.unpack_from('<II',blob,at+564)
        shots,rng=struct.unpack_from('<II',blob,at+588)
        if actor==uid:
            if shots!=1:raise RuntimeError(f'Expected one saved shot, got {shots}')
            request=struct.unpack_from('<III3f',blob,at+600+move+combat)
            result={'event':request[0],'remaining':request[1],'mode':request[2],'point':request[3:],'rng':rng}
        at+=600+move+combat+24*shots+animation
    if at!=len(blob) or result is None:raise RuntimeError('Malformed NPC component or absent actor')
    return result


def main():
    require_no_project_xemu(ROOT)
    base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not base.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    hdd = prepare(ROOT, base)
    folder = ROOT / 'artifacts/xemu' / ('single-fire-save-' +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()}
    names |= {'player-control.flag'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'Xbox ordinary save/reload of a hidden NPC pending authored shot',
              'phases': {}}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels2.vpp'.ljust(64, b'\0') + b'L8S4.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<I',8478))
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'world-hdd-save.flag').write_bytes(b'1')
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI5' + struct.pack('<I', 44) + bytes(50 * 44))
        build(folder, 'save')
        saved = run_guest(folder, 'save', hdd, 50, 360, capture_world=True,
                          extra_symbols=SYMBOLS, allow_guest_error=True)
        report['phases']['save'] = saved
        state = saved['checkpoint_state']
        live=saved['extra']['rf_scene_script_fire_no_anim']
        if saved['guest_phase'] & 0x80000000 or state[9]!=1 or state[3] or live[:2]!=[1,0] or live[5]!=1:
            raise RuntimeError(f'Pending shot did not save: {state}, {saved["extra"]}')
        shot=saved_shot((folder/'save/xbox-world.rfwc').read_bytes(),6810)
        report['saved_shot']=shot
        if shot['event']!=8478 or shot['mode']!=2 or not 0<shot['remaining']<=120:
            raise RuntimeError(f'Bad pending shot: {shot}')
        (DISC/'campaign-setup.bin').write_bytes(struct.pack('<2I',6825,6825))
        (DISC / 'world-hdd-save.flag').unlink()
        (DISC / 'world-hdd-load.flag').write_bytes(b'1')
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI5' + struct.pack('<I', 44) + bytes(110 * 44))
        build(folder, 'load')
        loaded = run_guest(folder, 'load', hdd, 110, 360, snapshot=True,
                           extra_symbols=SYMBOLS, allow_guest_error=True)
        report['phases']['load'] = loaded
        load_state = loaded['checkpoint_state']
        resumed=loaded['extra']['rf_scene_single_fire_restored']
        fired=loaded['extra']['rf_scene_script_fire_no_anim']
        trace=loaded['extra']['rf_scene_single_fire_trace']
        aim=2166136261
        for byte in struct.pack('<3f',*shot['point']):aim=((aim^byte)*16777619)&0xffffffff
        if loaded['guest_phase']&0x80000000 or load_state[8]!=1 or load_state[0] or load_state[1]!=state[4]:
            raise RuntimeError(f'Pending shot load failed: {load_state}, {loaded["extra"]}')
        if resumed!=[1,8478,shot['remaining'],2] or fired[0]!=0 or fired[1]!=1 or fired[5]!=0 or trace[:4]!=[1,2,8478,aim]:
            raise RuntimeError(f'Pending shot not resumed exactly once: {resumed}, {fired}, {trace[:4]}')
        if loaded['extra']['rf_scene_enemy_fire'][1]!=0 or loaded['extra']['rf_scene_enemy_combat'][2]!=1:
            raise RuntimeError('No-animation fire behavior changed after load')
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
