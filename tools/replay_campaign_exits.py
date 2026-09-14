"""Dispatch authored exits inside a running PC process and verify reload loops."""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/campaign-exits';folder.mkdir(exist_ok=True)
source=folder/'idle.bin';source.write_bytes(b'RFI5'+struct.pack('<I',44)+bytes(44*120))
rows=[]
for name,level,uid,target in [('forward','L1S1.rfl',9019,'L1S2.rfl'),('backward','L1S2.rfl',9346,'L1S1.rfl'),('current','L1S1.rfl',9018,None),('carried','L1S1.rfl',9019,'L1S2.rfl')]:
 env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
 env.update(RF_REPLAY_LEVEL=level,RF_REPLAY_ARCHIVE='levels1.vpp',RF_REPLAY_EXIT_UID=str(uid),RF_REPLAY_PLAYER_STATE_OUT=str(folder/(name+'.state')))
 if name=='carried':
  carried=bytearray((folder/'current.state').read_bytes());carried[8]=1
  struct.pack_into('<i',carried,192+8*4,39);struct.pack_into('<ffI',carried,448,37.5,12.25,8)
  (folder/'carried-input.state').write_bytes(carried);env['RF_REPLAY_PLAYER_STATE_IN']=str(folder/'carried-input.state')
 result=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=env,capture_output=True,text=True)
 (folder/(name+'.log')).write_text(result.stdout+result.stderr);result.check_returncode()
 transitions=[line.split()[1:] for line in result.stdout.splitlines() if line.startswith('LEVEL_TRANSITION ')]
 assert transitions==([[level,target,str(uid),'61']] if target else []),transitions
 assert 'Completed 120 frames' in result.stdout
 state=(folder/(name+'.state')).read_bytes();assert len(state)==464
 if name=='carried':assert state==carried,'automatic exit changed player state'
 rows.append(dict(case=name,transitions=transitions,health=struct.unpack_from('<f',state,448)[0]));print(rows[-1],flush=True)
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows),indent=2))
