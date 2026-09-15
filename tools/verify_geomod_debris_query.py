"""Execute original debris query wrappers with only final solid intersection supplied."""
import hashlib,json,struct,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/crater-shading-re').mkdir(parents=True,exist_ok=True)
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
exe=R/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);B=0x30000000;u.mem_map(B,65536);stack=B+0xe000;stop=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v);f=lambda *v:struct.pack('<'+'f'*len(v),*v);rd=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
trace=[];fraction=.25

def hook(cpu,a,size,_):
 if a!=0x4df1c0:return
 sp=cpu.reg_read(UC_X86_REG_ESP);query=rd(sp+4);out=rd(sp+8)
 trace.append(dict(flags=hex(rd(query+0x50)),start=struct.unpack('<3f',cpu.mem_read(query+0x34,12)),delta=struct.unpack('<3f',cpu.mem_read(query+0x40,12))))
 cpu.mem_write(out,w(1)+f(fraction,1,2,3)+bytes(16)+w(B+0x3000))
 cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+16)
u.hook_add(UC_HOOK_CODE,hook);rows=[]
for radius in [.5,1,5]:
 for fraction in [0,.25,1]:
  trace.clear();u.mem_write(B,f(0,0,0,0,0,radius));u.mem_write(B+0x100,w(B+0x2000));u.mem_write(B+0x3000,bytes(80));u.mem_write(B+0x200,f(radius*14));u.mem_write(stack,w(stop,B,B+12,B+0x2000)+f(radius)+w(B+0x200));u.reg_write(UC_X86_REG_ESP,stack)
  u.emu_start(0x490890,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
  remaining=struct.unpack('<f',u.mem_read(B+0x200,4))[0];assert remaining==radius*14-fraction,(radius,fraction,remaining)
  assert len(trace)==1 and trace[0]['flags']=='0x5'
  rows.append(dict(radius=radius,supplied_intersection_fraction=fraction,remaining=remaining,queries=trace.copy()))
(R/'artifacts/crater-shading-re/debris-query.json').write_text(json.dumps(dict(exe_sha256=sha,scope=__doc__,rows=rows),indent=2));print('PASS: nine complete490890/48fc10/49c5c0 wrapper cases; flags5 and fraction subtraction')
