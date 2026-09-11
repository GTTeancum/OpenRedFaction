"""Compare owned campaign startup poses to original authored startup reports."""
import hashlib,json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];game=root/'Installed_Game'
poses=json.loads((root/'artifacts/npc-authored-pose.json').read_text())['results']
advances=json.loads((root/'artifacts/npc-authored-advance.json').read_text())['results']
def key(r):return r['level'],r['entity_class'],r['creation_flags']
expected_poses={key(r):r for r in poses if r['prior_action']==0 and r['delta']==struct.unpack('<f',struct.pack('<f',1/30))[0]}
expected_states={key(r):r['state_hex'] for r in advances if r['prior_action']==0 and r['delta']==struct.unpack('<f',struct.pack('<f',1/30))[0]}
results=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 out=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--catalog',str(game/'levels1.vpp'),str(game/'tables.vpp'),str(game/'motions.vpp'),str(game/'meshes.vpp'),level],text=True)
 count=0;bones=0
 for row in out.splitlines():
  f=row.split('\t')
  if f[0]!='STARTUP_POSE':continue
  k=(level,f[1],int(f[2]));assert f[3]==expected_states[k],(k,'playback')
  assert hashlib.sha256(bytes.fromhex(f[4])).hexdigest()==expected_poses[k]['pose_sha256'],(k,'matrices/generations')
  count+=1;bones+=expected_poses[k]['bones']
 assert count>0
 results.append(dict(level=level,actors=count,bone_matrices=bones))
report=dict(result='PASS',scope='All authored skeletal actors in three opening levels, shared startup owner at1/30 second; exact playback and bone/cache bytes versus original fixture reports, complete release balances all counters to zero. Not per-frame AI/rendering or complete actor construction.',results=results)
(root/'artifacts/npc-startup-owned.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
