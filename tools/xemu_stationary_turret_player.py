"""Bounded Xbox authored turret Use/aim/fire/exit, using process-local replay.

No fake turret, health writes, campaign route, host input, images or AI-disable
flag. One L2S2a Stationary Turret faces away from the player in empty CTF06.
Tests control ownership and actual shot dispatch, not damage to an added enemy.
"""
import argparse
import datetime
import json
import math
from pathlib import Path
import struct
from build_fragment_platform_fixture import read_entry, U, F
from xemu_turret_combat import UID, entity_rows, prepare_level, f
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

FRAMES = 120
SYMBOLS = {'rf_scene_turret_player': 12, 'rf_scene_turret_player_probe': 19,
           'rf_scene_turret_combat': 10, 'rf_scene_turret_shots': 8,
           'rf_scene_turret_owners': 8, 'rf_scene_turret_test': 22,
           'rf_scene_combat': 8, 'rf_scene_player_vitals': 6,
           'rf_scene_pickup_vitals': 4}


def prepare_control(folder):
    path = prepare_level(folder)
    recipe = json.loads((folder/'recipe.json').read_text())
    level = bytearray(read_entry(path, 'ctf06.rfl'))
    row, = entity_rows(level)
    spawn = recipe['spawn']
    # Spawn is an eye, while Use measures from the settled physical body.
    # Lower the fixture origin so its two-metre horizontal offset is within
    # the authored 2.5m body-to-origin radius after gravity settles.
    position = (spawn[0], spawn[1]-.75, spawn[2]-2)
    offset = row['offset']+row['transform']
    # Disk order: position, forward, right, up. The player is behind the
    # emplacement; its ordinary forward acquisition cannot see the player.
    level[offset:offset+48] = F(*position, 0,0,-1, -1,0,0, 0,1,0)
    copied, = entity_rows(level)
    at = row['transform']
    if copied['uid'] != UID or copied['raw'][:at] != row['raw'][:at] or copied['raw'][at+48:] != row['raw'][at+48:]:
        raise ValueError('Control fixture changed authored non-transform bytes')
    archive = bytearray(path.read_bytes())
    archive[4096:4096+len(level)] = level
    path.write_bytes(archive)
    recipe.update(position=position, staged_fields=['transform'],
                  scope=__doc__, runtime_fixture=False,
                  use_frames=[30,100], fire_frames=[45,70], probe_frame=80)
    (folder/'recipe.json').write_text(json.dumps(recipe, indent=2)+'\n')
    return path


def replay():
    data = bytearray(b'RFI6'+U(48))
    for frame in range(FRAMES):
        # Movement while mounted must not translate the physical player.
        data += struct.pack('<5f7I', 0,0,float(40 <= frame <= 60),
                            0,.25 if 35 <= frame <= 50 else 0,
                            0,0,int(frame in (30,100)),int(45 <= frame <= 70),0,0,0)
    return data


def live_probe(monitor, mapping):
    return {name: words(monitor, address(mapping,name), count)
            for name,count in SYMBOLS.items()}


def validate(result):
    if result['guest_phase'] != 5 or result['frames'] != FRAMES or result['memory_bytes'] != 64*1024*1024:
        raise RuntimeError('Incomplete bounded stock64MiB control run')
    live, final = result['probe'], result['extra']
    mount, end = live['rf_scene_turret_player'], final['rf_scene_turret_player']
    pose, last = live['rf_scene_turret_player_probe'], final['rf_scene_turret_player_probe']
    if mount[:4] != [1,0,1,UID] or not mount[4] or not mount[5] or mount[7]:
        raise RuntimeError(f'Use did not establish working player control: {mount}')
    if end[:4] != [1,1,0,UID] or end[5] != mount[5] or end[7] or end[11]:
        raise RuntimeError(f'Use exit or post-exit shot accounting failed: {end}')
    if pose[0:2] != [1,UID] or pose[4:6] != [1,1] or pose[6] != mount[9] or mount[8] == mount[9] or 0xffffffff in mount[8:10]:
        raise RuntimeError(f'Player/turret ownership links missing: {pose}, {mount}')
    if last[:2] != [0,UID] or last[4:7] != [0,0,0xffffffff]:
        raise RuntimeError(f'Exit retained owner or player link: {last}')
    for sample in (pose,last):
        values = [f(word) for word in sample[8:14]+sample[16:19]]
        if not all(math.isfinite(value) for value in values):
            raise RuntimeError('Nonfinite body/camera position')
        # Gravity remains active; the body can finish falling after entry.
        # Replay requests horizontal movement, so test those actual axes.
        if max(abs(f(sample[8+i])-f(sample[16+i])) for i in (0,2)) > .05:
            raise RuntimeError('Voluntary movement or exit displaced the body horizontally')
        if f(sample[2]) != 200 or f(sample[3]) != 100:
            raise RuntimeError('Unexpected turret damage or player self-damage')
    if sum((f(pose[11+i])-f(pose[8+i]))**2 for i in range(3)) < 1:
        raise RuntimeError('Camera did not move to the turret eye')
    for extra in (live,final):
        combat, shots = extra['rf_scene_turret_combat'], extra['rf_scene_turret_shots']
        if not combat[2] or combat[3] or combat[9] or shots[0] != extra['rf_scene_turret_player'][5] or shots[7]:
            raise RuntimeError(f'Control aim/fire or autonomous suppression failed: {combat}, {shots}')
        if extra['rf_scene_combat'][0] or any(extra['rf_scene_turret_test']):
            raise RuntimeError('Handheld shots or synthetic turret damage fixture ran')
        owners = extra['rf_scene_turret_owners']
        if owners[0] != 1 or owners[2] or owners[3] or owners[7]:
            raise RuntimeError(f'Turret owner damaged or invalid: {owners}')
    if result['free_pages'] <= 0:
        raise RuntimeError('No free memory')
    return dict(result='PASS', entries=end[0], exits=end[1], shots=end[5],
                turns=final['rf_scene_turret_combat'][2], free_pages=result['free_pages'],
                limitations='Control ownership, static-room movement suppression, eye camera and real shot dispatch only. No target/source-attribution runtime, pushed-body, save, audible/visual output or seat-animation claim.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path,help='Prepare local assets/replay only; no build/emulator')
    args = parser.parse_args()
    if args.prepare_only:
        path = prepare_control(args.prepare_only)
        (args.prepare_only/'player-replay.bin').write_bytes(replay())
        print(path)
        return
    require_no_project_xemu(ROOT)
    hdd = prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT/'artifacts/xemu'/('stationary-turret-player-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    archive = prepare_control(folder/'level')
    names = set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original = {n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report = {'result':'FAIL','scope':__doc__}
    try:
        for name in names:(DISC/name).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(archive.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'')
        (DISC/'player-control.flag').write_bytes(b'')
        (DISC/'player-replay.bin').write_bytes(replay())
        build(folder,'control')
        result = run_guest(folder,'control',hdd,FRAMES,360,snapshot=True,
                           extra_symbols=SYMBOLS,probe=live_probe,probe_frame=80)
        report['native'] = result
        report.update(validate(result))
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        for name,data in original.items():
            if data is None:(DISC/name).unlink(missing_ok=True)
            else:(DISC/name).write_bytes(data)
        try:build(folder,'restore')
        finally:
            report['disc_restored'] = all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']:report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
            print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__ == '__main__':main()
