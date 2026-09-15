"""Execute original4c8a10 impact dispatch, supplying only child effect call boundary."""
import hashlib,json,struct,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/crater-shading-re').mkdir(parents=True,exist_ok=True)
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
exe=R/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);B=0x30000000;u.mem_map(B,65536);stack=B+0xe000;stop=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]);f=lambda *v:struct.pack('<'+'f'*len(v),*v);rd=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
trace=[]
def hook(cpu,a,size,_):
 if a!=0x4c16e0:return
 sp=cpu.reg_read(UC_X86_REG_ESP);args=list(struct.unpack('<8I',cpu.mem_read(sp+4,32)));trace.append(args)
 cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EAX,0xffffffff)
u.hook_add(UC_HOOK_CODE,hook);rows=[]
for weapon in [0,7]:
 for count in [-1,0,1,2,3]:
  for radii in [(0,1.5,3),(-1,.125,0)]:
   trace.clear();off=weapon*1360;handles=[2,-1,17];u.mem_write(0x85d158+off,w(count,*handles));u.mem_write(0x85d180+off,f(*radii));u.mem_write(stack,w(stop,weapon,0x1111,0x2222,0x3333,0x4444,0x5555));u.reg_write(UC_X86_REG_ESP,stack)
   u.emu_start(0x4c8a10,stop,count=1000);assert u.reg_read(UC_X86_REG_EIP)==stop
   expected=[[handles[i]&0xffffffff,0x1111,0x2222,0x3333,struct.unpack('<I',f(radii[i]))[0],0x5555,0x4444,1] for i in range(max(0,count))];assert trace==expected,(trace,expected)
   rows.append(dict(weapon=weapon,count=count,radii=radii,calls=trace.copy()))
(R/'artifacts/crater-shading-re/impact-dispatch.json').write_text(json.dumps(dict(exe_sha256=sha,scope=__doc__,rows=rows),indent=2));print('PASS:20 original ordered dispatch cases; raw radius, invalid child and failed child continued')
