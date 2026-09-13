"""Full original4141a0 nonactor volume path versus shared PC/NXDK."""
import runpy,struct,re,random,subprocess,json,math
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,B,S,STOP=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(int(i)&0xffffffff for i in v))
f=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
r=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
entry=int(re.search(r'_rf_glare_volume_render\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
DEF=B+0x1000;FRAME=B+0x2000;BE=B+0x3000;PARENT=B+0x4000;ROOM=B+0x5000;VIEW=B+0x6000;CB=STOP+16
original=[0x40a0e0,0x426fc0,0x431950,0x50cf80,0x50d060,0x515c00]
callbacks=[CB+i*16 for i in range(7)];traces={u:[],x:[]};case=None
def hook(m,a,size,context):
 sp=m.reg_read(UC_X86_REG_ESP);arg=lambda i:r(m,sp+4+i*4)
 if m is u and a in (0x40d760,0x40d780):
  m.mem_write(arg(0),w(*case[8:11]) if a==0x40d760 else w(*map(f,[1,0,0,0,1,0,0,0,1])))
  m.reg_write(UC_X86_REG_EIP,r(m,sp));m.reg_write(UC_X86_REG_ESP,sp+4);return
 addresses=original if m is u else callbacks
 if a not in addresses:return
 op=addresses.index(a)+(1 if m is u else 0);shift=0 if m is u else 1;result=0
 assert op!=0 # No special owner in this original fixture.
 if m is x:assert arg(0)==77
 if op==1:
  assert arg(shift)==32;payload=[32]
  if m is u:result=PARENT if case[0] else 0
  else:m.mem_write(arg(2),w(case[0]!=2))
 elif op==2:
  if m is u:assert arg(0)==32
  else:assert arg(1)==B
  payload=[32]
 elif op==3:payload=[arg(shift)]
 elif op==4:payload=[arg(shift+i) for i in range(4)]
 elif op==5:payload=[arg(shift),arg(shift+1)]
 else:
  assert bytes(m.mem_read(arg(shift+1),12))==w(*case[2:5])
  payload=list(struct.unpack('<3I',m.mem_read(arg(shift),12)))+[arg(shift+2),arg(shift+3)]
 traces[m].append([op,*payload,*([0]*(7-len(payload)))])
 if m is x and case[14]==len(traces[m]):result=-1
 m.reg_write(UC_X86_REG_EAX,result&0xffffffff);m.reg_write(UC_X86_REG_EIP,r(m,sp));m.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(4141);cases=[]
for n in range(256):
 direction=[rng.uniform(-1,1) for _ in range(3)];length=math.sqrt(sum(v*v for v in direction))
 direction=[v/length*.999 for v in direction] if n%2 else [0,0,-1]
 cases.append([n%4,(-1,0,77,77)[(n//4)%4],*map(f,[rng.uniform(-5,5),rng.uniform(-5,5),8,*direction,rng.uniform(-2,2),rng.uniform(-2,2),0,rng.choice([0,45,90,180]),rng.uniform(.3,7),rng.uniform(1,8)]),0])
for error in range(1,8):cases.append([3,77,*map(f,[0,0,8,0,0,1,0,0,0,45,2,8]),error])
actual=subprocess.check_output([str(c['probe']),'--volume-frame'],input=b''.join(w(*v) for v in cases));assert len(actual)==268*len(cases)
for n,case in enumerate(cases):
 traces[u]=[];traces[x]=[]
 u.mem_write(B,bytes(768));u.mem_write(B+0x30,w(32));u.mem_write(B+0x3c,w(*case[2:5]));u.mem_write(B+0x60,w(*case[5:8]));u.mem_write(B+0x78,w(f(13)));u.mem_write(B+0x2ac,w(DEF))
 u.mem_write(DEF,bytes(128));u.mem_write(DEF+0x10,w(case[11]));u.mem_write(DEF+0x24,w(case[1],case[12],case[13]));u.mem_write(PARENT,w(ROOM if case[0]>=2 else 0));u.mem_write(ROOM+0x161,bytes([case[0]==3]));u.mem_write(0x7c763c,w(VIEW));u.mem_write(VIEW+0xc4,w(1));u.mem_write(0x1775b30,w(0x06110c42))
 u.mem_write(S,w(STOP,B));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x4141a0,STOP,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 trace=traces[u];radius=bytes(u.mem_read(B+0x78,4));error=case[14]
 if error:
  assert len(trace)==7
  trace=trace[:error]
  if error<3:radius=w(f(13))
  if 3<error<7:trace=trace+[[3,0,0,0,0,0,0,0]]
 expected=w(-1 if error else 0)+radius+w(len(trace))+b''.join(w(*row) for row in trace)+bytes(32*(8-len(trace)))
 x.mem_write(B,bytes(528));x.mem_write(B+128,w(32));x.mem_write(B+144,w(f(13)));x.mem_write(B+152,w(*case[2:5]));x.mem_write(B+188,w(*case[5:8]))
 x.mem_write(DEF,bytes(304));x.mem_write(DEF+268,w(case[11]));x.mem_write(DEF+288,w(case[12],case[13]));x.mem_write(FRAME,w(*case[8:11],case[1],0x06110c42));x.mem_write(BE,w(*callbacks,77))
 x.mem_write(S,w(STOP,B,DEF,FRAME,BE));x.reg_write(UC_X86_REG_ESP,S);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+144,4))+w(len(traces[x]))+b''.join(w(*row) for row in traces[x])+bytes(32*(8-len(traces[x])))
 assert got==expected and actual[n*268:(n+1)*268]==expected,(n,case,traces[u],traces[x],got.hex(),expected.hex(),actual[n*268:(n+1)*268].hex())
report=dict(result='PASS',original_cases=256,callback_failure_cases=7,scope='Full4141a0 with actual camera/angular math and beam endpoint arithmetic. Parent lookup, camera, null actor query and graphics supplied. Exact PC/NXDK radius and ordered graphics calls. Error cleanup is explicit shared policy. Special-owner and actor aim/RNG branches remain service boundaries; no scene/native claim.')
(root/'artifacts/volume-frame.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
