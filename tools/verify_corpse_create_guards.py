"""Check constructor preflight and partial-owner exposure on PC/NXDK."""
import json,runpy,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];base=runpy.run_path(str(root/'tools/verify_corpse_create.py'));w=base['w']
standard=[0,0,0,0,0,0,1,0,0,2,0x23450080,0x34560080,0xffffffff,0,0,0,0,0,0,3,0,1,2]
rows=[]
for guard in range(1,6):
 v=standard.copy()
 if guard==2:v[12]=1
 rows.append((guard,v))
for j in (20,21,22):
 v=standard.copy();v[j]=0xfffffffe if j==20 else 45;rows.append((0,v))
raws=[w(*v)+bytes(24) for guard,v in rows]
inputs=[raw[:92]+w(guard)+raw[92:] for raw,(guard,v) in zip(raws,rows)]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-create-guard'],input=b''.join(inputs))
assert len(actual)==612*len(rows)
for i,((guard,v),raw) in enumerate(zip(rows,raws)):
 result=actual[i*612:i*612+160];trace=actual[i*612+160:(i+1)*612];values=struct.unpack('<40I',result)
 assert values[0]==(-3 if guard==5 else -4)&0xffffffff
 assert values[5]==0
 if guard:
  assert values[4]==0 and struct.unpack_from('<I',trace)[0]==0,'preflight/full-list path dispatched resource work'
  assert values[1]==(0x402 if guard==5 else 0)
  assert values[3]==(30 if guard==5 else 0)
 else:
  assert values[4]==1 and values[1]==0x402,'late failure must expose allocated owner and retain source marks'
  assert values[3]==(0 if v[20]!=0 else 1)
  count=struct.unpack_from('<I',trace)[0];ops=[struct.unpack_from('<I',trace,4+j*28)[0] for j in range(count)]
  assert ops[-1]==0x428fe0 and 0x42dc00 not in ops
  assert count==(3 if v[20]!=0 else 6 if v[21]!=1 else 7)
 base['traces'].append(trace);index=len(base['traces'])-1
 base['native_case'](index,raw,result,[(0,0,0,0)]*30 if guard==5 else (),guard)
report=dict(result='PASS',guard_cases=5,partial_owner_cases=3,scope='PC/NXDK insufficient scratch, invalid clock/timestamp, broken head and full30-body list guards; no resource calls. Invalid motion callbacks after allocation retain output owner, and may precede/follow list insertion. This verifies exposure, not concrete resource cleanup; caller unwind remains open.')
(root/'artifacts/corpse-create-guards.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
