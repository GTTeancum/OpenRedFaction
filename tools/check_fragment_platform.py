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
parser=argparse.ArgumentParser();parser.add_argument('--moving',action='store_true');args=parser.parse_args()
name='moving' if args.moving else 'stationary'
(OUT/(name+'-report.json')).unlink(missing_ok=True)
if args.moving:subprocess.run([sys.executable,'-B',str(Path(__file__).resolve())],cwd=ROOT,check=True)
subprocess.run([sys.executable,'-B','tools/build_fragment_platform_fixture.py'],cwd=ROOT,check=True)
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',
           RF_REPLAY_AUTHORED_SOURCE='108',RF_REPLAY_AUTHORED_SOURCES='3')
if args.moving:env['RF_REPLAY_FRAGMENT_PLATFORM_TEST']='1'
env['RF_REPLAY_FRAGMENT_SUPPORT_AUDIT']='1'
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
    assert report['platform'][:3]==[599,60,1],report['platform']
    assert len(report['platform'])==16
    bottom=struct.unpack('<2f',struct.pack('<2I',*report['platform'][6:8]))
    assert abs(bottom[0]-.65)<.005 and abs(bottom[1]+1.5)<.005
    assert 420<report['platform'][8]<=480 and report['platform'][9]==1
    xyz=struct.unpack('<3f',struct.pack('<3I',*report['platform'][3:6]))
    assert abs(xyz[0]-12.449)<.00001 and abs(xyz[1]-.55)<.00001 and xyz[2]==2.5
    stationary=json.loads((OUT/'stationary-report.json').read_text())
    assert abs(stationary['mesh_bottoms'][1]-.65)<.005,stationary
    assert all(abs(y+1.5)<.005 for y in report['mesh_bottoms']),report
    assert stationary['mesh_bottoms'][1]-report['mesh_bottoms'][1]>2.1
    assert report['motion'][3]==3 and stationary['motion'][3]==3
    report['scope']='Rocket-generated rubble rests on explicit platform; 60 kinematic mover steps withdraw support, one fragment wakes and all three settle on the floor. PC only; saves intentionally disabled.'
else:
    assert abs(report['mesh_bottoms'][1]-.65)<.005,report
    report['scope']='Stationary platform control: second rocket fragment rests at platform top0.65; remaining fragments rest on world floor.'
from PIL import Image
Image.open(OUT/(name+'.ppm')).save(OUT/(name+'.png'))
(OUT/(name+'-report.json')).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
