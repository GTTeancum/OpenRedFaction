"""Original lightmap RGB upload slice vs shared PC/NXDK packing and mutation."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_EBP
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
 p=pefile.PE(str(path));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data);m.mem_map(0x30000000,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);binary=root/'build/xbox/main.exe';x=machine(binary)
b=0x30000000;rgb=b+0x1000;packed=b+0x2000;stack=b+0xe000;stop=b+0xf000
get=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
capability=1;available=1;pitch=0;trace=[]
def hook(m,address,size,unused):
 if address not in [0x50df50,0x50e2e0,0x50e310]:return
 sp=m.reg_read(UC_X86_REG_ESP);result=0
 if address==0x50df50:result=capability;trace.append('capability')
 elif address==0x50e2e0:
  assert [get(m,sp+4),get(m,sp+8),get(m,sp+16)]==[123,0,2]
  target=get(m,sp+12);m.mem_write(target+12,w(packed));m.mem_write(target+24,w(pitch));result=available;trace.append('lock')
 else:trace.append('unlock')
 m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,get(m,sp))
u.hook_add(UC_HOOK_CODE,hook)
entry=int(re.search(r'\s_rf_lightmap_pack_1555\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
inputs=[];outputs=[]
def shared(wire,want):
 width,height,stride,rgb_bytes,packed_bytes,double,present=struct.unpack_from('<7I',wire)
 x.mem_write(rgb,wire[28:540]);x.mem_write(packed,wire[540:]);x.mem_write(stack,w(stop,rgb,rgb_bytes,width,height,double,packed if present else 0,stride,packed_bytes));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(rgb,512))+bytes(x.mem_read(packed,512))
 assert actual==want,('NXDK',len(inputs),[(i,a,b) for i,(a,b) in enumerate(zip(actual,want)) if a!=b][:10])
 inputs.append(wire);outputs.append(want)
for channel in range(256):
 for capability in [0,255]:
  for available in [0,1]:
   width=[1,3,8][channel%3];height=[1,2,4][channel%3];pitch=width*2+(channel%3)*2
   raw=bytes((channel+i*17)&255 for i in range(512));initial=bytes([0xa5])*512;trace.clear()
   u.mem_write(rgb,raw);u.mem_write(packed,initial);u.mem_write(b+12,w(rgb,123));u.mem_write(stack,bytes(128))
   u.mem_write(stack+0x10,w(b));u.mem_write(stack+0x24,w(height));u.mem_write(stack+0x2c,w(width))
   u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,b);u.reg_write(UC_X86_REG_EBP,height);u.reg_write(UC_X86_REG_EDI,width)
   u.emu_start(0x4ed32c,0x4ed4fa,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x4ed4fa
   assert trace==['capability','lock']+(['unlock'] if available else [])
   want=w(0)+bytes(u.mem_read(rgb,512))+bytes(u.mem_read(packed,512))
   shared(w(width,height,pitch,width*height*3,pitch*height,int(not capability),available)+raw+initial,want)
original_cases=len(inputs)
for field,value in [(0,0),(2,1),(3,0),(4,0),(5,2)]:
 raw=bytearray(inputs[1]);struct.pack_into('<I',raw,field*4,value)
 shared(bytes(raw),w(-4)+raw[28:])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--lightmap-pack'],input=b''.join(inputs));assert actual==b''.join(outputs),'PC mismatch'
report=dict(result='PASS',original_cases=original_cases,port_guards=5,original_sha256=digest,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Original4ed32c..4ed4fa upload slice, actual brightening/packing/max helper/pitched loops. Capability and bitmap lock/unlock boundaries supplied. Exact PC/NXDK RGB mutation, packed output and untouched padding; all256 byte values, both capability branches, success/failure locks. Does not establish active renderer capability, file upload dispatch or texture allocation ownership.')
(root/'artifacts/lightmap-pack.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
