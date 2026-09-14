"""Live authored pistol rules through recorded process-local input."""
import os,struct,subprocess,json
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/pistol-rules';folder.mkdir(exist_ok=True);rows=[]
cases=[('held',180,lambda i:i>=30,lambda i:False,1,15,0),
       ('rapid',150,lambda i:i>=30 and i%10==0,lambda i:False,4,12,0),
       ('reload_before',126,lambda i:i==30,lambda i:i>=60,1,15,1),
       ('reload_done',127,lambda i:i==30,lambda i:i>=60,1,16,0),
       ('reload_held_fire',160,lambda i:i==30 or i>=90,lambda i:i==60,1,16,0),
       ('auto',660,lambda i:i>=30 and i%30==0,lambda i:False,18,14,0)]
for name,frames,fire,reload,shots,ammo,timer in cases:
 source=folder/(name+'.bin');source.write_bytes(b'RFI4'+struct.pack('<I',40)+b''.join(struct.pack('<5f5I',0,0,0,0,0,0,0,0,int(fire(i)),int(reload(i))) for i in range(frames)))
 env=dict(os.environ,RF_REPLAY_ACTOR_UID='8431',RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
 for k in ('RF_REPLAY_DAMAGE_UID','RF_REPLAY_DEATH_ANIMATION','RF_REPLAY_LIGHTMAP_REGEN','RF_REPLAY_DOOR_START','RF_REPLAY_REGION_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(k,None)
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=env,capture_output=True,text=True)
 (folder/(name+'.txt')).write_text(run.stdout+run.stderr);run.check_returncode()
 def words(key):return list(map(int,next(l for l in run.stdout.splitlines() if l.startswith(key+' ')).split()[1:]))
 rules=words('PISTOL_RULES');combat=words('COMBAT')
 assert rules==[16,66,30,1109393408,1,24,1],rules
 assert combat[0]==shots and combat[5:]==[ammo,timer,0],combat
 rows.append(dict(name=name,rules=rules,combat=combat));print(rows[-1],flush=True)
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows,scope='Table-driven16-round pistol,30-tick primary cooldown,66-tick reload and semi-auto edges including held trigger during reload.40SP base damage enters shared damage pipeline; finite reserve/alternate fire excluded.'),indent=2))
