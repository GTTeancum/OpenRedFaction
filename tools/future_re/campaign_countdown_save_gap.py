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
for armed in [0,1]:
 for fired in [0,1]:
  for pending in [0,1]:
   u=machine();event=BASE;out=BASE+0x2000;count=out+0x100;u.mem_write(event+0x24,w(20721));u.mem_write(event+0x290,w(84,0,1500 if pending else -1));u.mem_write(event+0x2a8,w(-1,-1));u.mem_write(event+0x2b8,bytes([armed,fired,0,0]));u.mem_write(event+0x2bc,w(60));u.mem_write(0x856470,w(1,1,BASE+0x3000));u.mem_write(BASE+0x3000,w(event));u.mem_write(0x5a3ed8,w(1000));u.mem_write(out,b'\xaa'*32)
   def hook(cpu,a,n,data):
    if a==0x48a4f0:return_boundary(cpu,0xffffffff)
   u.hook_add(UC_HOOK_CODE,hook);call(u,0x4bdaa0,(out,count));n=u.mem_read(count,1)[0];assert n==pending;blob=bytes(u.mem_read(out,20)) if n else b''
   results.append(dict(armed=armed,fired=fired,pending=pending,count=n,record=blob.hex()))
assert len(set(r['record'] for r in results if r['pending']))==1
report=dict(result='PASS',original_sha256=SHA,cases=len(results),results=results,limitations=['Actual generic pending-event collector4bdaa0 and encoder4bda40 executed.','Handle-to-UID conversion boundary intercepted; other save collectors/full file not executed.','Does not establish global absence of another threshold-specific save path.'])
(ROOT/'artifacts/future-campaign-re/countdown-save-gap.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'pending-event save cases')
