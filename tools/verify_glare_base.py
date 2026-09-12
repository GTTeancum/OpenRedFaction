"""Owned PC glare bases versus original type10 generic object evidence."""
import json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];rows=json.loads((root/'artifacts/glare-base-original.json').read_text())['records']
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
commands=[struct.pack('<f3I',a['radius'],7 if a['parent'] else 0,17 if a['parent'] else 1,0) for a in rows]
commands += [struct.pack('<f3I',.5,0,1,mode) for mode in (1,2)]
output=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--glare-base'],input=b''.join(commands));assert len(output)==552*len(commands)
for i,a in enumerate(rows):
 q=output[i*552:(i+1)*552];assert q[:24]==w(0,528,1,1024,0,0xfffffffe)
 raw=q[24:];b=bytes.fromhex(a['base']);body=b[0x88:0x1f8]
 expected=body[:12]+body[0x10:0xfc]+body[0x108:0x128]+body[0x138:0x148]+body[0x15c:0x160]+body[0x164:0x16c]
 assert raw[200:508]==expected,(i,[(j,raw[200+j:204+j].hex(),expected[j:j+4].hex()) for j in range(0,308,4) if raw[200+j:204+j]!=expected[j:j+4]])
 # Identity values are normalized because each PC case starts a fresh registry.
 assert raw[112:144]==w(0x10000,0xffffffff,0x6030000,10,123,7 if a['parent'] else 0,17 if a['parent'] else 1,-1)
 assert raw[144:152]==b[0x78:0x7c]+b[0x34:0x38]
 assert raw[152:200]==b[0x3c:0x6c]
 assert raw[:8]==w(-1,-1) and raw[104:112]==bytes(8)
 assert raw[508:]==w(0,0,12,324,528)
for i,mode in enumerate((1,2),len(rows)):
 q=output[i*552:(i+1)*552];assert q[:24]==w(-4 if mode==1 else 0,528,0,1024 if mode==1 else 0,0,-1) and q[24:]==bytes(528)
report=dict(result='PASS',original_cases=len(rows),guard_cases=2,owner_bytes=528,scope='PC concrete owner body floats and defined generic fields versus actual original type10 allocation, normalized identities. Exact budget, one-byte-short budget, empty registry, linked-family close rejection, unlink/close, stale handle removal and repeated close. No compiled NXDK allocation failure or native XEMU/live glare proof.')
(root/'artifacts/glare-base.json').write_text(json.dumps(report,indent=2));print(report)
