"""Contained opening dialogue audio: control, single message, replacement."""
import json, os, struct, subprocess, hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/message-audio';folder.mkdir(exist_ok=True)
source=folder/'input.bin';source.write_bytes(b'RFI5'+struct.pack('<I',44)+bytes(240*44))
cases=[]
for name,setup in [('control',None),('single','8356'),('repeat','8356,8356')]:
 env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
 trace=folder/(name+'.trace');env.update(RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp',RF_REPLAY_AUDIO_TRACE=str(trace))
 if setup:env['RF_REPLAY_SETUP_UID']=setup
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=env,capture_output=True,text=True)
 (folder/(name+'.log')).write_text(run.stdout+run.stderr);assert run.returncode==0,run.stderr[-1000:]
 state=list(map(int,next(x for x in run.stdout.splitlines() if x.startswith('MESSAGE_AUDIO ')).split()[1:]))
 expected=0 if not setup else 2 if ',' in setup else 1
 # One natural startup message also fires during this240-frame opening replay.
 assert state==[expected+1,expected+1,0,0],state
 pcm=Path(str(trace)+'.pcm').read_bytes();assert pcm
 cases.append(dict(name=name,state=state,bytes=len(pcm),sha256=hashlib.sha256(pcm).hexdigest()))
assert len({c['sha256'] for c in cases})==3,cases
report=dict(result='PASS',scope='PC software mixer: authored message starts and replacement, PCM differs from control. Device output and native Xbox audio are not verified.',cases=cases)
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
