"""Installed animated sphere loading and original object-radius calculation."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_FPCW,UC_X86_REG_EAX
b=0x30000000;stack=b+0xe000;stop=b+0xff00
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);binary=root/'build/xbox/main.exe';x=machine(binary);entry=int(re.search(r'\s_rf_model_origin_radius\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
inv=json.loads((root/'artifacts/inventory.json').read_text())['files'];archive=next(a for a in inv if a['path']=='meshes.vpp');rows=[]
for e in archive['vpp']['entries']:
 if not e['name'].lower().endswith('.v3c'):continue
 with (root/'Installed_Game/meshes.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
 # Locate first SUBM through serialized outer chunk sizes before any submesh.
 offset=40
 while True:
  kind,size=struct.unpack_from('<II',raw,offset);offset+=8
  if kind==0x5355424d:break
  assert kind and size;offset+=size
 lods=struct.unpack_from('<I',raw,offset+52)[0];assert 1<=lods<=3
 sphere=raw[offset+56+lods*4:offset+72+lods*4];assert len(sphere)==16
 pc=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),'--bound-sphere',str(root/'Installed_Game/meshes.vpp'),e['name']]);assert pc[:16]==sphere
 u.mem_write(b,bytes(0x8000));u.mem_write(b+0x80,w(b+0x2000));u.mem_write(b+0x2000,w(2,0,b+0x3000))
 u.mem_write(b+0x3000+0x19c0+0x90,w(b+0x6000));u.mem_write(b+0x6000+0x8c,w(b+0x7000));u.mem_write(b+0x7000+0x1c,sphere)
 u.reg_write(UC_X86_REG_ESI,b);u.reg_write(UC_X86_REG_EBP,2);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(0x48a091,0x48a0c2,count=2000);assert u.reg_read(UC_X86_REG_EIP)==0x48a0c2
 assert bytes(u.mem_read(b+0x78,4))==pc[16:],e['name']
 x.mem_write(b,sphere);x.mem_write(stack,w(stop,b,b+0x100));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=2000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(b+0x100,4))==pc[16:],e['name']
 rows.append(dict(model=e['name'],sphere=list(struct.unpack('<4f',sphere)),object_radius=struct.unpack('<f',pc[16:])[0]))
assert len(rows)==95
report=dict(result='PASS',models=len(rows),original_sha256=sha,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='PC streamed first SUBM sphere compared with independent serialized offsets. Original48a091..48a0c0 executes full kind2 model pointer chain and real5032d0/5015f0/501610/504510/5044e0/504500/40a000. Exact PC/NXDK numeric radius. Original model-file loader and native file I/O not executed; campaign retention remains open.',rows=rows)
(root/'artifacts/model-bound-sphere.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='rows'}))
