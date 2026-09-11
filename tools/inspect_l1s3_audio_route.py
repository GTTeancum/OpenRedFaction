"""Record L1S3 traversal observations; not a geometry-correctness test."""
import argparse,hashlib,json,os,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'artifacts/l1s3-route-observation';out.mkdir(exist_ok=True)
parser=argparse.ArgumentParser()
parser.add_argument('--native',action='store_true',help='Compare the 930-frame descent with stock64MiB XEMU; parity does not establish route correctness')
args=parser.parse_args()
env=dict(os.environ)
for key in ('RF_REPLAY_REGION_START','RF_REPLAY_DOOR_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(key,None)
env.update(RF_REPLAY_LEVEL='L1S3.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
rows=[]
for frames in (30,570,930,1530):
    source=out/f'inputs-{frames}.bin';source.write_bytes(bytes(30*24)+struct.pack('<5fI',1,0,1,0,0,0)*(frames-30))
    r=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(out/f'frame-{frames}.ppm')],env=env,capture_output=True,text=True)
    (out/f'pc-{frames}.txt').write_text(r.stdout+r.stderr);r.check_returncode()
    def row(name):return list(map(int,next(l for l in r.stdout.splitlines() if l.startswith(name+' ')).split()[1:]))
    body=row('PC_PLAY_BODY');rows.append(dict(frames=frames,position=list(struct.unpack('<3f',struct.pack('<3I',*body[22:25]))),ambient=row('AMBIENT_AUDIO'),bank=row('SOUND_BANK')))
report=dict(status='OBSERVED',pc_sha256=hashlib.sha256((root/'build/pc/Release/rf_pc_play.exe').read_bytes()).hexdigest(),checkpoints=rows,scope='Authored spawn with diagonal process-local input. Records current movement and audio residency; no assertion that this follows the intended route or that descending is a collision bug. No native or eviction claim. Resolve level path/traversal before using this as an audio-pressure route.')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if args.native:
    subprocess.run([sys.executable,str(root/'tools/xemu_replay_check.py'),str(out/'inputs-930.bin'),'--level','L1S3.rfl','--audio-capture','--seconds','300'],check=True)
