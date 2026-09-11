"""Compare shared PC constructor fields against full original416940 execution."""
import json,runpy,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
cases=[];expected=[]
def observe(g):
 keys=('flags724','flags728','flags814','flags810','flags7c','class_index','num_spheres','keep','seek','kind','source_model','extra_model','emitter_kind')
 values=[g[k] for k in keys]+[struct.unpack('<I',struct.pack('<f',g['life']))[0]]+[int(g[k]) for k in ('replacement','fail','model_fail','emit_found','null')]+[g['case']%4]+g['motion_indices']
 cases.append(w(*values)+g['raw']);read=g['read'];actor=g['actor'];corpse=g['corpse'];success=bool(g['result'])
 out=[0 if success else -3,read(actor+0x7c),read(actor+0x1410),read(0x5caed0),int(success),0]
 if success:
  offsets=(0x20,0x26c,0x2a0,0x1fc,0x2d8,0x2c8,0x2b4,0x2bc,0x2c0,0x2c4,0x2d4,0x78,0x180,0x294,0x29c,0x34,0x80,0x2b8,0x2cc,0x2ac,0x2b0)
  out += [read(corpse+j) for j in offsets]+[0x40a00000,read(corpse+0x2d0),read(corpse+0x2dc),read(corpse+0x2e0),int(bool(read(corpse+0x268))),1,1]+[0]*6
 else:out += [0]*34
 assert len(out)==40;expected.append(w(*out))
runpy.run_path(str(root/'tools/verify_corpse_create_original.py'),init_globals={'observe_case':observe})
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-create'],input=b''.join(cases))
assert len(actual)==len(cases)*160
for i,want in enumerate(expected):assert actual[i*160:(i+1)*160]==want,(i,struct.unpack('<40I',actual[i*160:(i+1)*160]),struct.unpack('<40I',want))
report=dict(result='PASS',cases=len(cases),scope='Shared PC constructor fields/source ownership/physics-seed copy vs complete original416940. Supplied allocation/resource backends. No NXDK execution comparison, multi-corpse constructor retention or live scene dispatch yet.')
(root/'artifacts/corpse-create-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
