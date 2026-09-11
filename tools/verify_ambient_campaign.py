"""Actual campaign registration versus authored order and archive presence."""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/ambient-campaign';folder.mkdir(exist_ok=True)
source=folder/'inputs.bin';source.write_bytes(b'RFI3'+struct.pack('<I',32)+bytes(64))
inventory=json.loads((root/'artifacts/inventory.json').read_text())
available={e['name'].lower() for a in inventory['files'] if a['path']=='audio.vpp' for e in a['vpp']['entries']}
globals=json.loads((root/'artifacts/sound-table-inventory.json').read_text())['rows']
levels=json.loads((root/'artifacts/ambient-records.json').read_text())['results']
env=dict(os.environ)
for key in ('RF_REPLAY_REGION_START','RF_REPLAY_DOOR_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(key,None)
results=[]
for archive,name in [('levels1.vpp','L1S1.rfl'),('levels1.vpp','L1S3.rfl'),('levels1.vpp','L2S1.rfl'),('levelsm.vpp','ctf01.rfl')]:
 records=next(l['records'] for l in levels if l['file']==name and l['archive']==archive)
 indices={row['name'].lower():row['index'] for row in globals};states=bytearray();rejected=0
 for r in records:
  key=r['name'].lower()
  if key not in available:rejected+=1;continue
  if key not in indices:indices[key]=len(indices)
  states.extend(struct.pack('<Iii6fIi',r['uid'],indices[key],-1,*r['position'],r['near_distance'],r['volume'],r['rolloff'],r['flags'],-1))
 h=2166136261
 for byte in states:h=((h^byte)*16777619)&0xffffffff
 expected=[len(records)-rejected,rejected,16+44*len(records),h]
 env.update(RF_REPLAY_LEVEL=name,RF_REPLAY_ARCHIVE=archive)
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=env,capture_output=True,text=True)
 (folder/(name+'.txt')).write_text(run.stdout+run.stderr);run.check_returncode()
 def words(label):return list(map(int,next(s for s in run.stdout.splitlines() if s.startswith(label+' ')).split()[1:]))
 actual=words('AMBIENT_INSTANCES');audio=words('LIVE_AUDIO');bank=words('SOUND_BANK')
 assert actual==expected,(name,actual,expected)
 assert audio[3]==0 and bank[2]+bank[3]==audio[1]<=1024*1024,(name,audio,bank)
 if name=='L1S1.rfl':assert bank[1:3]==[4,111904] and audio[0]==100,bank
 results.append(dict(level=name,instances=actual,sound_bank=bank,registered_samples=audio[0]))
report=dict(result='PASS',results=results,scope='Actual PC campaign metadata registration in authored order. Independent sample indices/state hash, omitted missing resources and retained1MiB bank budget. L1S1 controller PCM residency unchanged. No ambient voice scheduling or playback.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
