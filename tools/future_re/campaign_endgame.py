"""Original cutscene completion broadcast and lifecycle boundary evidence; no port build."""
import sys,struct,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'local/python'))
(ROOT/'artifacts/future-campaign-re').mkdir(parents=True,exist_ok=True)
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
EXE=ROOT/'Installed_Game/RF.exe';SHA=hashlib.sha256(EXE.read_bytes()).hexdigest()
assert SHA=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
IMAGE=pefile.PE(str(EXE)).get_memory_mapped_image()
BASE=0x30000000;STACK=BASE+0x7e000;STOP=BASE+0x7f000
w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v])
def machine():
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(IMAGE)+4095)//4096*4096);u.mem_write(0x400000,IMAGE);u.mem_map(BASE,0x80000);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
def word(u,a):return struct.unpack('<I',u.mem_read(a,4))[0]
def call(u,entry,args=(),ecx=0):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ECX,ecx);u.emu_start(entry,STOP,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==STOP

def return_boundary(u,value=0,pop=0):
 sp=u.reg_read(UC_X86_REG_ESP);ret=word(u,sp);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,ret)
results=[]
case=IMAGE[0xb76f0+71];assert struct.unpack_from('<I',IMAGE,0xb7610+case*4)[0]==0x4b741a
for name in ['Shuttle','station_blowup','train02_end','call_credits','unknown_case']:
 u=machine();event=BASE;textptr=BASE+0x2000;player=BASE+0x4000;trace=[];raw=name.encode();u.mem_write(event+0x1c,w(len(raw),textptr));u.mem_write(textptr,raw+b'\0');u.mem_write(0x7c75d4,w(player))
 def hook(cpu,a,n,data):
  sp=cpu.reg_read(UC_X86_REG_ESP)
  if a==0x434190:trace.append(dict(call='state',args=list(struct.unpack('<2I',cpu.mem_read(sp+4,8)))));return_boundary(cpu)
  elif a==0x4a73e0:trace.append(dict(call='transition',args=list(struct.unpack('<3I',cpu.mem_read(sp+4,12)))));return_boundary(cpu)
 u.hook_add(UC_HOOK_CODE,hook);call(u,0x4bd0e0,ecx=event)
 if name=='call_credits':assert trace==[dict(call='state',args=[23,0])]
 else:
  assert trace==[dict(call='transition',args=[player,0x3fc00000,0x43e9a0])];assert bytes(u.mem_read(0x63aaf8,len(raw)+1))==raw+b'\0';call(u,0x43e9a0);assert trace[-1]==dict(call='state',args=[19,0])
 results.append(dict(name=name,trace=trace))
for flags in [0,0x400000,0xffffffff,0x12345678]:
 u=machine();event=BASE;entity=BASE+0x2000;u.mem_write(event+0x290,w(67));u.mem_write(event+0x29c,w(2,2,BASE+0x4000));u.mem_write(BASE+0x4000,w(101,999));u.mem_write(entity+0x810,w(flags))
 def hook(cpu,a,n,data):
  if a==0x426fc0:return_boundary(cpu,entity if word(cpu,cpu.reg_read(UC_X86_REG_ESP)+4)==101 else 0)
 u.hook_add(UC_HOOK_CODE,hook);call(u,0x4b9070,ecx=event);after=word(u,entity+0x810);assert after==flags&~0x400000;results.append(dict(clear_initial=hex(flags),clear_after=hex(after)))
report=dict(result='PASS',original_sha256=SHA,cases=len(results),results=results,limitations=['Actual Endgame handler4bd0e0,43e9b0 name handling/string copy and callback43e9a0 executed.','Player transition4a73e0 and state manager434190 intercepted; state19/23 presentation effects not executed.','Actual Clear_Endgame action via generic dispatcher executes; entity resolution intercepted.'])
(ROOT/'artifacts/future-campaign-re/endgame.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'endgame boundary cases')
