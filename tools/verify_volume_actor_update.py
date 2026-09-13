"""Original full actor branch with real target-array selection and math/RNG."""
import runpy,struct,re,random,subprocess,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,B,S,STOP=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EDI
w=lambda *v:struct.pack('<'+'I'*len(v),*(int(v)&0xffffffff for v in v))
f=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
r=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
P=B+0x4000;T=B+0x6000;CLASS=B+0x8000;ARRAY=B+0x9000;NODES=B+0x9100;DEF=B+0x3000;CB=STOP+16
entry=int(re.search(r'_rf_glare_volume_actor_update\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
traces={u:[],x:[]};case=None;failure=0
def hook(m,a,size,ctx):
 if a!=(0x426fc0 if m is u else CB):return
 sp=m.reg_read(UC_X86_REG_ESP);handle=r(m,sp+(4 if m is u else 8));traces[m].append(handle)
 result=P if handle==32 and case[0] else T if handle==64 and case[6] else 0
 if m is x:
  assert r(m,sp+4)==77;m.mem_write(r(m,sp+12),w(result));result=0
  if failure==len(traces[m]):result=-1
 m.reg_write(UC_X86_REG_EAX,result&0xffffffff);m.reg_write(UC_X86_REG_EIP,r(m,sp));m.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(414307);cases=[]
for n in range(512):
 parent=1 if n%4==0 else rng.randrange(2);cls=0x800 if n%4==0 else rng.choice([0,0x800,0x801]);flags=0 if n%4==0 else rng.choice([0,1,2,3])
 targets=rng.choice([[-1,-1,-1],[-1,-1,64],[-1,99,64],[64,99,-1]])
 first=[rng.uniform(-2,2) for _ in range(3)] if n%17 else [0,0,0];second=[rng.uniform(-2,2) for _ in range(3)] if n%19 else [0,0,0]
 cases.append([parent,cls,flags,*targets,rng.randrange(2),rng.choice([0,8,9]),rng.randrange(2),*map(f,first+second+[rng.uniform(-100,100) for _ in range(3)]+[1,0,0,0,1,0,0,0,1]+[rng.uniform(.1,10),rng.uniform(.1,5)]),rng.randrange(2),rng.getrandbits(32)])
actual=subprocess.check_output([str(c['probe']),'--volume-actor-update'],input=b''.join(w(*row) for row in cases));assert len(actual)==32*len(cases)
for n,case in enumerate(cases):
 traces[u]=[];traces[x]=[];u.mem_write(B,bytes(0x300));u.mem_write(B+0x30,w(32));u.mem_write(B+0x2b4,w(case[2]));u.mem_write(DEF+0x28,w(case[28],case[27]));u.mem_write(c['thread']+0x14,w(case[30]));u.mem_write(S+0x60,bytes([case[29]]))
 u.mem_write(P,bytes(0x1500));u.mem_write(T,bytes(0x1500));u.mem_write(P+0x294,w(CLASS));u.mem_write(CLASS+0x724,w(case[1]));u.mem_write(P+0x3c,w(*case[15:27]));u.mem_write(P+0x714,w(*case[9:12]));u.mem_write(T+0x714,w(*case[12:15]));u.mem_write(T+0x7c,w(case[7]));u.mem_write(T+0x1430,w(case[8]));u.mem_write(P+0x8cc,w(3,3,ARRAY));u.mem_write(ARRAY,w(NODES,NODES+16,NODES+32))
 for i in range(3):u.mem_write(NODES+16*i,w(0,case[3+i]))
 u.reg_write(UC_X86_REG_ESI,DEF);u.reg_write(UC_X86_REG_EDI,B);u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x414307,0x41441e,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x41441e
 trace=traces[u];expected=bytes(4)+bytes(u.mem_read(c['thread']+0x14,4))+bytes(u.mem_read(S+8,4))+bytes(u.mem_read(S+16,4))+w(u.mem_read(S+0x60,1)[0],len(trace),*(trace+[0]*(2-len(trace))))
 x.mem_write(P,w(case[1],0,0,*case[15:27],*case[9:12],ARRAY,3));x.mem_write(T,w(0,case[7],case[8])+bytes(48)+w(*case[12:15])+bytes(8));x.mem_write(ARRAY,w(*case[3:6]));x.mem_write(B+0x2000,w(case[30],case[29]));x.mem_write(B+0x2010,bytes([0xa5])*8)
 x.mem_write(S,w(STOP,32,case[2],case[27],case[28],CB,77,B+0x2000,B+0x2010,B+0x2004));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 trace=traces[x];got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+0x2000,4))+bytes(x.mem_read(B+0x2010,8))+bytes(x.mem_read(B+0x2004,4))+w(len(trace),*(trace+[0]*(2-len(trace))))
 assert got==expected and actual[n*32:(n+1)*32]==expected,(n,case,got.hex(),expected.hex(),actual[n*32:(n+1)*32].hex())
case=[1,0x800,0,-1,-1,64,1,0,0]+list(map(f,[0,0,1,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,2,1]))+[1,123]
for guard in range(4):
 failure=guard+1 if guard<2 else 0;traces[x]=[]
 x.mem_write(P,w(case[1],0,0,*case[15:27],*case[9:12],0 if guard==3 else ARRAY,4097 if guard==2 else 3));x.mem_write(T,w(0,0,0)+bytes(48)+w(*case[12:15])+bytes(8));x.mem_write(ARRAY,w(-1,-1,64))
 x.mem_write(B+0x2000,w(123,1));x.mem_write(B+0x2010,bytes([0xa5])*8)
 x.mem_write(S,w(STOP,32,0,f(2),f(1),CB,77,B+0x2000,B+0x2010,B+0x2004));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)!=0
 assert bytes(x.mem_read(B+0x2000,8))==w(123,1) and bytes(x.mem_read(B+0x2010,8))==bytes([0xa5])*8
report=dict(result='PASS',cases=len(cases),nxdk_error_guards=4,scope='Original414307..41441e actor branch: actual40a210,4154d0,427da0 target array,42a8e0/4895d0 player predicate, aim math and CRT RNG. Only426fc0 actor lookup supplied. Exact PC/NXDK lookup order, dimensions, draw and RNG. Four compiled NXDK callback/array errors preserve state. Live retained actor714/8cc ownership not supplied.')
(root/'artifacts/volume-actor-update.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
