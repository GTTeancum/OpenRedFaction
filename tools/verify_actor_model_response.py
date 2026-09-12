"""Original49afe0 with real geometry/vector helpers; actor velocity lookup supplied."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_collision_actor_model_response\s+([0-9a-fA-F]+)',mapping)[1],16)
trace=[];hits=[];query_index=0;present=0
from unicorn.x86_const import UC_X86_REG_ECX
def hook(m,address,size,native):
 global query_index
 addresses=(b+0xd000,b+0xd100) if native else (0x426fc0,0x5031f0)
 if address not in addresses:return
 op=addresses.index(address);sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<5I',m.mem_read(sp,20));row=bytearray(96);row[:4]=w(op)
 if op==0:
  handle=args[2] if native else args[1];row[4:8]=w(handle);result=(b+0x6500 if native else b+0x6000) if present else 0
 else:
  model,query,out=args[2:5] if native else args[1:4]
  if not native:assert args[4]==0
  row[4:92]=w(model)+bytes(m.mem_read(query,80))+bytes(m.mem_read(out,4))
  result=struct.unpack('<I',hits[query_index][:4])[0];m.mem_write(out,hits[query_index][4:]);query_index+=1
 trace.append(bytes(row));m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,args[0])
u.hook_add(UC_HOOK_CODE,hook,False);x.hook_add(UC_HOOK_CODE,hook,True)
x.mem_write(b+0xd300,w(b+0xd000,b+0xd100,0));u.mem_write(0x17543e0,w(15))
def run(m,address,args):
 global query_index
 trace.clear();query_index=0;m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,stop,count=20000)
 assert m.reg_read(UC_X86_REG_EIP)==stop and m.reg_read(UC_X86_REG_FPCW)==0x27f
 return w(m.reg_read(UC_X86_REG_EAX)&255)+w(len(trace))+b''.join(trace)+bytes((5-len(trace))*96)
rng=random.Random(0x49afe0);commands=[];answers=[];counts=[0,0,0,0];lookups=0;immune_cases=0;immune_hits=0;immune_miss_writes=0
for case in range(8192):
 rows=[]
 for i in range(2):
  pos=[rng.randrange(-16,17)/4 for _ in range(3)];nxt=[p+rng.randrange(-16,17)/4 for p in pos]
  radii=[rng.choice((0,.25,.5,1,2)) for _ in range(4)];count=rng.choice((-1,0,1,2,4));flags=rng.choice((0,0x40000000,0xc0000020,0x400));time=rng.choice((0,.01,.1,.5,1,2))
  bounds=[p-2 for p in pos]+[p+2 for p in pos]
  if case<2048:
   pos=[0,0,0] if i==0 else [0,case%3,2.5];nxt=pos.copy();nxt[2]+= (0,1,2,-1)[case//2%4] if i==0 else (-4,-2,-1,0)[case//8%4]
   radii=[1,.5,.25,0];count=case//32%5;bounds=[-10,-10,-10,10,10,10];time=(0,.05,.1,.5,1,2)[case//160%6] if i==0 else (0,.05,.1,.5,1,2)[case//27%6]
  velocity=[rng.randrange(-32,33)/8 for _ in range(3)];mass=rng.choice((.5,1,2,10))
  contact=f([11,12,13,14,15,16,time])+w(81)+f([19,20,21,22])+w(83,84,85,86,87)
  rows.append(f(bounds+pos+nxt+velocity+[mass])+w(101+i,31+i,flags,count)+f(radii)+contact+f([.125,-.25,.5])+w(case//(i+1)%2))
 for i in range(2):
  rotations=([1,0,0,0,1,0,0,0,1],[0,0,1,0,1,0,-1,0,0],[-1,0,0,0,1,0,0,0,-1],[.6,0,.8,0,1,0,-.8,0,.6],[.36,.48,.8,-.8,.6,0,-.48,-.64,.6])
  matrix=rotations[case//4%len(rotations)];next_matrix=rotations[case//7%len(rotations)];extent=(case//3+i)%4
  centers=[rng.randrange(-4,5)/4 for _ in range(12)] if case>=1024 else [0]*12
  rows[i]+=f(list(matrix)+list(next_matrix)+[extent])+w((0,2,3)[case//(i+1)%3])+f(centers)
 model=0x12345678;use=case%3;present=case%4!=0;armor=(0,1,2,-1)[case//3%4];class_flags=0x02000000 if case%3 else 0;flags=0x20 if case%5==0 else 0
 rows[1]=rows[1][:256]+w(0)+rows[1][260:]
 if case%11==0:rows[0]=rows[0][:36]+rows[0][24:36]+rows[0][48:]
 if case<256:
  present=True;armor=1;class_flags=0x02000000;flags=0
  for i in range(2):
   row=bytearray(rows[i]);row[:24]=f([-10,-10,-10,10,10,10]);row[72:76]=w(0x40000020)
   row[24:48]=f([0,0,0,0,0,(0,.1,.5,1,2,4)[case%6]]) if i==0 else f([0,0,(0,.5,1,2,3)[case//6%5]]*2)
   row[252:256]=f([(0,.1,.5,1,2,4)[case//30%6]]);row[256:260]=w(2 if i==0 else 0);rows[i]=bytes(row)
 hits=[w(rng.choice((0,1,0x100,0x101)))+f([rng.choice((0,.01,.25,.5,1,2))]+[rng.randrange(-16,17)/8 for _ in range(6)])+w(0x98765432+j) for j in range(4)]
 commands.append(b''.join(rows)+w(model,use,present)+f([armor])+w(class_flags,flags)+b''.join(hits));before=[]
 u.mem_write(b+0x6038,f([armor]));u.mem_write(b+0x6294,w(b+0x7000));u.mem_write(b+0x7724,w(class_flags));u.mem_write(b+0x6814,w(flags))
 x.mem_write(b+0x6500,f([armor])+w(class_flags,flags))
 u.mem_write(0x7c6a6c,w(1));u.mem_write(0x7c5a58,bytes(12))
 for i,row in enumerate(rows):
  obj=b+i*0x2000;native=b+0x8000+i*0x100;spheres=b+0x4000+i*0x100
  for m in (u,x):
   m.mem_write(spheres,b''.join(row[260+j*12:272+j*12]+row[80+j*4:84+j*4]+bytes(8) for j in range(4)))
   m.mem_write(b+0x68a0+i*0x1000,row[164:176])
  u.mem_write(obj,b'\xa5'*0x2000)
  for dst,src,n in ((0x190,0,24),(0xe4,24,24),(0x144,48,12),(0x98,60,4),(0x2c,64,4),(0x1fc,68,4),(0x1a8,72,4),(0x184,76,4),(0x1b4,96,68)):u.mem_write(obj+dst,row[src:src+n])
  u.mem_write(obj+0xfc,row[180:216]);u.mem_write(obj+0x120,row[216:252]);u.mem_write(obj+0x180,row[252:256]);u.mem_write(obj+0x24,row[256:260])
  u.mem_write(obj+0x188,w(4,spheres));before.append(bytes(u.mem_read(obj,0x2000)))
  x.mem_write(native,row[:80]+w(spheres)+row[96:164]+row[180:260])
 u.mem_write(b+0x2000+0x294,w(b+0x5000));u.mem_write(b+0x51b4,w(use));u.mem_write(b+0x2000+0x80,w(model))
 before[1]=bytes(u.mem_read(b+0x2000,0x2000))
 expected=run(u,0x49afe0,[b,b+0x2000]);actual=run(x,entry,[b+0x8000,b+0x8100,model,use,b+0xd300]);lookups+=query_index
 for i,row in enumerate(rows):
  obj=b+i*0x2000;native=b+0x8000+i*0x100
  expected+=row[:72]+bytes(u.mem_read(obj+0x1a8,4))+row[76:80]+bytes(u.mem_read(obj+0x1b4,68))
  actual+=bytes(x.mem_read(native,80))+bytes(x.mem_read(native+84,68))
  after=bytes(u.mem_read(obj,0x2000));preserved=bytearray(before[i]);preserved[0x1a8:0x1ac]=after[0x1a8:0x1ac];preserved[0x1b4:0x1f8]=after[0x1b4:0x1f8];assert bytes(preserved)==after
 assert expected==actual,(case,commands[-1].hex(),expected.hex(),actual.hex())
 result=struct.unpack('<I',expected[:4])[0];mode=0 if not trace else 1 if not query_index else 2 if not result else 3;counts[mode]+=1
 if trace and present and armor>0 and class_flags&0x02000000 and not flags&0x20 and struct.unpack('<I',rows[0][256:260])[0]==2:
  immune_cases+=1;immune_hits+=result;immune_miss_writes+=not result and expected[568:580]!=rows[0][96:108]
 answers.append(expected)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--actor-model-response'],input=b''.join(commands))
for i,expected in enumerate(answers):assert actual[i*784:(i+1)*784]==expected,('PC',i,commands[i].hex(),expected.hex(),actual[i*784:(i+1)*784].hex())
assert all(counts) and immune_cases and immune_hits and immune_miss_writes
report=dict(result='PASS',cases=len(commands),outcomes=counts,queries=lookups,immunity_cases=immune_cases,immunity_hits=immune_hits,immunity_misses_writing_point=immune_miss_writes,original_sha256=sha,scope='Full original49afe0 orchestration with real immunity42cca0, segment506ae0 and all bounds/list/vector/transform/distance/class helpers. Only426fc0 lookup and5031f0 model geometry supplied. Exact PC/NXDK query data/time carry, contact fields, return and surrounding original bytes under027f. Missing/immune targets, zero-length projectile tests, mass/class gates, model hit/miss sequences and oblique transforms. Live ownership/model query implementation excluded.')
(root/'artifacts/actor-model-response.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
