"""Original room-surface query vs shared PC/NXDK retained-room callback."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
binary=root/'build/xbox/main.exe';p=pefile.PE(str(binary));data=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(data)+4095)//4096*4096);x.mem_write(base,data)
b=0x30000000;x.mem_map(b,65536);world=b+0x1000;rooms=b+0x2000;node=b+0x3000;face=b+0x4000;verts=b+0x5000;start=b+0x6000;out=b+0x7000;scratch=b+0x8000;source=b+0x9000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
put=lambda a,v:x.mem_write(a,w(v))
entry=int(re.search(r'\s_rf_geometry_corpse_surface\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
inputs=[];outputs=[]

def compare_wire(wire,want):
 descriptor=struct.unpack_from('<I',wire)[0];geometry=wire[4:]
 x.mem_write(b,bytes(0xa000))
 put(world+4,rooms);put(world+20,2)
 x.mem_write(rooms+80,w(0,node,face,source,scratch,1,1,1,0,0))
 x.mem_write(node,geometry[16:40]+w(0,1,0xffffffff,0xffffffff))
 x.mem_write(face,geometry[:40]+w(verts,4,0,0,0,0,0,0));x.mem_write(verts,geometry[40:88]);x.mem_write(start,geometry[88:100]);put(source,7)
 x.mem_write(out,bytes([0xa5])*28);put(out+28,99)
 x.mem_write(stack,w(stop,world,descriptor,start,out,out+28));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out+28,4))+bytes(x.mem_read(out,28))
 assert actual==want,('NXDK',len(inputs),actual.hex(),want.hex())
 inputs.append(wire);outputs.append(want)

def observe(c):
 want=w(0,int(c['matched']))+(c['hit']+w(7) if c['matched'] else bytes([0xa5])*28)
 compare_wire(w(2)+c['geometry'],want)

runpy.run_path(str(root/'tools/verify_corpse_source_surface.py'),init_globals={'observe_case':observe})
original_cases=len(inputs)
for descriptor in [0,1,3]:
 want=w(0 if descriptor==1 else -4,0 if descriptor==1 else 99)+bytes([0xa5])*28
 compare_wire(w(descriptor)+inputs[0][4:],want)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--corpse-surface-room'],input=b''.join(inputs))
assert actual==b''.join(outputs),('PC',len(actual),len(b''.join(outputs)))
report=dict(result='PASS',original_cases=original_cases,port_room_guards=3,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Exact point/normal and mapped face identity match original4df690 within full42dc50 across384 single-face flat/sloped queries. Shared selected-room callback runs compiled PC/NXDK collision code at FLT_MAX, with no primary-room list. Empty/absent/out-of-range room tests are port guards. No authored world, dynamic geometry, color or render comparison.')
(root/'artifacts/corpse-room-surface.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
