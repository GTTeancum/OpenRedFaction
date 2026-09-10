"""Registered controller ownership and ordered key mapping across authored levels."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];results=[]
for level in json.loads((root/'artifacts/moving-groups.json').read_text())['results']:
 groups=[g for g in level['records'] if g['keys']]
 output=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--registered-groups',str(root/'Installed_Game'/level['archive']),level['file']],text=True).splitlines()
 count,keys,budget=map(int,output[0].split());assert count==len(groups) and keys==sum(len(g['keys']) for g in groups)
 assert budget==32+24*count+8*keys
 expected=[]
 for i,g in enumerate(groups):expected.append(f"OBJECT {g['keys'][0]['uid']} {((i+2)<<16)|(i+1)} {0x6000001}")
 for i,g in enumerate(groups):
  for key in g['keys']:expected.append(f"KEY {key['uid']} {((i+2)<<16)|(i+1)}")
 assert output[1:]==expected,level['file']
 results.append(dict(level=level['file'],controllers=count,keys=keys,bytes=budget))
report=dict(result='PASS',levels=len(results),controllers=sum(r['controllers'] for r in results),keys=sum(r['keys'] for r in results),peak_bytes=max(r['bytes'] for r in results),scope='Owned runtime controllers registered in authored order after a sentinel object; first key UID identifies controller and every key maps to its owner. Exact budget and one-byte-short rejection, unchanged registry on preflight failure, repeated close, stale-handle removal and unrelated-object survival. PC integration, not original whole-world registration order or activation.',results=results)
(root/'artifacts/group-registration-verification.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='results'})
