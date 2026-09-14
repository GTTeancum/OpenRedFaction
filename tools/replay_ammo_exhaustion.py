"""Run the live pistol dry through ordinary discrete trigger presses."""
import os,struct,subprocess,json
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/ammo-exhaustion';folder.mkdir(exist_ok=True)
frames=6000;source=folder/'input.bin'
source.write_bytes(b'RFI4'+struct.pack('<I',40)+b''.join(struct.pack('<5f5I',0,0,0,0,0,0,0,0,int(i>=30 and i%30==0),0) for i in range(frames)))
env=dict(os.environ,RF_REPLAY_ACTOR_UID='8431',RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
for k in ('RF_REPLAY_DAMAGE_UID','RF_REPLAY_DEATH_ANIMATION','RF_REPLAY_LIGHTMAP_REGEN','RF_REPLAY_DOOR_START','RF_REPLAY_REGION_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(k,None)
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'final.ppm')],env=env,capture_output=True,text=True)
(folder/'log.txt').write_text(run.stdout+run.stderr);run.check_returncode()
def words(key):return list(map(int,next(l for l in run.stdout.splitlines() if l.startswith(key+' ')).split()[1:]))
ammo=words('PLAYER_AMMO');combat=words('COMBAT');audio=words('WEAPON_AUDIO');enemy=words('ENEMY_COMBAT')
assert enemy[6]==0 and combat[0]==141 and combat[5:]==[0,0,0],combat
assert ammo[1:5]==[0,0,125,8] and ammo[5]>0 and ammo[7]==0,ammo
assert audio[:3]==[149]*3 and audio[7]==0,audio
report=dict(result='PASS',frames=frames,ammo=ammo,combat=combat,audio=audio,scope='141 rounds consumed,125 reserve transferred over8 reloads including a partial final magazine; later presses create no shots/reloads/audio. Initial supply is first-pass policy; pickups excluded.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
