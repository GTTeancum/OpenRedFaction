"""Original door hold occupancy scan versus shared PC/NXDK snapshots."""
import json,random,re,runpy,struct,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
callback=base+0x9000;trace=[];native=[]
def hook(m,address,size,data):
 sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
 if address==0x4c08e0:value=base+0x1000
 else:
  obj=struct.unpack('<I',m.mem_read(sp+4,4))[0];trace.append(struct.unpack('<I',m.mem_read(obj+0x2c,4))[0]);value=0
 m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
for addr in (0x4c08e0,0x4174f0):u.hook_add(UC_HOOK_CODE,hook,begin=addr,end=addr)
def wake(m,address,size,data):
 sp=m.reg_read(UC_X86_REG_ESP);ret,ctx,handle=struct.unpack('<3I',m.mem_read(sp,12));native.append(handle)
 m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,wake,begin=callback,end=callback)
entry=int(re.search(r'_rf_trigger_occupancy\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
u.mem_write(0x1754474,w(7));u.mem_write(base,bytes(1024));u.mem_write(base+0x318,w(0x2003));u.mem_write(base+0x2e8,w(2))
rng=random.Random(46280);commands=bytearray();expected=bytearray();occupied=0;wakes=0
for case in range(2048):
 shape=case%3;radius=(0.,-1.,1.,2.)[case%4]
 volume=w(shape)+struct.pack('<16f',0,0,0,radius,1,0,0,0,1,0,0,0,1,2,2,2)
 people=[]
 for i in range(5):
  point=[rng.choice([-2.,-1.,-.5,0.,.5,1.,2.]) for j in range(3)]
  people.append(w(100+i,0x4000 if (case>>i)&1 else 0)+struct.pack('<3f',*point))
 command=volume+b''.join(people);commands.extend(command)
 trigger=bytearray(0x400);trigger[0x294:0x298]=w(shape);trigger[0x3c:0x48]=volume[4:16];trigger[0x78:0x7c]=volume[16:20];trigger[0x48:0x6c]=volume[20:56];trigger[0x2c8:0x2d4]=volume[56:68];u.mem_write(base+0x1000,bytes(trigger))
 for i,person in enumerate(people):
  obj=bytearray(0x400);obj[0x2c:0x30]=person[:4];obj[0x7c:0x80]=person[4:8];obj[0x3c:0x48]=person[8:20]
  obj[0x28c:0x290]=w(0x5cb060 if i==2 else 0x5cabb8 if i==4 else base+0x2000+(i+1)*0x400)
  u.mem_write(base+0x2000+i*0x400,bytes(obj))
 u.mem_write(0x5cb2ec,w(base+0x2000));u.mem_write(0x5cae44,w(base+0x2c00));trace.clear()
 u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(0x46a280,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop;result=u.reg_read(UC_X86_REG_EAX)&255;occupied+=result;wakes+=len(trace)
 x.mem_write(base,command);x.mem_write(base+0x1000,w(0xa5a5a5a5));native.clear()
 x.mem_write(stack,w(stop,base,base+68,3,base+128,2,callback,0,base+0x1000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert struct.unpack('<I',x.mem_read(base+0x1000,4))[0]==result and native==trace,(case,result,trace,native)
 h=0
 for handle in trace:h=(h*31+handle)&0xffffffff
 expected.extend(w(0,result,len(trace),h))
pc=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--trigger-occupancy'],input=commands);assert pc==expected
report=dict(result='PASS',cases=2048,occupied=occupied,wake_requests=wakes,scope='Original46a280 actor/item scan with real exclusion/shape/distance helpers. Source lookup supplied and item wake effects recorded. Exact PC/NXDK boolean and ordered wakes; box/sphere/unknown shape, signed radius, boundaries, flagged actors, actor early return, multiple items. Hold-open mode gates fixed eligible; no live ownership or reversal.')
(root/'artifacts/trigger-occupancy-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
