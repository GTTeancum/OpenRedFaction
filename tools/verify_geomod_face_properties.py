"""Bounded original face property copy and face inversion; no shared builds."""
import hashlib,json,struct,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
sha=hashlib.sha256((R/'Installed_Game/RF.exe').read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(R/'Installed_Game/RF.exe')).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im)
B=0x30000000;u.mem_map(B,65536);w=lambda *v:struct.pack('<'+'I'*len(v),*v);f=lambda *v:struct.pack('<'+'f'*len(v),*v)
(R/'artifacts/crater-shading-re').mkdir(parents=True,exist_ok=True)
rows=[]
for flags in [0x100,0,1,2,4,8,16,32,64,128,256,512,1024,0xffffffff]:
 props=w(flags,0x12345678,101,0xffff0000,0xffffffff,0)
 u.mem_write(B,bytes(0x3000));u.mem_write(B+0x28,props);u.reg_write(UC_X86_REG_ESI,B+0x28);u.reg_write(UC_X86_REG_EAX,B+0x100);u.reg_write(UC_X86_REG_ESP,B+0xe000)
 u.emu_start(0x4e0254,0x4e0268,count=100);assert bytes(u.mem_read(B+0x128,24))==props
 u.mem_write(B+0x100,f(.25,-.5,.75,2));u.mem_write(B+0x140,w(B+0x1000))
 for n in range(3):u.mem_write(B+0x1000+32*n+0x14,w(B+0x1000+32*((n+1)%3),B+0x1000+32*((n-1)%3)))
 u.mem_write(B+0xe000,w(B+0xf000));u.reg_write(UC_X86_REG_ESP,B+0xe000);u.reg_write(UC_X86_REG_ECX,B+0x100)
 u.emu_start(0x4e2a00,B+0xf000,count=1000);assert u.reg_read(UC_X86_REG_EIP)==B+0xf000
 assert bytes(u.mem_read(B+0x128,24))==props
 plane=struct.unpack('<4f',u.mem_read(B+0x100,16));assert plane==(-.25,.5,-.75,-2)
 ring=[];p=struct.unpack('<I',u.mem_read(B+0x140,4))[0]
 for _ in range(3):ring.append((p-B-0x1000)//32);p=struct.unpack('<I',u.mem_read(p+0x14,4))[0]
 assert ring==[2,1,0]
 rows.append(dict(flags=hex(flags),copied_properties=True,inversion_preserves_properties=True,plane=plane,ring=ring))
(R/'artifacts/crater-shading-re/face-properties.json').write_text(json.dumps(dict(exe_sha256=sha,scope=__doc__,copy_entry='4e0254',copy_stop='4e0268',inversion_entry='4e2a00',cases=rows),indent=2))
print('PASS: 14 property-copy cases and complete unhooked face inversion cases')
