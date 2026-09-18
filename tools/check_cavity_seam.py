"""Live two-window floor crater, with explicit developer hardness fixture."""
import json,math,os,struct,subprocess
from pathlib import Path
from replay_authored_post import pitch_for,pitch_commands
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/cavity-seam';folder.mkdir(exist_ok=True)
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_AUTHORED_SOURCE='66',
           RF_REPLAY_CAVITY_SEAM_TEST='1',RF_REPLAY_PLAYER_CHECKPOINT='1',RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='240')
eye=[-24,2.879973+.785402536,8];target=[-29,-1.949,.5]
data=bytearray(b'RFI6'+struct.pack('<I',48)+bytes(400*48))
for f in (10,20,30,40):struct.pack_into('<I',data,8+48*f+40,1)
for offset,start,end in [(12,0,pitch_for(eye,target)),(16,-math.pi/2,math.atan2(target[0]-eye[0],target[2]-eye[2]))]:
    commands,_=pitch_commands(start,end,90)
    for i,value in enumerate(commands):struct.pack_into('<f',data,8+48*(100+i)+offset,value)
struct.pack_into('<I',data,8+48*240+32,1)
(folder/'shot.bin').write_bytes(data);(folder/'shot.rfcp').unlink(missing_ok=True)
env['RF_REPLAY_GEOMOD_CHECKPOINT_OUT']=str(folder/'shot.rfcp')
with (folder/'shot.log').open('w') as log:
    r=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(folder/'shot.bin'),str(folder/'shot.ppm')],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
assert r.returncode==0,r.returncode
lines=(folder/'shot.log').read_text().splitlines()
def values(label,kind=int):return list(map(kind,next(l for l in reversed(lines) if l.startswith(label+' ')).split()[1:]))
assert values('AUTHORED_SOURCE_CUTS')[:2]==[66,1]
assert values('PLAYER_LIFE')[0]==0
impact=values('ROCKET_IMPACT',float)
assert abs(impact[2]+29)<.03 and abs(impact[3]+2)<.001 and abs(impact[4]-.5)<.03,impact
assert not any('REJECT' in l for l in lines)
report=dict(impact=impact,publication=values('TERRAIN_PUBLICATION'),checkpoint_bytes=(folder/'shot.rfcp').stat().st_size)
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

# Same shot without this fixture must preserve the retail hardness100 floor.
env.pop('RF_REPLAY_CAVITY_SEAM_TEST')
env['RF_REPLAY_GEOMOD_CHECKPOINT_OUT']=str(folder/'control.rfcp')
(folder/'control.rfcp').unlink(missing_ok=True)
with (folder/'control.log').open('w') as log:
    r=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(folder/'shot.bin'),str(folder/'control.ppm')],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
assert r.returncode==0,r.returncode
lines=(folder/'control.log').read_text().splitlines()
assert values('GEOMOD')[1]==0
assert values('TERRAIN_PUBLICATION')[0]==0
report['fixture_off_preserves_floor']=True
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS fixture-off control: original floor remains indestructible')

# The altered hardness world must not restore under ordinary fixture settings.
env['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/'shot.rfcp')
env['RF_REPLAY_GEOMOD_CHECKPOINT_OUT']=str(folder/'mismatch.rfcp')
(folder/'mismatch.rfcp').unlink(missing_ok=True)
with (folder/'mismatch.log').open('w') as log:
    r=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(folder/'shot.bin'),str(folder/'mismatch.ppm')],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
assert r.returncode!=0 and not (folder/'mismatch.rfcp').exists(),'Mismatched fixture save must fail before writing output'
report['mismatched_save_rejected']=True
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS mismatched fixture checkpoint rejected')
