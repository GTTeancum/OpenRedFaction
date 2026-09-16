"""Original liquid-contact dispatcher numerical evidence; no native game launch."""
import sys,struct,json,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX
exe=R/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image()
B=0x30000000;S=B+0xe0000;stop=B+0xf0000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
rd=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
(R/'artifacts/crater-shading-re').mkdir(parents=True,exist_ok=True)
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x100000);rows=[];calls=[]
def hook(cpu,a,n,_):
 if a in [0x4c4ec0,0x4c59f0,0x467020]:raise AssertionError('liquid must not enter terrain/entity/geomod handler '+hex(a))
 if a in [0x4c16e0,0x40a490]:
  sp=cpu.reg_read(UC_X86_REG_ESP)
  if a==0x4c16e0:calls.append(list(struct.unpack('<8I',cpu.mem_read(sp+4,32))));v=0
  else:v=B+0x2000
  cpu.reg_write(UC_X86_REG_EAX,v);cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook)
for flags in [0,0x10000,0x1000000]:
 for liquid in [1,2]:
  calls.clear();u.mem_write(B,bytes(0x3000));u.mem_write(B+0x294,w(B+0x1000));u.mem_write(B+0x1264,w(flags));u.mem_write(B+0x152c,w(1));u.mem_write(B+0x34,f(10));u.mem_write(B+0x78,f(.1));u.mem_write(B+0x1c0,f(0,1,0));u.mem_write(B+0x1ec,w(liquid));u.mem_write(B+0x1ac,w(0x1000));u.mem_write(B+0x298,w(7));u.mem_write(0x872118,w(88));u.mem_write(0x8568a8,w(42));u.mem_write(S,w(stop));u.reg_write(UC_X86_REG_ECX,B);u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x4c4b50,stop,count=100000)
  assert u.reg_read(UC_X86_REG_EIP)==stop;ret=u.reg_read(UC_X86_REG_EAX);life=struct.unpack('<f',u.mem_read(B+0x34,4))[0];assert ret==1 and rd(B+0x1ec)==0 and rd(B+0x1ac)==0 and len(calls)==1;assert life==(0 if flags&0x10000 else 10)
  rows.append(dict(flags=flags,liquid=liquid,result=ret,life=life,liquid_after=rd(B+0x1ec),query_after=rd(B+0x1ac),effect=calls[0][0],effect_scale=struct.unpack('<f',w(calls[0][4]))[0]))
(R/'artifacts/crater-shading-re/geomod-liquid-contact.json').write_text(json.dumps(rows,indent=2));print('PASS',len(rows),'whole original liquid dispatch cases')
