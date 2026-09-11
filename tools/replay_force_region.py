"""Explicit authored L1S2 turbulence fixture, no host input or synthetic force."""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/force-replay';folder.mkdir(exist_ok=True)
source=folder/'inputs.bin';source.write_bytes(b'RFI3'+struct.pack('<I',32)+bytes(120*32))
env=dict(os.environ)
for key in ('RF_REPLAY_REGION_START','RF_REPLAY_DOOR_START','RF_REPLAY_LIFT_START'):env.pop(key,None)
env.update(RF_REPLAY_LEVEL='L1S2.rfl',RF_REPLAY_ARCHIVE='levels1.vpp',RF_REPLAY_FORCE_UID='3705')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'final.ppm')],env=env,capture_output=True,text=True,check=True)
(folder/'pc.txt').write_text(run.stdout)
def words(label):return list(map(int,next(l for l in run.stdout.splitlines() if l.startswith(label+' ')).split()[1:]))
ticks=words('FORCE_TICKS');assert ticks[0]==119 and ticks[2]>0 and ticks[3]>0 and ticks[5]>0 and ticks[6]>0 and ticks[11]==0,ticks
report=dict(result='PASS',frames=120,level='L1S2.rfl',staged_uid=3705,force_ticks=ticks,
 scope='Explicit process-local start inside authored region3705, no command input. Nonzero ordinary force/turbulence/shake activation through campaign with shared RNG; no original full-trajectory parity, replacement-force or native Xbox claim.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
