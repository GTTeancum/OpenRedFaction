"""Original code-explosion dispatch and central-emitter normal assignment."""
import hashlib,json,struct,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/crater-shading-re').mkdir(parents=True,exist_ok=True)
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
exe=R/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);B=0x30000000;u.mem_map(B,65536);S=B+0xe000;w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]);f=lambda *v:struct.pack('<'+'f'*len(v),*v);rd=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
trace=[]
def hook(cpu,a,size,_):
 if a!=0x48e640:return
 sp=cpu.reg_read(UC_X86_REG_ESP);args=struct.unpack('<6I',cpu.mem_read(sp+4,24));trace.append(dict(name=bytes(cpu.mem_read(args[0],32)).split(b'\0')[0].decode(),position=struct.unpack('<3f',cpu.mem_read(args[1],12)),normal=struct.unpack('<3f',cpu.mem_read(args[2],12)),scale=struct.unpack('<f',w(args[3]))[0],room=args[4],flags=args[5]));cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);rows=[]
for scale in [-1,0,1.5]:
 for normal in [None,(1,0,0),(0,0,-1)]:
  for liquid,y in [(0,0),(1,-1),(1,0),(1,1)]:
   trace.clear();u.mem_write(B,bytes(0x4000));u.mem_write(B+0x30,w(8));u.mem_write(B+0xc0,b'rocket hit\0');u.mem_write(B+0x1184,bytes([liquid]));u.mem_write(B+0x2000,f(3,y,5));u.mem_write(B+0x2100,f(*(normal or (0,0,0))));u.mem_write(S+0xdc,w(B+0x2000)+f(scale));u.mem_write(S+0xe8,w(B+0x2100 if normal else 0));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_EBX,B+0x1000)
   u.emu_start(0x4c1a02,0x4c1a97,count=10000);assert len(trace)==1 and trace[0]['normal']==(normal or (0,1,0)) and trace[0]['scale']==scale and trace[0]['flags']==int(liquid and y<=0);rows.append(trace[0].copy())
# Actual central template assignment uses explosion normal, replacing authored direction.
central=[]
for normal in [(1,0,0),(0,0,-1),(.25,.5,.75)]:
 u.mem_write(B+0x3000,f(*normal));u.mem_write(B+0x4010,f(0,1,0));u.mem_write(B+0x5000,w(0));u.mem_write(0x7b2770,w(B+0x4000));u.mem_write(S+0xfc,w(B+0x3000));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_EDI,B+0x5000);u.emu_start(0x48e900,0x48e91e,count=1000);assert bytes(u.mem_read(B+0x4010,12))==f(*normal);central.append(normal)
(R/'artifacts/crater-shading-re/code-explosion.json').write_text(json.dumps(dict(exe_sha256=sha,scope=__doc__,dispatch_cases=rows,central_direction_cases=central),indent=2));print('PASS:36 original code-explosion cases +3 unhooked central direction assignments')
