"""Compiled corona-stage partial progress and cache reuse; full original oracle separate."""
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
entry=int(re.search(r'\s_rf_clutter_create_glares\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
def text(a):
 out=bytearray()
 while x.mem_read(a,1)!=b'\0':out+=x.mem_read(a,1);a+=1
 return out.decode()
trace=[];failure=-1;available=0

def hook(cpu,address,length,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i)
 assert arg(0)==123 and arg(1)==S;op=arg(2);q=arg(3)
 if op==4:
  assert r(q)==777;name=text(r(q+20));number=int(name.removeprefix('corona_'));trace.append(('tag',number));value=100+number if number<=available else -1
 elif op==5:
  values=tuple(struct.unpack('<4I',cpu.mem_read(q,16)));assert values[0]==55 and values[3]==0
  trace.append(('glare',values[1],values[2]));value=0
 else:raise AssertionError(op)
 status=0xffffffff if len(trace)-1==failure else 0
 if not status:cpu.mem_write(arg(4),w(value))
 cpu.reg_write(UC_X86_REG_EAX,status);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
x.hook_add(UC_HOOK_CODE,hook,begin=CB,end=CB)
def call():
 x.mem_write(STACK,w(STOP,C,S,BE));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=1000000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)

def expected(flags,glare,initial,fail):
 cache=list(initial);events=[]
 if glare!=-1 and not flags&0x400:
  for number in range(1,available+2):
   events.append(('tag',number))
   if len(events)-1==fail:return flags,cache,events,0xffffffff
   if number<=available and len(cache)<4:cache.append(100+number)
  flags|=0x400
 for tag in cache:
  events.append(('glare',tag,glare&0xffffffff))
  if len(events)-1==fail:return flags,cache,events,0xffffffff
 return flags,cache,events,0
cases=0
for flags,glare,cached,available in itertools.product((0x20,0x420),(-1,3),(0,2,4),(0,1,4,6)):
 initial=list(range(201,201+cached));full=expected(flags,glare,initial,-1)
 for failure in range(-1,len(full[2])):
  cls=bytearray(b'\xa5'*96);cls[40:44]=w(flags);cls[52:56]=w(glare);cls[64:80]=w(*(initial+[0]*(4-cached)));cls[80:84]=w(cached)
  state=bytearray(b'\xa5'*108);state[8:16]=w(55,777)
  x.mem_write(C,bytes(cls));x.mem_write(S,bytes(state));x.mem_write(BE,w(0,CB,123));trace=[]
  finalflags,cache,events,status=expected(flags,glare,initial,failure)
  assert call()==status and trace==events
  cls[40:44]=w(finalflags);cls[64:64+4*len(cache)]=w(*cache);cls[80:84]=w(len(cache))
  assert bytes(x.mem_read(C,96))==cls and bytes(x.mem_read(S,108))==state
  cases+=1
  if not status:
   # Reuse the mutated class; no fresh tag queries once flag0x400 was set.
   failure=-1;trace=[];again=expected(finalflags,glare,cache,-1)
   assert call()==0 and trace==again[2] and bytes(x.mem_read(C,96))==cls;cases+=1
report=dict(result='PASS',cases=cases,scope='Compiled extracted stage: first-missing stop, four-tag cap, preexisting cache, class-1 cached requests, class flag reuse, every TAG/GLARE callback failure and exact class/state footprints. Callback effects supplied. Full original4104a0/PC/NXDK equivalence separately verified by verify_clutter_factory.py --shared.')
(root/'artifacts/clutter-glare-stage.json').write_text(json.dumps(report,indent=2));print(report)
