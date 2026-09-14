"""Controlled authored Attack9802 replay; never injects host input or bypasses its delay."""
import hashlib,json,math,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'artifacts/attack-encounter/verified';out.mkdir(parents=True,exist_ok=True)
exe=root/'build/pc/Release/rf_pc_play.exe'
runs=[]
for frames in (840,1000,3000,4000):
    source=out/f'inputs-{frames}.bin';source.write_bytes(b'RFI3'+struct.pack('<I',32)+bytes(frames*32))
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ACTOR_UID='8324',RF_REPLAY_GOTO_UID='9802')
    run=subprocess.run([str(exe),'--spawn-replay',str(root/'Installed_Game'),str(source),str(out/f'final-{frames}.ppm')],env=env,capture_output=True,text=True)
    (out/f'pc-{frames}.log').write_text(run.stdout+run.stderr)
    if run.returncode:raise RuntimeError(f'{frames}: exit {run.returncode}; see log')
    def row(name,kind=int):return list(map(kind,next(x for x in run.stdout.splitlines() if x.startswith(name+' ')).split()[1:]))
    runs.append(dict(frames=frames,attack=row('SCRIPT_ATTACK'),positions=row('SCRIPT_ATTACK_POSITION',float),movement=row('SCRIPT_MOVE'),routes=row('SCRIPT_ROUTES'),enemy=row('ENEMY_COMBAT'),aim=row('ENEMY_AIM'),death=row('COMBAT_DEATH'),input_sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
assert runs[0]['attack'][0]==0,'Attack fired before authored delay'
for run in runs[1:]:
    assert run['attack'][1:3]==[9802,8324] and run['attack'][4]==1
    assert run['movement'][1]>0 and run['attack'][9]>0
    run['attacker_displacement']=math.dist(run['positions'][:3],run['positions'][3:6])
    assert run['attacker_displacement']>.1,'No actual attacker movement'
last=runs[-1]
engaged=last['attack'][5]>0 and last['attack'][8]!=last['attack'][7]
health=struct.unpack('<f',struct.pack('<I',last['attack'][8]))[0]
defeated=engaged and health<=0
report=dict(result='TARGET_DEFEATED' if defeated else 'ENGAGEMENT_OBSERVED' if engaged else 'PURSUIT_VERIFIED_ENCOUNTER_INCOMPLETE',exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),runs=runs,scope='Process-local event9802 request at frame30; real13.75s authored delay and target link retained. Camera/player staged beside actor8324. Not natural trigger traversal, retail-equivalence proof, or Xbox encounter validation.')
(out/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
assert defeated and last['death'][0]>0,'Scripted attack failed to defeat target and enter death presentation; inspect report'

assert last['aim'][0]>0,'No stationary aiming updates observed'
