"""Compiled two-tag clutter stage: ordering, disabled class and failure footprint."""
import itertools,json,re,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x10000);C=B;S=B+0x1000;BE=B+0x2000;STACK=B+0xe000;STOP=B+0xf000;CB=STOP+0x100
entry=int(re.search(r'\s_rf_clutter_create_rods\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
def text(a):
 out=bytearray()
 while x.mem_read(a,1)!=b'\0':out+=x.mem_read(a,1);a+=1
 return out.decode()
trace=[];failure=-1;first=-1;second=-1

def hook(cpu,address,length,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i)
 assert arg(0)==123 and arg(1)==S;op=arg(2);q=arg(3)
 if op==4:
  assert r(q)==777;name=text(r(q+20));assert name in ('corona_rod1','corona_rod2')
  trace.append(('tag',name));value=first if name=='corona_rod1' else second
 elif op==6:
  values=tuple(struct.unpack('<5I',cpu.mem_read(q,20)))
  assert values==(55,rodclass,first,second,0xffffffff);trace.append(('rod',values));value=0
 else:raise AssertionError(op)
 status=0xffffffff if len(trace)-1==failure else 0
 if not status:cpu.mem_write(arg(4),w(value))
 cpu.reg_write(UC_X86_REG_EAX,status);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
x.hook_add(UC_HOOK_CODE,hook,begin=CB,end=CB)
cases=0
for first,second,rodclass in itertools.product((-1,0,51),(-1,0,52),(-1,0,4)):
 events=[('tag','corona_rod1')];status=0
 if first>=0:
  events.append(('tag','corona_rod2'))
  if second<0:status=0xfffffffe # RF_FORMAT
  elif rodclass>=0:events.append(('rod',(55,rodclass,first,second,0xffffffff)))
 for failure in range(-1,len(events)):
  cls=bytearray(b'\xa5'*96);cls[56:60]=w(rodclass)
  state=bytearray(b'\xa5'*108);state[8:16]=w(55,777)
  x.mem_write(C,bytes(cls));x.mem_write(S,bytes(state));x.mem_write(BE,w(0,CB,123));trace=[]
  x.mem_write(STACK,w(STOP,C,S,BE));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=100000)
  assert x.reg_read(UC_X86_REG_EIP)==STOP
  expected_status=0xffffffff if failure>=0 else status
  assert x.reg_read(UC_X86_REG_EAX)==expected_status,(first,second,rodclass,failure,x.reg_read(UC_X86_REG_EAX),expected_status)
  assert trace==(events[:failure+1] if failure>=0 else events)
  assert bytes(x.mem_read(C,96))==cls and bytes(x.mem_read(S,108))==state
  cases+=1
report=dict(result='PASS',cases=cases,scope='Actual compiled NXDK rod stage: ordered tags, disabled rod class, zero tag/class indices, missing second tag, every callback failure and unchanged class/state. Callback effects supplied; full original factory equivalence tested separately. No native scene claim.')
(root/'artifacts/clutter-rod-stage.json').write_text(json.dumps(report,indent=2));print(report)

