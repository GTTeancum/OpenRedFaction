"""Authored L1S1 movement crosses ambient audibility boundaries; no host input."""
import argparse,json,os,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser();parser.add_argument('--native',action='store_true');args=parser.parse_args()
folder=root/'artifacts/ambient-walk-check';folder.mkdir(exist_ok=True)
commands=b''.join(struct.pack('<5fI',0,0,front,0,0,0)*count for count,front in ((30,0),(360,1),(360,-1),(30,0)))
env=dict(os.environ)
for key in ('RF_REPLAY_REGION_START','RF_REPLAY_DOOR_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(key,None)
env.update(RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
rows=[]
for frames in (30,390,780):
    source=folder/f'inputs-{frames}.bin';source.write_bytes(commands[:frames*24])
    run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/f'frame-{frames}.ppm')],env=env,capture_output=True,text=True)
    (folder/f'pc-{frames}.txt').write_text(run.stdout+run.stderr);run.check_returncode()
    def row(name):return list(map(int,next(l for l in run.stdout.splitlines() if l.startswith(name+' ')).split()[1:]))
    audio=row('AMBIENT_AUDIO');bank=row('SOUND_BANK');body=row('PC_PLAY_BODY');spawn=row('PLAYER_SPAWN')
    assert audio[0]==frames and audio[4]==0 and sum(bank[2:])<=1024*1024
    rows.append(dict(frames=frames,audio=audio,bank=bank,position=list(struct.unpack('<3f',struct.pack('<3I',*body[22:25]))),spawn=list(struct.unpack('<3f',struct.pack('<3I',*spawn[1:4])))))
assert rows[-1]['audio']==[780,3,2,1483,0,116632,1,2828698778],rows
assert rows[1]['position']!=rows[0]['position'] and rows[2]['position']!=rows[1]['position']
assert rows[-1]['bank']==[88,7,228536,13128]
report=dict(result='PASS',checkpoints=rows,scope='Normal authored L1S1 spawn,30 neutral/360 forward/360 backward/30 neutral input frames. Player position changes; ambient starts/stops and bank bytes checked without device playback on PC. Final three starts/two stops, no failures. No teleport, camera override, complete level traversal or budget-pressure claim.')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True)
if args.native:subprocess.run([sys.executable,str(root/'tools/xemu_replay_check.py'),str(folder/'inputs-780.bin'),'--level','L1S1.rfl','--audio-capture','--seconds','180'],check=True)
