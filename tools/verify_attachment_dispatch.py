"""Execute original attachment ordering and tag-matrix dispatch at service boundaries."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));raw=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(raw)+4095)&~4095);u.mem_write(0x400000,raw)
base=0x30000000;stack=base+0x3e000;stop=base+0x3f000
u.mem_map(base,0x40000);u.reg_write(UC_X86_REG_FPCW,0x27f)
w=lambda *a:struct.pack('<'+'I'*len(a),*a)
def read(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def ret(value=0):
 sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_EIP,read(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
mode='order';events=[];parents={}
def hook(m,a,s,d):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x40a0e0:
  handle=read(sp+4);events.append(('lookup',handle));ret(parents.get(handle,0))
 elif a==0x487630 and mode=='order':
  events.append(('pose',read(sp+4),read(sp+8)));ret()
 elif a==0x5034f0:
  events.append(tuple(read(sp+4+4*i) for i in range(6)));ret()
for a in (0x40a0e0,0x487630,0x5034f0):u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
rng=random.Random(4881);visited=0x1000000;dirty=0x4000000
for case in range(1000):
 n=16;addresses=[base+i*0x1000 for i in range(n)];parents={i:addresses[i] for i in range(n)}
 links=[rng.randrange(-1,i) if i else -1 for i in range(n)]
 flags=[rng.getrandbits(32) for i in range(n)];expected=flags[:];want=[];events=[]
 for i,a in enumerate(addresses):u.mem_write(a+0x7c,w(flags[i]));u.mem_write(a+0x200,w(links[i]&0xffffffff))
 def visit(i):
  if expected[i]&visited:return
  parent=links[i];want.append(('lookup',parent&0xffffffff))
  if parent>=0:
   visit(parent)
   if expected[parent]&dirty:expected[i]|=dirty;want.append(('pose',addresses[i],addresses[parent]))
  expected[i]|=visited
 order=list(range(n));rng.shuffle(order)
 for i in order:
  visit(i);u.mem_write(stack,w(stop,addresses[i],0));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4881a0,stop,count=10000)
  assert u.reg_read(UC_X86_REG_EIP)==stop
 assert events==want,(case,events,want)
 assert [read(a+0x7c) for a in addresses]==expected
mode='tag';child=base;parent=base+0x1000;cls=base+0x2000;tag_cases=0
for kind in (0,1,2,3,4,5,6,10,0xffffffff):
 for category in (0,1,4,7,8,0xffffffff):
  events=[];u.mem_write(child,bytes(0x900));u.mem_write(parent,bytes(0x900));u.mem_write(cls,bytes(0x200))
  u.mem_write(child+0x204,w(13));u.mem_write(parent+0x24,w(kind));u.mem_write(parent+0x294,w(cls))
  u.mem_write(parent+0x80,w(0x12345678));u.mem_write(cls+0x1b4,w(category));u.mem_write(cls+0x44,w(category))
  u.mem_write(stack,w(stop,child,parent));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x487630,0x48770f,count=10000)
  assert u.reg_read(UC_X86_REG_EIP)==0x48770f and len(events)==1
  alternate=kind in (0,4) and category==4
  assert events[0][:4]==(0x12345678,13,parent+(0x7e0 if alternate else 0x48),parent+0x3c),(kind,category,events)
  tag_cases+=1
report=dict(result='PASS',ordering_forests=1000,objects_per_forest=16,tag_dispatch_cases=tag_cases,original_sha256=sha,
 scope='Original4881a0 recursion/flags executed; handle lookup and pose publication intercepted as explicit services. Original487630 positive-tag prefix and unmodified486c90 classify actual object/class fields;5034f0 arguments captured. No shared runtime or live moving-parent validation; negative-tag math and cyclic graphs excluded.')
(root/'artifacts/attachment-dispatch.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
