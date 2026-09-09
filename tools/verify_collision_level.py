"""Complete original segment/AABB function, including failed-attempt writes."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EBX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);stack=base+60000;stop=base+64000

raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--level',str(root/'Installed_Game/levels1.vpp'),'L1S1.rfl'])
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_thin_face\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match
entry=int(match.group(1),16)
u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:uc.emu_stop(),begin=0x4e0045,end=0x4e0045)
at=0;cases=0;hits=0;max_corners=0
while at<len(raw):
 index,count=struct.unpack_from('<II',raw,at);assert index==cases and 1<=count<=256
 head=raw[at+8:at+48];vertices=raw[at+48:at+48+count*12];ray=raw[at+48+count*12:at+72+count*12];actual=raw[at+72+count*12:at+108+count*12];at+=108+count*12
 face=base;query=base+4096;edges=base+8192;out=base+20000;verts=base+24576
 u.mem_write(face,head);u.mem_write(face+0x28,bytes(16));u.mem_write(face+0x30,struct.pack('<i',-1));u.mem_write(face+0x40,struct.pack('<II',edges,0));u.mem_write(verts,vertices)
 for i in range(count):
  u.mem_write(edges+i*32,struct.pack('<I',verts+i*12));u.mem_write(edges+i*32+0x14,struct.pack('<II',edges+((i+1)%count)*32,edges+((i-1)%count)*32))
 # Execute original extrema accumulation plus inflation before the face query.
 u.reg_write(UC_X86_REG_ESI,face);u.reg_write(UC_X86_REG_EBP,edges);u.reg_write(UC_X86_REG_EBX,0);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4dff92,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x4e0045
 assert bytes(u.mem_read(face+0x10,24))==head[16:40],('bounds',index)
 u.mem_write(query+0x4c,struct.pack('<fI',0,0x461));u.mem_write(query+0x54,ray);u.mem_write(out,struct.pack('<If',0,1)+bytes(32));u.mem_write(0xca06b0,struct.pack('<I',15))
 u.mem_write(stack,struct.pack('<4I',stop,face,query,out));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4dec10,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 hit=u.reg_read(UC_X86_REG_EAX)&255;assert hit in (0,1)
 want=struct.pack('<iI',0,hit)+(bytes(u.mem_read(out+4,28)) if hit else bytes([0xa5])*28)
 assert actual==want,(index,actual.hex(),want.hex())
 x.mem_write(face,head+struct.pack('<II6I',verts,count,0x461,0,0,0,0,0));x.mem_write(verts,vertices);x.mem_write(query,ray);x.mem_write(out,bytes([0xa5])*32)
 x.mem_write(stack,struct.pack('<7I',stop,face,query,query+12,0x3f800000,out,out+28));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out+28,4))+bytes(x.mem_read(out,28));assert got==want,('NXDK',index,got.hex(),want.hex())
 cases+=1;hits+=hit;max_corners=max(max_corners,count)
assert at==len(raw)
report=dict(result='PASS',nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),level='L1S1.rfl',faces=cases,original_bounds_cases=cases,hits=hits,max_corners=max_corners,capacity_guards=cases,scope='Every loaded face bound to caller scratch and tested against complete original 4dec10. Diagnostic filter flags 0x461 with clear face/owner attributes, NOT reconstructed runtime face metadata. Rays use vertex-average plus normal and -2*normal. No world traversal or nearest world result.')
(root/'artifacts/collision-level-verification.json').write_text(json.dumps(report,indent=2));print(report)
