"""Two post cuts, ordinary crouch/use/alt reset, and optional unselected-source recut."""
import json, math, os, struct, subprocess
from pathlib import Path
from replay_authored_post import pitch_commands
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'artifacts/paired-reset';folder.mkdir(parents=True,exist_ok=True)
data=bytearray((ROOT/'artifacts/paired-two-shot/inputs.bin').read_bytes())
assert data[:8]==b'RFI6'+struct.pack('<I',48) and len(data)==8+850*48
for frame in range(730,770):struct.pack_into('<I',data,8+48*frame+20,1)
for offset in (28,44):struct.pack_into('<I',data,8+48*750+offset,1)
eye=json.loads((ROOT/'artifacts/authored-post-live/post-recipe.json').read_text())['eye']
yaw0=-math.pi/2;yaw1=math.atan2(-4.699-eye[0],-2.5-eye[2]);middle=(yaw0+yaw1)/2
recut=bytearray(data)+bytes(300*48)
for frame,start,end in [(850,middle,yaw1),(1030,yaw1,middle)]:
    commands,_=pitch_commands(start,end,60)
    for i,c in enumerate(commands):struct.pack_into('<f',recut,8+48*(frame+i)+16,c)
struct.pack_into('<I',recut,8+48*940+32,1)
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_AUTHORED_SOURCES='2')
report={}
for name,payload,last in [('reset',data,[94,0,93,0,0,0,0,0]),('recut',recut,[94,0,93,1,0,0,0,0])]:
    path=folder/(name+'.bin');path.write_bytes(payload)
    with (folder/(name+'.log')).open('wb') as log:
        result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path),str(folder/(name+'.ppm'))],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
    assert result.returncode==0,name
    lines=(folder/(name+'.log')).read_text().splitlines()
    cuts=[list(map(int,l.split()[1:])) for l in lines if l.startswith('AUTHORED_SOURCE_CUTS ')]
    resets=[list(map(int,l.split()[1:])) for l in lines if l.startswith('AUTHORED_RESET ')]
    assert cuts[:3]==[[94,1,93,0,0,0,0,0],[94,1,93,1,0,0,0,0],[94,0,93,0,0,0,0,0]],cuts
    assert cuts[-1]==last and resets==[[0,4294967295,0,0,3]],(cuts,resets)
    def values(label):return list(map(int,next(l for l in lines if l.startswith(label+' ')).split()[1:]))
    publication,pieces,geomod=map(values,('TERRAIN_PUBLICATION','DETACHED_PIECES','GEOMOD'))
    expected=0 if name=='reset' else 1
    assert publication[2:4]==[expected,3+expected] and pieces[1:3]==[expected,expected],(publication,pieces)
    assert geomod[4]<=13*1024*1024 and not geomod[5],geomod
    report[name]=dict(source_cuts=cuts,reset=resets,publication=publication,pieces=pieces,geomod=geomod)
report['scope']='Safe floor reset restores both posts and clears both registries; later shot cuts unselected93 while94 remains intact. Unsafe paired reset and save/restore remain separate acceptance.'
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS paired reset and unselected-source recut')
