"""Authored trigger volume conversion versus original prepared loader/factory."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data)
 m.mem_map(base,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
u=machine(original);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_trigger_volume_init\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_EBX
source=b'';cursor=0
params=base+0x1000
def read_bytes(m,address,size,data):
 global cursor
 sp=m.reg_read(UC_X86_REG_ESP);ret,dest=struct.unpack('<2I',m.mem_read(sp,8))
 count=struct.unpack('<I',m.mem_read(sp+8,4))[0] if address==0x52d410 else 4
 m.mem_write(dest,source[cursor:cursor+count]);cursor+=count
 m.reg_write(UC_X86_REG_ESP,sp+(12 if address==0x52d410 else 16));m.reg_write(UC_X86_REG_EIP,ret)
for a in (0x52d410,0x52d780):u.hook_add(UC_HOOK_CODE,read_bytes,begin=a,end=a)
records=[r for level in json.loads((root/'artifacts/triggers.json').read_text())['results'] for r in level['records']]
commands=bytearray();expected=bytearray();shapes=[0,0]
for n,r in enumerate(records):
 shape=r['shape'];shapes[shape]+=1
 wire=w(shape)+struct.pack('<16f',*r['position'],r.get('radius',0),*r.get('orientation_disk',[0]*9),*r.get('dimensions_disk',[0]*3));commands.extend(wire)
 u.mem_write(base,bytes(0x2000));u.mem_write(params+0x2c,wire[4:16]);u.mem_write(params+0x28,wire[16:20]);u.mem_write(stack,bytes(0x200))
 if shape:
  source=wire[20:56];cursor=0;u.mem_write(stack,w(stop,params+0x38));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base+0x3000)
  u.emu_start(0x52d2d0,stop,count=10000);assert cursor==36 and u.reg_read(UC_X86_REG_EIP)==stop
  source=wire[56:68];cursor=0;u.reg_write(UC_X86_REG_ESP,stack)
  u.emu_start(0x4656e9,0x465722,count=10000);assert cursor==12 and u.reg_read(UC_X86_REG_EIP)==0x465722
  u.mem_write(params+0x5c,bytes(u.mem_read(stack+0x9c,12)))
 u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBP,params);u.reg_write(UC_X86_REG_EDI,params+0x2c);u.reg_write(UC_X86_REG_EBX,params+0x38);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x4bfb3b if shape else 0x4bfb71,0x4bfb85,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x4bfb85
 output=w(shape)+bytes(u.mem_read(base+0x3c,12))+bytes(u.mem_read(base+0x78,4))+bytes(u.mem_read(base+0x48,36))+bytes(u.mem_read(base+0x2c8,12));want=w(0)+output;expected.extend(want)
 x.mem_write(params,bytes(668));x.mem_write(params+20,w(shape));x.mem_write(params+596,wire[4:68]);x.mem_write(base,bytes([0xa5])*68);x.mem_write(stack,w(stop,params,base));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base,68));assert got==want,('NXDK',n,got.hex(),want.hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--trigger-volume'],input=commands);assert actual==expected
report=dict(result='PASS',authored_records=len(records),spheres=shapes[0],boxes=shapes[1],original_sha256=digest,scope='Original matrix reader 52d2d0, dimension read placement 4656e9..465722 and constructor shape blocks 4bfb3b/4bfb71..4bfb85. Binary read boundaries supplied bytes; vector/matrix copy callees unchanged. Exact PC/NXDK volumes for every installed trigger. Full factory, registration, live contact and dynamic transforms excluded.')
(root/'artifacts/trigger-volume-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
