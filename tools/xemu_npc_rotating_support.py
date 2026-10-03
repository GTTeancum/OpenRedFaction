"""Stock64MiB live NPC carry, natural support loss and landing on a tilting DEV platform.

Uses existing platform mode2/DEV NPC8. New telemetry is read-only; source seed
offset is explicit. No host input, images, PC gameplay or campaign traversal.
Parent owns serial builds and XEMU. --prepare-only creates only local assets.
"""
import argparse
import datetime
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import sys

from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu

FRAMES = 600
SYMBOLS = {'rf_scene_npc_rotating_fixture':256,
           'rf_scene_npc_rotating_support':8,'rf_scene_npc_support_lifecycle':8,
           'rf_scene_npc_carry_collision':8,'rf_scene_npc_idle_ground':6,
           'rf_scene_npc_mover_support':8,'rf_scene_fragment_platform_audit':32}


def prepare_fixture():
    subprocess.run([sys.executable,'-B','tools/build_fragment_platform_fixture.py','--npc-rotating-platform'],
                   cwd=ROOT,check=True)
    return ROOT/'artifacts/npc-rotating-platform/game/levelsm.vpp'


def floats(values):
    return list(struct.unpack('<'+'f'*len(values),struct.pack('<'+'I'*len(values),*values)))


def decode_rows(raw):
    if len(raw)!=256:raise RuntimeError('Unexpected rotating fixture telemetry layout')
    rows=[]
    for i in range(8):
        p=raw[i*32:(i+1)*32]
        rows.append(dict(valid=p[0],frame=p[1],actor=p[2],support=p[3],mode=p[4],material=p[5],
            position=floats(p[6:9]),velocity=floats(p[9:12]),inherited=floats(p[12:15]),
            normal=floats(p[15:18]),rotations=p[18],losses=p[19],static_contacts=p[20],
            moving_contacts=p[21],retries=p[22],landings=p[23],proposal=floats(p[24:27]),
            health=floats(p[27:28])[0],body_flags=p[28],mover=p[29],error=p[30],lifecycle_error=p[31]))
    return rows


def validate(guest):
    if guest['guest_phase']!=5 or guest['frames']!=FRAMES or guest['memory_bytes']!=64*1024*1024 or \
       guest['free_pages']<=0 or guest['player_life'][2]:
        raise RuntimeError('Incomplete stock64MiB rotating support run')
    x=guest['extra'];rows=decode_rows(x['rf_scene_npc_rotating_fixture'])
    for r in rows:
        values=[r['health'],*r['position'],*r['velocity'],*r['inherited'],*r['normal'],*r['proposal']]
        if not all(math.isfinite(v) for v in values):
            raise RuntimeError(f'Nonfinite NPC telemetry: {r}')
    if any(not r['valid'] or r['error'] or r['lifecycle_error'] or r['health']<=0 for r in rows):
        raise RuntimeError(f'Missing live phase or owner failure: {rows}')
    for name in ('rf_scene_npc_rotating_support','rf_scene_npc_support_lifecycle','rf_scene_npc_carry_collision'):
        if x[name][7]:raise RuntimeError(f'{name} failed: {x[name]}')
    base,first,second,mid,loss,fall,land,final=rows
    if [r['frame'] for r in rows[:4]]!=[419,420,421,435] or final['frame']!=598:
        raise RuntimeError('Deterministic observations did not execute at requested frames')
    if not base['support'] or base['support']!=base['mover'] or base['mode']==3 or \
       abs(base['position'][0]-8.699)>.03 or abs(base['position'][2]-2.5)>.03:
        raise RuntimeError(f'Off-center actor did not naturally land before rotation: {base}')
    if first['support']!=base['mover'] or second['support']!=base['mover'] or \
       first['rotations']!=base['rotations']+1 or second['rotations']!=first['rotations']+1:
        raise RuntimeError('Ordinary frame scheduler missed or duplicated the first rotations')
    # Independent rational first-step transform. Existing controller starts its
    # one-second quarter-turn at420. Ground correction can move Y; carry X/Z
    # must agree with the exact old/new host transform and cannot remain still.
    t=1/60;c=(1-t*t)/(1+t*t);s=2*t/(1+t*t)
    dx,dy=base['position'][0]-9.449,base['position'][1]-5.55
    expected=[9.449+dx*c+dy*s,5.55-dx*s+dy*c,base['position'][2]]
    if max(abs(first['proposal'][i]-expected[i]) for i in range(3))>.003 or \
       abs(first['position'][0]-expected[0])>.03 or abs(first['position'][0]-base['position'][0])<.015:
        raise RuntimeError(f'Live off-center rotation is wrong: {base}, {first}, expected{expected}')
    dx,dy=first['position'][0]-9.449,first['position'][1]-5.55
    contact_velocity=[(dx-(dx*c-dy*s))*60,(dy-(dx*s+dy*c))*60,0]
    if max(abs(a-b) for a,b in zip(first['inherited'],contact_velocity))>.005:
        raise RuntimeError(f'Ground acceptance lost rotational point velocity: {first}, expected{contact_velocity}')
    if loss['frame']<421 or loss['mode']!=3 or loss['support'] or loss['material']!=0xffffffff or \
       math.sqrt(sum(v*v for v in loss['inherited']))<.1:
        raise RuntimeError(f'Natural slope/support loss did not retain point velocity: {loss}')
    if fall['frame']!=loss['frame']+1 or fall['mode']!=3 or fall['support'] or \
       max(abs(a-b) for a,b in zip(loss['inherited'],fall['inherited']))>1e-5 or fall['velocity'][1]>=0:
        raise RuntimeError(f'Fall refresh lost or changed the launch contribution: {loss}, {fall}')
    if not fall['frame']<land['frame']<=final['frame'] or land['mode']==3 or land['support'] or \
       any(abs(v)>1e-5 for v in land['inherited']) or land['landings']<=base['landings']:
        raise RuntimeError(f'Ordinary static landing failed to clear inherited carry: {land}')
    if final['mode']==3 or final['support'] or any(abs(v)>1e-5 for v in final['inherited']):
        raise RuntimeError('Actor did not remain grounded after landing')
    if x['rf_scene_fragment_platform_audit'][1]!=60 or not x['rf_scene_npc_carry_collision'][1]:
        raise RuntimeError('Tilt interval or narrow own-mover retry was not exercised')
    return dict(result='PASS',rows=rows,first_step_expected=expected,
                collision_retry=x['rf_scene_npc_carry_collision'],free_pages=guest['free_pages'],
                limitations='Live idle actor carry/loss/landing only. No image verification, save/load, walking rider or simultaneous unrelated obstacle fixture.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',action='store_true')
    parser.add_argument('--validate-existing',type=Path,help='Existing run/result.json; no build or emulator')
    args=parser.parse_args()
    if args.prepare_only:print(prepare_fixture());return
    if args.validate_existing:
        print(json.dumps(validate(json.loads(args.validate_existing.read_text())),indent=2));return
    require_no_project_xemu(ROOT)
    hdd=ROOT/'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():raise RuntimeError('Missing isolated XEMU HDD base')
    fixture=prepare_fixture()
    folder=ROOT/'artifacts/xemu'/('npc-rotating-support-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{
        'fragment-platform-test.flag','fragment-platform.vpp','dev-npc.flag','dev-room.flag','player-control.flag'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report=dict(result='FAIL',scope=__doc__)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        shutil.copyfile(fixture,DISC/'fragment-platform.vpp')
        (DISC/'fragment-platform-test.flag').write_bytes(b'2');(DISC/'dev-npc.flag').write_bytes(b'8')
        (DISC/'dev-room.flag').write_bytes(b'');(DISC/'campaign-spawn.flag').write_bytes(b'')
        (DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-level.bin').write_bytes(b'fragment-platform.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(FRAMES*48))
        build(folder,'run')
        guest=run_guest(folder,'run',hdd,FRAMES,600,extra_symbols=SYMBOLS,allow_guest_error=True)
        report['guest']=guest;report.update(validate(guest))
    except Exception as exc:report['error']=str(exc);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        finally:
            report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==v for n,v in original.items())
            if not report['disc_restored']:report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__=='__main__':main()
