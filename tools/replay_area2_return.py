"""Naturally return from L2S3 and verify the completed Area2 combat state.
Run replay_area2_exit.py first. No forced exit or placement is used.
"""
import hashlib,json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/area2-return-replay';folder.mkdir(parents=True,exist_ok=True)
prefix=(root/'artifacts/area2-exit-replay/input.bin').read_bytes()
assert prefix[:8]==b'RFI6'+struct.pack('<I',48) and len(prefix)==8+7450*48
records=bytearray(prefix)
for i in range(7450,8000):
    x,z=(.716,.698) if i<7570 or 7760<=i<7830 else ((.698,-.716) if 7590<=i<7690 or 7710<=i<7745 else (0,0))
    records.extend(struct.pack('<5f7I',x,0,z,0,0,int(i<7710),0,1,0,0,0,0))
source=folder/'input.bin';source.write_bytes(records)
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
(folder/'run.log').write_text(run.stdout+run.stderr);run.check_returncode()
def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
transitions=[l.split()[1:] for l in run.stdout.splitlines() if l.startswith('LEVEL_TRANSITION ')]
assert transitions==[['L2S2a.rfl','L2S3.rfl','5150','7275'],['L2S3.rfl','L2S2a.rfl','5151','7688']]
assert 'Completed 8000 frames' in run.stdout and words('PLAYER_LIFE')[0]==0
assert words('NPC_COMBAT_FRAME')==[7999]
actors={int(l.split()[1]) for l in run.stdout.splitlines() if l.startswith('NPC_COMBAT_ROW ')}
assert actors and actors.isdisjoint({8490,5677,5676,5678,8071,5683,5458})
assert words('ACTOR_RETIREMENT')[1]==7
assert 'TAKEN_PICKUP l2s2a.rfl 8553' in run.stdout
ammo=words('PLAYER_AMMO');assert ammo[:3]==[3,88,16]
health=struct.unpack('<f',struct.pack('<I',words('ENEMY_COMBAT')[5]))[0];prefix_result=json.loads((root/'artifacts/area2-exit-replay/report.json').read_text());assert prefix_result['result']=='PASS' and prefix_result['frames']==7450
assert health==prefix_result['health'] and 0<health<100
report=dict(result='PASS',frames=8000,transitions=transitions,health=health,ammo=ammo,retired=7,prefix_sha256=hashlib.sha256(prefix).hexdigest(),scope='Natural return preserves six killed guards, dead miner, consumed kit and player vitals/ammo; not complete world-state persistence coverage.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
