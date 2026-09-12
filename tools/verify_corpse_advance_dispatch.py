"""Observe original503360/501ab0 dispatch; stop at the skeletal update entry."""
import hashlib,itertools,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));data=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(data)+4095)//4096*4096);u.mem_write(base,data)
b=0x30000000;u.mem_map(b,65536);stack=b+0xe000;stop=b+0xf000;payload=b+0x100
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
u.mem_write(b,w(2,payload));observed=[]
def hook(m,address,size,context):
 if address==0x51ba80:
  sp=m.reg_read(UC_X86_REG_ESP)
  observed.append((m.reg_read(UC_X86_REG_ECX),bytes(m.mem_read(sp+4,12))))
  m.emu_stop()
u.hook_add(UC_HOOK_CODE,hook)
cases=0
for delta,aux,position,basis,flag in itertools.product([0,0x3c888889,0x3e99999a,0x3f800000],[0,0x12345678],[0,0xdead0000],[0,0xbeef0000],[0,1]):
 observed.clear();u.mem_write(stack,w(stop,b,delta,aux,position,basis,flag));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x503360,stop,count=1000)
 assert observed==[(payload,w(delta,aux,flag))],(delta,aux,position,basis,flag,observed)
 cases+=1
report=dict(result='PASS',cases=cases,original_sha256=digest,scope='Original503360 and501ab0 type2 dispatch with unmapped transform pointers. Observes51ba80 entry then stops: payload this-pointer and delta/aux/flag arguments preserved, position/basis not forwarded or read. Does not execute playback, test type3, or establish pose-evaluation/render timing.')
(root/'artifacts/corpse-advance-dispatch.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
