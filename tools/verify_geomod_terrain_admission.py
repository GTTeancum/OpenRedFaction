"""Original terrain-cut master minimum radius and duplicate-record gates."""
import hashlib,json,struct,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
exe=R/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);B=0x30000000;u.mem_map(B,65536);S=B+0xe000;w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]);f=lambda *v:struct.pack('<'+'f'*len(v),*v);rd=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
reached=[];stops=set()
def hook(cpu,a,size,_):
 if a in stops:reached.append(a);cpu.emu_stop();return
 if a==0x4b5900:
  sp=cpu.reg_read(UC_X86_REG_ESP);cpu.mem_write(rd(sp+12),bytes(cpu.mem_read(rd(sp+8),12)));cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);radii=[];stops={0x4670d7,0x4670e4}
for radius in [-1,0,.9999999403953552,1,1.0000001192092896,5]:
 reached.clear();u.mem_write(S+0x9c,f(radius));u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x4670c3,0x4670e5,count=100);assert reached==[0x4670e4 if radius>=1 else 0x4670d7];radii.append(dict(radius=radius,accepted=radius>=1))
rows=[];stops={0x467155,0x46726a}
for shape,room in [(7,11),(8,11),(7,12)]:
 for d in [0,.1,.19999998807907104,.2,.20000001788139343,.3]:
  reached.clear();u.mem_write(0x647c9c,w(1));u.mem_write(0x648600,struct.pack('<HHI',shape,0,room)+f(d,0,0)+bytes(12));u.mem_write(S+0x24,w(7,11));u.mem_write(B,f(0,0,0));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_EBP,B);u.emu_start(0x4671f7,0x46726b,count=10000)
  skipped=reached==[0x467155];assert skipped==(shape==7 and room==11 and d<=.2),(shape,room,d,reached);rows.append(dict(shape=shape,room=room,distance=d,duplicate_rejected=skipped))
(R/'artifacts/crater-shading-re/terrain-create-gates.json').write_text(json.dumps(dict(exe_sha256=sha,scope=__doc__,minimum_radius=radii,duplicates=rows),indent=2));print('PASS:6 minimum-radius cases and18 original duplicate gates')
