"""Jump forward off the authored moving L1S2 lift onto static geometry."""
import hashlib,json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/lift-dismount';folder.mkdir(exist_ok=True)
source=folder/'inputs.bin'
source.write_bytes(b'RFI3'+struct.pack('<I',32)+b''.join(
    struct.pack('<5f3I',0,0,int(90<=i<150),0,0,0,i==90,i==30) for i in range(210)))
env=dict(os.environ)
for key in ('RF_REPLAY_REGION_START','RF_REPLAY_DOOR_START'):env.pop(key,None)
env.update(RF_REPLAY_LEVEL='L1S2.rfl',RF_REPLAY_ARCHIVE='levels1.vpp',RF_REPLAY_LIFT_START='1')
exe=root/'build/pc/Release/rf_pc_play.exe'
run=subprocess.run([str(exe),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True,check=True)
(folder/'pc.txt').write_text(run.stdout+run.stderr)
def row(name):return list(map(int,next(line for line in run.stdout.splitlines() if line.startswith(name+' ')).split()[1:]))
real=lambda word:struct.unpack('<f',struct.pack('<I',word))[0]
body=row('PC_PLAY_BODY');position=list(map(real,body[22:25]));raw=row('PLAYER_JUMP_FRAMES')
ring={raw[i]:raw[i:i+8] for i in range(0,len(raw),8)}
assert row('PLAYER_JUMP')==[1,1,1,90]
assert ring[90][4]==3 and ring[120][4]==3
landing=next(i for i in range(91,210) if ring[i][4]==1)
assert all(ring[i][4]==1 for i in range(landing,210))
assert position[2]-(-44.49818420410156)>4,position
assert not(body[68]&0x400000), 'Still marked as moving support'
# BODY_CONTACT retains the last body-sweep hit, not the current ground query.
# Grounded mode plus support flag cleared identifies the static-support path.
assert row('LIVE_MOTION')[7]==row('BODY_SWEEPS')[3]==0
report=dict(result='PASS',frames=210,landing_frame=landing,position=position,horizontal_travel=position[2]+44.49818420410156,
    pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),input_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    scope='Staged moving lift: use30, jump90, forward90..149. Airborne mode then static landing and cleared moving-support flag. No authored route, all dismount directions, per-frame support identity or original full-arc equivalence.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
