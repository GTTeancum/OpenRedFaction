"""Replay an ordinary rocket with the explicit developer platform fixture."""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'artifacts/fragment-platform'
parser=argparse.ArgumentParser();parser.add_argument('--moving',action='store_true')
mode=parser.add_mutually_exclusive_group();mode.add_argument('--tipping',action='store_true');mode.add_argument('--lifting',action='store_true')
args=parser.parse_args()
if args.tipping or args.lifting:args.moving=True
name='lifting' if args.lifting else 'tipping' if args.tipping else 'moving' if args.moving else 'stationary'
(OUT/(name+'-report.json')).unlink(missing_ok=True)
if args.moving:subprocess.run([sys.executable,'-B',str(Path(__file__).resolve())],cwd=ROOT,check=True)
subprocess.run([sys.executable,'-B','tools/build_fragment_platform_fixture.py'],cwd=ROOT,check=True)
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',
           RF_REPLAY_AUTHORED_SOURCE='108',RF_REPLAY_AUTHORED_SOURCES='3')
if args.moving:env['RF_REPLAY_FRAGMENT_PLATFORM_TEST']='3' if args.lifting else '2' if args.tipping else '1'
env['RF_REPLAY_FRAGMENT_SUPPORT_AUDIT']='1'
if args.tipping or args.lifting:env['RF_REPLAY_FRAGMENT_CONTACT_TRACE']='1'
with (OUT/(name+'.log')).open('w') as log:
    result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(OUT/'game'),
                           str(ROOT/'artifacts/side-group108/shot.bin'),str(OUT/(name+'.ppm'))],
                          cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
assert result.returncode==0,result.returncode
lines=(OUT/(name+'.log')).read_text().splitlines()
def row(name,kind=int):return list(map(kind,next(l for l in reversed(lines) if l.startswith(name+' ')).split()[1:]))
assert row('CAMPAIGN_MOVERS')[0]==1
assert row('AUTHORED_SOURCE_CUTS')[:6]==[108,1,103,1,99,0]
assert row('DETACHED_MOTION')[6]==0
report=dict(movers=row('CAMPAIGN_MOVERS'),motion=row('DETACHED_MOTION'),pose=row('DETACHED_POSE',float),
            cuts=row('AUTHORED_SOURCE_CUTS'),scope='Static platform with ordinary rocket; visual inspection and moving support acceptance are separate.')
support=[list(map(float,l.split()[1:])) for l in lines if l.startswith('DETACHED_SUPPORT_AUDIT ')]
assert len(support)==3
report['mesh_bottoms']=[r[8] for r in support]
if args.moving:
    report['platform']=row('FRAGMENT_PLATFORM')
    assert report['platform'][:2]==[599,60],report['platform']
    assert report['platform'][2]==1, 'Repeated sleep/wake while platform moves'
    assert len(report['platform'])==30
    bottom=struct.unpack('<2f',struct.pack('<2I',*report['platform'][6:8]))
    assert abs(bottom[0]-.65)<.005
    if args.lifting:
        support_y=struct.unpack('<f',struct.pack('<I',report['platform'][28]))[0]
        gap=struct.unpack('<f',struct.pack('<I',report['platform'][29]))[0]
        assert abs(gap)<=.01, ('Finite support gap',gap)
        report['support_gap']=gap
        assert report['platform'][27]==1 and abs(support_y-1.15)<.005, ('No finite platform support',report['platform'])
    else:assert abs(bottom[1]+1.5)<.005
    assert 420<report['platform'][8]<=480 and report['platform'][9]==1
    basis=struct.unpack('<9f',struct.pack('<9I',*report['platform'][16:25]))
    assert basis==((0,-1,0,1,0,0,0,0,1) if args.tipping else (1,0,0,0,1,0,0,0,1))
    xyz=struct.unpack('<3f',struct.pack('<3I',*report['platform'][3:6]))
    assert abs(xyz[0]-(9.449 if args.tipping or args.lifting else 12.449))<.00001 and abs(xyz[1]-(1.05 if args.lifting else .55))<.00001 and xyz[2]==2.5
    stationary=json.loads((OUT/'stationary-report.json').read_text())
    assert abs(stationary['mesh_bottoms'][1]-.65)<.005,stationary
    expected_bottoms=[-1.5,1.15 if args.lifting else -1.5,-1.5]
    assert all(abs(y-want)<.005 for i,(y,want) in enumerate(zip(report['mesh_bottoms'],expected_bottoms)) if not(args.lifting and i==1)),report
    if not args.lifting:assert stationary['mesh_bottoms'][1]-report['mesh_bottoms'][1]>2.1
    assert report['motion'][3]==3 and stationary['motion'][3]==3
    if args.tipping or args.lifting:
        steps=[l.split() for l in lines if l.startswith('DETACHED_STEP_TRACE ')]
        steps=[w for w in steps if w[2:5]==['0','0','1'] and 420<=int(w[1])<=480]
        assert len(steps)==61, 'Missing per-frame platform evidence'
        limited=[int(w[1]) for w in steps if int(w[7])]
        assert not limited, ('Platform motion exhausted collision substeps',limited)
        report['platform_motion']={'sampled_frames':len(steps),'limited_frames':limited,
                                  'max_substeps':max(int(w[5]) for w in steps)}
    report['mode']='lift' if args.lifting else 'tip' if args.tipping else 'translate'
    report['scope']='Rocket-generated rubble rests on explicit platform; 60 kinematic mover steps test support withdrawal or lifting, and fragments settle at the expected support heights. PC only; saves intentionally disabled.'
else:
    assert abs(report['mesh_bottoms'][1]-.65)<.005,report
    report['scope']='Stationary platform control: second rocket fragment rests at platform top0.65; remaining fragments rest on world floor.'
from PIL import Image
Image.open(OUT/(name+'.ppm')).save(OUT/(name+'.png'))
(OUT/(name+'-report.json')).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
