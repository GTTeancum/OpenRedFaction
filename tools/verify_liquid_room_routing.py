"""Original liquid-contact dispatcher numerical evidence; no native game launch."""
import sys,struct,json,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_FPCW
exe=R/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image()
B=0x30000000;S=B+0xe0000;stop=B+0xf0000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
rd=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
(R/'artifacts/crater-shading-re').mkdir(parents=True,exist_ok=True)
from unicorn.x86_const import *
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x100000)
room=B; child=B+0x1000; query=B+0x2000;out=B+0x3000; faces=[B+0x4000,B+0x4100,B+0x4200];calls=[];rows=[];fractions={}
def hook(cpu,a,n,_):
 if a in [0x4df661,0x4df681]:cpu.emu_stop();return
 if a not in [0x40a490,0x40a480,0x476900,0x4deab0,0x4dec10]:return
 sp=cpu.reg_read(UC_X86_REG_ESP);ecx=cpu.reg_read(UC_X86_REG_ECX);cleanup=0;value=0
 if a==0x40a490:value=faces[0] if ecx==room+0x28 else 1 if ecx==room+0x6c else 0
 elif a==0x40a480:u.mem_write(B+0x7000,w(child));value=B+0x7000;cleanup=4
 elif a==0x476900:
  ix=faces.index(rd(sp+4));value=faces[ix+1] if ix+1<len(faces) else 0;cleanup=4
 else:
  token=rd(sp+4);calls.append(hex(token));t=fractions[token];old=struct.unpack('<f',u.mem_read(out+4,4))[0]
  if t<=old:u.mem_write(out,w(rd(out)+1)+f(t));u.mem_write(out+36,w(token));value=1
  cleanup=12 if a==0x4deab0 else 0
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
u.hook_add(UC_HOOK_CODE,hook)
configs=[('nearest',0x1000,0,1,0,.8,.7,.5,.3),('first_solid',0x1001,0,1,0,.8,.7,.5,.3),('sky_water',0x1000,1,1,0,.8,.7,.5,.3),('sky_first_water',0x1001,1,1,0,.8,.7,.5,.3),('allow_sky',0x1008,1,1,0,.8,.7,.5,.3),('query_disabled',0,0,1,0,.8,.7,.5,.3),('room_disabled',0x1000,0,0,0,.8,.7,.5,.3),('outside_bounds',0x1000,0,1,1,.8,.7,.5,.3),('tie_later',0x1000,0,1,0,.5,.5,.5,.5),('far_water',0x1000,0,1,0,.2,.3,.5,.7)]
for label,flags,sky,liquid,outside,pt,ct,f1,f2 in configs:
 u.mem_write(B,bytes(0x10000));calls.clear();fractions={B+0x8000:pt,B+0x8100:ct,faces[0]:f1,faces[2]:f2}
 for addr,tree in [(room,B+0x8000),(child,B+0x8100)]:u.mem_write(addr+8,f(-10,-10,-10,10,10,10));u.mem_write(addr+0x3c,w(tree))
 u.mem_write(room+1,bytes([sky]));u.mem_write(room+0x184,bytes([liquid]));u.mem_write(query+0x50,w(flags));u.mem_write(out,w(0)+f(1))
 for ix,face in enumerate(faces):u.mem_write(face+0x28,w(4 if ix!=1 else 0))
 u.mem_write(S+0x10,w(B+0x9000));u.mem_write(S+0x20,f(101,101,101) if outside else f(1,1,1));u.mem_write(S+0x2c,f(100,100,100) if outside else f(-1,-1,-1))
 for reg,v in [(UC_X86_REG_ESP,S),(UC_X86_REG_ESI,query),(UC_X86_REG_EDI,room),(UC_X86_REG_EBP,out),(UC_X86_REG_EBX,B+0x9000)]:u.reg_write(reg,v)
 u.emu_start(0x4df523,0x4df682,count=10000)
 rows.append(dict(label=label,calls=list(calls),count=rd(out),fraction=struct.unpack('<f',u.mem_read(out+4,4))[0],selected=hex(rd(out+36))))
assert rows[0]['calls']==list(map(hex,[B+0x8000,B+0x8100,faces[0],faces[2]]))
assert rows[1]['calls']==[hex(B+0x8000)]
assert rows[2]['calls']==list(map(hex,[faces[0],faces[2]]))
assert rows[3]['calls']==[hex(faces[0])]
assert rows[7]['calls']==[]
assert rows[8]['selected']==hex(faces[2])
(R/'artifacts/crater-shading-re/liquid-room-routing.json').write_text(json.dumps(rows,indent=2));print('PASS',len(rows),'original room routing cases')
