"""Registered mover memberships preserve existing authored attachment results."""
import json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];results=[]
for level in json.loads((root/'artifacts/moving-groups.json').read_text())['results']:
 def probe(mode):return subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),mode,str(root/'Installed_Game'/level['archive']),level['file']])
 legacy=bytearray(probe('--member-groups'));actual=probe('--registered-member-groups')
 count,movers=struct.unpack_from('<2I',legacy)
 def convert(at):
  old,=struct.unpack_from('<I',legacy,at)
  if old==0xffffffff:return
  assert old>>16 in (0x1234,0x2345)
  slot=old&0xffff;struct.pack_into('<I',legacy,at,((slot+1)<<16)|slot)
 for i in range(movers):convert(16+20*i+8);convert(16+20*i+12)
 at=16+20*movers;links=0
 for i in range(count):
  n,=struct.unpack_from('<I',legacy,at);at+=8
  for j in range(n):convert(at);at+=4
  links+=n
 assert at==len(legacy) and legacy==actual,level['file']
 results.append(dict(file=level['file'],movers=movers,controllers=count,links=links))
report=dict(result='PASS',levels=len(results),movers=sum(r['movers'] for r in results),controllers=sum(r['controllers'] for r in results),links=sum(r['links'] for r in results),scope='PC registered handles replace explicit fixture handles without changing authored memberships, flags or rotation signs; same retained inputs/budget checks. Registration order is movers then controllers, not original full-level construction.',results=results)
(root/'artifacts/registered-memberships-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
