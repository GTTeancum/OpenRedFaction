"""First-pass playable hitscan/reload smoke checks; process-local input only."""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/combat-first';folder.mkdir(exist_ok=True)
rows=[]
for name,frames,fire,reload in [('idle',180,False,False),('fire',360,True,False),('reload',180,True,True)]:
 payload=b'RFI4'+struct.pack('<I',40)+b''.join(struct.pack('<5f5I',0,0,0,0,0,0,0,0,int(fire and i>=30),int(reload and i==60)) for i in range(frames))
 source=folder/(name+'.bin');source.write_bytes(payload)
 env=dict(os.environ,RF_REPLAY_ACTOR_UID='8456',RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
 for key in ('RF_REPLAY_DAMAGE_UID','RF_REPLAY_DEATH_ANIMATION','RF_REPLAY_LIGHTMAP_REGEN','RF_REPLAY_DOOR_START','RF_REPLAY_REGION_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(key,None)
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=env,capture_output=True,text=True)
 (folder/(name+'.txt')).write_text(run.stdout+run.stderr);run.check_returncode()
 state=list(map(int,next(line for line in run.stdout.splitlines() if line.startswith('COMBAT ')).split()[1:]))
 assert f'Completed {frames} frames' in run.stdout and state[7]==0 and state[5]<=12 and state[6]<=72
 if name=='idle':assert state[:3]==[0,0,0] and state[5]==12
 if name=='fire':assert state[0]>12 and state[1]>0 and state[2]==1 and struct.unpack('<f',struct.pack('<I',state[4]))[0]<=0
 if name=='reload':assert 0<state[0]<12 and state[1]>0
 rows.append(dict(case=name,frames=frames,combat=state));print(rows[-1],flush=True)
report=dict(result='PASS',cases=rows,scope='Staged real L1S1 visible guard8456, manual/automatic reload, repeated trigger and retained damage/death on PC. Prototype tuning, finite clip/unlimited reserves. Static world rays and conservative moving-solid bounds; AABB actor hits. No enemy return fire, shot audio, first-person weapon presentation or full weapon inventory claim.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print('PASS',flush=True)
