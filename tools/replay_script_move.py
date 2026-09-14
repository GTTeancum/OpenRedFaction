"""Gryphon Goto delay, approach and obstruction checks through rendered PC scenes."""
import json,math,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/script-move';folder.mkdir(exist_ok=True)
raw=subprocess.check_output([str(root/'build/pc/Release/rf_level_entity_probe.exe'),str(root/'Installed_Game/levels2.vpp'),'L7S2.rfl'])
start=next(struct.unpack_from('<3f',raw,i+4) for i in range(0,len(raw),1084) if struct.unpack_from('<I',raw,i)[0]==4952)
results=[]
for frames in (59,180,600,601):
 source=folder/f'inputs-{frames}.bin';source.write_bytes(b'RFI5'+struct.pack('<I',44)+struct.pack('<5f6I',*([0]*11))*frames)
 env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')};env.update(RF_REPLAY_LEVEL='L7S2.rfl',RF_REPLAY_ARCHIVE='levels2.vpp',RF_REPLAY_GOTO_UID='4994')
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/f'final-{frames}.ppm')],env=env,capture_output=True,text=True)
 (folder/f'pc-{frames}.log').write_text(run.stdout+run.stderr);assert run.returncode==0,run.stderr[-1000:]
 def words(label):return list(map(int,next(x for x in run.stdout.splitlines() if x.startswith(label+' ')).split()[1:]))
 state,actor=words('SCRIPT_MOVE'),words('SCRIPT_ACTOR');assert state[7]==0,state
 if frames==59:assert state==[0]*8 and actor==[0]*8,(state,actor)
 else:
  assert actor[0]==4952 and state[0]==1 and state[5:7]==[4952,4994],(state,actor)
  position=struct.unpack('<3f',struct.pack('<3I',*actor[1:4]));assert position[1]==start[1]
  if frames==180:assert state[1]==120 and state[3]==0 and abs(math.dist(start,position)-3)<.001,(state,position)
  else:assert state[1]==355 and state[3]==frames-415,(state,position)
 results.append(dict(frames=frames,state=state,actor=actor))
assert results[-1]['actor']==results[-2]['actor'],'Blocked frame must preserve position'
report=dict(result='PASS',scope='Explicit authored Goto dispatch, delayed horizontal approach and static geometry stop; routing, support/vertical motion and locomotion animation remain open.',start=start,cases=results)
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
