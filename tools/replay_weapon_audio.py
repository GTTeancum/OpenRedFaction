"""Weapon sound requests and captured software-mixer voices through normal input."""
import os,struct,subprocess,json,hashlib,wave
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/weapon-audio';folder.mkdir(exist_ok=True);rows=[]
for name,frames,requests in [('idle',180,0),('shot',180,1),('reload',180,2),('auto',660,19),('full_clip',180,0)]:
 source=folder/(name+'.bin');source.write_bytes(b'RFI4'+struct.pack('<I',40)+b''.join(struct.pack('<5f5I',0,0,0,0,0,0,0,0,int(name in ('shot','reload','auto') and (i==30 or (name=='auto' and i>=30 and i%30==0))),int(name in ('reload','full_clip') and i>=60)) for i in range(frames)))
 trace=folder/(name+'.trace');env=dict(os.environ,RF_REPLAY_ACTOR_UID='8431',RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp',RF_REPLAY_AUDIO_TRACE=str(trace))
 for key in ('RF_REPLAY_DAMAGE_UID','RF_REPLAY_DEATH_ANIMATION','RF_REPLAY_LIGHTMAP_REGEN','RF_REPLAY_DOOR_START','RF_REPLAY_REGION_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(key,None)
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=env,capture_output=True,text=True)
 (folder/(name+'.txt')).write_text(run.stdout+run.stderr);run.check_returncode()
 audio=list(map(int,next(l for l in run.stdout.splitlines() if l.startswith('WEAPON_AUDIO ')).split()[1:]));assert audio[:3]==[requests]*3 and audio[7]==0
 names=sorted(set(l.split(' ',7)[-1] for l in trace.read_text().splitlines() if l.startswith('V ')))
 pcm=Path(str(trace)+'.pcm').read_bytes();assert len(pcm)>0
 with wave.open(str(folder/(name+'.wav')),'wb') as f:f.setnchannels(2);f.setsampwidth(2);f.setframerate(48000);f.writeframes(pcm)
 rows.append(dict(case=name,audio=audio,voices=names,pcm_bytes=len(pcm),pcm_sha256=hashlib.sha256(pcm).hexdigest()));print(rows[-1],flush=True)
assert rows[0]['pcm_sha256']!=rows[1]['pcm_sha256']!=rows[2]['pcm_sha256']
assert rows[0]['pcm_sha256']==rows[4]['pcm_sha256']
assert any('reload' in n.lower() for n in rows[2]['voices'])
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows,scope='Pistol shot, manual held reload and automatic reload request counts, loaded PCM and named mixer voices; captured mixed PCM changes from idle. No physical speaker or Xbox device claim.'),indent=2))
