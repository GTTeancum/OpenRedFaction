"""One bounded stock64MiB APC moving exit and on-foot continuation.

Copies installed L1S3 APC9627 into empty CTF06, retaining all non-transform
entity bytes. Process-local replay boards30, drives45..140, turns100..140,
exits140 and walks150..174. No campaign route, screenshots or host input.
"""
import argparse
import datetime
import hashlib
import io
import json
import math
from pathlib import Path
import struct

from build_fragment_platform_fixture import read_entry,U,F
from check_ai_projectile_ordinary import archive
from inspect_levels import inspect
from xemu_turret_combat import entity_rows,f
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

HOST,FRAMES,EXIT_FRAME=9627,180,140
IDENTITY_DISK=(0,0,1,1,0,0,0,1,0)
SYMBOLS={'rf_scene_apc_exit_transition':48,'rf_scene_apc_exit_continuation':11,
         'rf_scene_vehicle_state':16,'rf_scene_vehicle_damage':8,
         'rf_scene_apc_primary':8,'rf_scene_setup_result':4,
         'rf_scene_vehicle_route_state':8}


def replay():
    rows=[]
    for frame in range(FRAMES):
        drive=45<=frame<=EXIT_FRAME
        turn=.4 if 100<=frame<=EXIT_FRAME else 0
        walk=1.0 if 150<=frame<175 else 0
        rows.append(struct.pack('<5f7I',turn if drive else walk,0,float(drive),0,0,
                                0,0,int(frame in (30,EXIT_FRAME)),0,0,0,0))
    return b'RFI6'+U(48)+b''.join(rows)


def prepare_level(folder):
    source=read_entry(ROOT/'Installed_Game/levels1.vpp','L1S3.rfl')
    owner=next(r for r in entity_rows(source) if r['uid']==HOST)
    if owner['name']!='APC':raise RuntimeError('Authored APC identity changed')
    original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    if entity_rows(original):raise RuntimeError('CTF06 is not an empty entity testbed')
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    start=next(s for s in meta['sections'] if s['type']=='0x70000')
    spawn=struct.unpack_from('<3f',original,start['offset']+8)
    position=(spawn[0],spawn[1]+.8,spawn[2]+6)
    raw=bytearray(owner['raw']);at=owner['transform'];raw[at:at+48]=F(*position,*IDENTITY_DISK)
    if raw[:at]!=owner['raw'][:at] or raw[at+48:]!=owner['raw'][at+48:]:
        raise RuntimeError('Changed non-transform APC bytes')
    replacements={0x30000:U(1)+raw,0x600:U(0),0x60000:U(0),
                  0x70000:F(*spawn,*IDENTITY_DISK)}
    out=bytearray(original[:meta['sections'][0]['offset']]);offsets={};added=0
    present={int(s['type'],16) for s in meta['sections']}
    for section in meta['sections']:
        kind=int(section['type'],16)
        if kind==0:
            for missing in sorted(replacements.keys()-present):
                payload=replacements[missing];offsets[missing]=len(out);out+=U(missing,len(payload))+payload;added+=1
        payload=replacements.get(kind,original[section['offset']+8:section['offset']+8+section['size']])
        offsets[kind]=len(out);out+=U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    struct.pack_into('<I',out,20,meta['declared_sections']+added)
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='L1S3.rfl'))
    checked=entity_rows(out)
    if len(checked)!=1 or checked[0]['uid']!=HOST or checked[0]['raw']!=raw:
        raise RuntimeError('APC fixture independent entity round trip failed')
    folder.mkdir(parents=True,exist_ok=True);path=folder/'scene-fixture.vpp';archive(path,[('L1S3.rfl',out)])
    recipe=dict(source='levels1.vpp/L1S3.rfl',geometry='levelsm.vpp/ctf06.rfl',entry_alias='L1S3.rfl',
                host_uid=HOST,source_offset=owner['offset'],source_record_sha256=hashlib.sha256(owner['raw']).hexdigest(),
                spawn=spawn,host_position=position,frames=FRAMES,
                staged_fields=['APC transform','player start orientation','empty events/triggers'],
                schedule={'board':30,'drive':[45,EXIT_FRAME],'turn':[100,EXIT_FRAME],'exit':EXIT_FRAME,'walk':[150,174]},
                limits='Functional moving exit timing; no campaign traversal, image, save/load or death-ejection claim')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay());return path,recipe


def validate(result):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete stock64MiB functional run')
    x=result['extra'];p=x['rf_scene_apc_exit_transition'];tail=x['rf_scene_apc_exit_continuation'];v=x['rf_scene_vehicle_state']
    if p[0]<2 or p[1:6]!=[1,1,0,EXIT_FRAME,0] or p[6]==p[7] or 0xffffffff in p[6:8]:
        raise RuntimeError(f'Missing one ordinary moving Use exit: {p[:14]}')
    if p[8:14]!=[0xffffffff,0xffffffff,0xffffffff,0,1,0] or p[45:47]!=[1,1]:
        raise RuntimeError('Exit failed ownership release or final-pose full-body hull exclusion')
    values=[f(word) for word in p[14:45]]
    if not all(math.isfinite(value) for value in values):raise RuntimeError('Nonfinite exit motion/pose')
    step=math.sqrt(sum((f(p[17+i])-f(p[14+i]))**2 for i in range(3)))
    speed=math.hypot(f(p[20]),f(p[22]))
    if step<=1e-5 or speed<=.1 or abs(f(p[44])-step)>1e-5:
        raise RuntimeError(f'APC was not moving during exit: step={step}, speed={speed}')
    if v[1:4]!=[1,1,0] or v[5] or v[12]!=p[6]:raise RuntimeError('Runtime did not publish one board/release')
    if tail[0]<FRAMES-2 or tail[1:4]!=[0,0xffffffff,0xffffffff] or f(tail[7])<=0:
        raise RuntimeError('Player failed to continue alive on foot with released control')
    movement=math.hypot(f(tail[4])-f(p[23]),f(tail[6])-f(p[25]))
    if not math.isfinite(movement) or movement<=.05:raise RuntimeError('Post-exit on-foot movement did not occur')
    if any(x['rf_scene_setup_result']) or x['rf_scene_vehicle_route_state'][3] or x['rf_scene_vehicle_route_state'][7]:
        raise RuntimeError('Unexpected authored route/setup activity')
    if x['rf_scene_apc_primary'][1] or x['rf_scene_vehicle_damage'][3] or not x['rf_scene_vehicle_damage'][7]:
        raise RuntimeError('Unexpected weapon launch or host destruction')
    return dict(result='PASS',exit_frame=EXIT_FRAME,exit_speed=speed,exit_step_distance=step,
                final_pose_hull_exclusion=True,on_foot_distance=movement,
                limitations='One bounded living moving-APC exit; no saved continuation, crowded dynamic exits, wreck/death exit or visuals.')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('apc-moving-exit-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes());(DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'L1S3.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'');(DISC/'player-replay.bin').write_bytes(replay())
        build(folder,'exit');result=run_guest(folder,'exit',hdd,FRAMES,420,snapshot=True,extra_symbols=SYMBOLS,allow_guest_error=True)
        report['native']=result;report.update(validate(result))
    except Exception as exc:report['error']=str(exc);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        except Exception as exc:report['result']='FAIL';report['restore_build_error']=str(exc);raise
        finally:
            report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']:report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Fixture disc restoration failed')


if __name__=='__main__':main()
