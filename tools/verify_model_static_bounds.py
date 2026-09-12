"""Whole static-model bounds across original code, PC archive reads and NXDK."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_FPCW,UC_X86_REG_EAX,UC_X86_REG_ECX
b=0x30000000;stack=b+0xe000;stop=b+0xff00
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);binary=root/'build/xbox/main.exe';x=machine(binary);entry=int(re.search(r'\s_rf_model_static_bound_sphere\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
import random
from inspect_models import inspect
commands=[];expected=[];records=[]
def check(rows,name=None):
 count=len(rows)//16;u.mem_write(b+0x48,w(count,b+0x1000))
 for i in range(count):
  u.mem_write(b+0x1000+i*144+140,w(b+0x6000+i*64));u.mem_write(b+0x6000+i*64+28,rows[i*16:(i+1)*16])
 u.mem_write(stack,w(stop,b+0xc000,b+0xc00c));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,b);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(0x53c36d,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 original=bytes(u.mem_read(b+0xc000,16))
 x.mem_write(b,rows);x.mem_write(b+0xc000,b'\xa5'*16);x.mem_write(stack,w(stop,b,count,b+0xc000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(b+0xc000,16))==original,(name,original.hex(),bytes(x.mem_read(b+0xc000,16)).hex())
 if name:
  pc=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),'--static-bound',str(root/'Installed_Game/meshes.vpp'),name])
  assert pc[:16]==original,name
  records.append(dict(model=name,submeshes=count,sphere=list(struct.unpack('<4f',original))))
 commands.append(w(count)+rows);expected.append(w(0)+original)
inv=json.loads((root/'artifacts/inventory.json').read_text())['files'];archive=next(a for a in inv if a['path']=='meshes.vpp')
for e in archive['vpp']['entries']:
 if not e['name'].lower().endswith('.v3m'):continue
 with (root/'Installed_Game/meshes.vpp').open('rb') as stream:stream.seek(e['offset']);data=stream.read(e['size'])
 parsed=inspect(data);sections=parsed['sections'];rows=[]
 for section in sections:
  if section['type']!='0x5355424d':continue
  offset=section['offset']+8;lods=struct.unpack_from('<I',data,offset+52)[0];rows.append(data[offset+56+lods*4:offset+72+lods*4])
 check(b''.join(rows),e['name'])
rng=random.Random(0x53c36d)
for case in range(256):
 rows=b''.join(struct.pack('<4f',*[rng.uniform(-100,100) for _ in range(3)],rng.uniform(0,10)) for _ in range(1+case%32));check(rows)
check(bytes(16));check(struct.pack('<4f',-0.,0.,-0.,-0.))
for rows in (b'',struct.pack('<4f',0,0,0,-1),struct.pack('<4f',float('nan'),0,0,1)):
 count=len(rows)//16;x.mem_write(b,rows or bytes(16));x.mem_write(b+0xc000,b'\xa5'*16);x.mem_write(stack,w(stop,b,count,b+0xc000));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(b+0xc000,16))==b'\xa5'*16
 commands.append(w(count)+rows);expected.append(w(0xfffffffc)+b'\xa5'*16)
assert subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),'--static-bounds'],input=b''.join(commands))==b''.join(expected)
report=dict(result='PASS',models=len(records),synthetic_cases=258,guards=3,original_sha256=sha,scope='Complete original53c36d and real submesh getters/vector arithmetic, no hooks, versus PC/NXDK aggregation. PC streams all installed static submesh spheres using independent serialized offsets. Original full model loader and native Xbox archive I/O are not executed.',records=records)
(root/'artifacts/model-static-bounds.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})
