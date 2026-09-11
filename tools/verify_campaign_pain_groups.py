"""Retained campaign class bindings versus independently inventoried pain labels.

Run verify_pain_groups.py and verify_npc_support_probe.py first.
"""
import json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/pain-groups/report.json').read_text())
assert inventory['result']=='PASS'
lookup={r['name'].lower():r['groups'] for r in inventory['rows']};results=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 data=subprocess.check_output([str(root/'build/pc/Release/rf_level_entity_probe.exe'),str(root/'Installed_Game/levels1.vpp'),level])
 assert len(data)%1084==0
 names=list(dict.fromkeys(data[i+52:i+308].split(b'\0')[0].decode('ascii').lower() for i in range(0,len(data),1084)))
 bindings=[lookup[name] for name in names];value=2166136261
 for row in bindings:
  for byte in struct.pack('<2I',*row):value=((value^byte)*16777619)&0xffffffff
 text=(root/'artifacts/npc-support-probe'/(level+'.txt')).read_text()
 rows={line.split()[0]:list(map(int,line.split()[1:])) for line in text.splitlines() if line.startswith(('NPC_PAIN_GROUPS ','FOLEY '))}
 assert rows['NPC_PAIN_GROUPS']==[len(names),len(names)*8,value],(level,rows)
 assert rows['FOLEY'][3]==26452+len(names)*48
 results.append(dict(level=level,classes=len(names),bytes=len(names)*8,hash=value,bindings=dict(zip(names,bindings))))
report=dict(result='PASS',levels=results,scope='Retained PC campaign group hashes in first-authored-class order match the independently extracted label inventory checked against original434cb0. Three level startups; no live sound dispatch or allocation-failure injection.')
(root/'artifacts/campaign-pain-groups.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
