"""Full414e00 +415280 versus shared PC/NXDK; explicit cached-query correction."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;G=B;CAM=B+0x1000;M=B+0x2000;A=B+0x4000;PLAYER=B+0x6000;LIST=B+0x8000;BE=LIST+0x100;OUT=LIST+0x200;S=B+0x1e000;STOP=S+0x1000;CB=B+0xd000
MOV=[M,M+0x1000];ACT=[A,A+0x1000];basis=(0,0,1,0,1,0,-1,0,0);identity=(1,0,0,0,1,0,0,0,1)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x20000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_glare_visibility_search\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
callbacks=[CB+16*i for i in range(7)];original=[0x40a0e0,0x414ea1,0x40a490,0x4290d0,0x426fc0,0x4df1c0,0x5031f0]
traces={u:[],x:[]};snapshots=[];cfg=[];corrections=0
r=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
def hook(cpu,address,size,context):
 global corrections
 shared=cpu is x;op=(callbacks if shared else original).index(address);sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(cpu,sp+4+4*i);result=0;pop=0;payload=bytes(84)
 if not shared and op==1:
  token=r(cpu,G+0x294)
  if not token:return
 elif op in (0,1):token=arg(1 if shared else 0)
 elif op in (2,3,4):token=r(cpu,arg(1)) if shared else (cpu.reg_read(UC_X86_REG_ECX) if op==2 else arg(0))
 elif op==5:
  token=arg(1) if shared else cpu.reg_read(UC_X86_REG_ECX);q=arg(2 if shared else 0);assert arg(4 if shared else 2)==1;payload=bytes(cpu.mem_read(q,84))
  if not shared and token!=99 and not any(rw[:8]==w(5,99) for rw in traces[cpu]):
   # Only these original stack-derived fields intentionally differ in the port.
   assert payload[4:52]==b'\xa5'*48 and payload[76:84]==b'\xa5'*8
   payload=payload[:4]+f(0,0,0,*identity)+payload[52:76]+f(0)+w(0x85 if cfg[22]&255 else 5);corrections+=1
 else:
  token=r(cpu,arg(1)) if shared else ACT[arg(0)-301];q=arg(2 if shared else 1);assert arg(4 if shared else 3)==1;payload=bytes(cpu.mem_read(q,80))+bytes(4)
 if not shared and op==4:assert token==73;token=PLAYER
 traces[cpu].append(w(op,token)+payload)
 if not shared:snapshots.append(bytes(cpu.mem_read(G+0x290,12)))
 fail=shared and cfg[23]==len(traces[cpu])
 if shared:assert arg(0)==77
 if not fail:
  if op==0:
   result=(A+(token-41)*(96 if shared else 0x1000)) if token in (41,42) else 0
   if shared:cpu.mem_write(arg(2),w(result));result=0
  elif op==1:
   if not shared:return
   cpu.mem_write(arg(2),w(M+MOV.index(token)*96 if token in MOV else 0))
  elif op in (2,3):
   value=(7 if op==2 else cfg[19]) if token==PLAYER else cfg[(15 if op==2 else 17)+ACT.index(token)]
   if shared:cpu.mem_write(arg(2),w(value))
   else:result=value
  elif op==4:
   value=A+(cfg[20]-1)*(96 if shared else 0x1000) if cfg[20] else 0
   if shared:cpu.mem_write(arg(2),w(value))
   else:result=value
  elif op==5:
   value=cfg[9 if token==99 else 7+token-101];cpu.mem_write(arg(3 if shared else 1),w(value)+f(1)+bytes(24)+w(0,0x1234))
  elif op==6:
   value=cfg[5+ACT.index(token)]
   if shared:cpu.mem_write(arg(5),w(value))
   else:result=value
 if not shared and op==5:pop=12
 cpu.reg_write(UC_X86_REG_EAX,0xffffffff if fail else result);cpu.reg_write(UC_X86_REG_EIP,r(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
for cpu,addresses in ((u,original),(x,callbacks)):
 for a in addresses:cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def run(cpu,fn,args):
 cpu.mem_write(S,w(STOP,*args));cpu.reg_write(UC_X86_REG_ESP,S);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(fn,STOP,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==STOP;return cpu.reg_read(UC_X86_REG_EAX)
rng=random.Random(414001);commands=bytearray();answers=bytearray();errors=0;error_operations=set();visible_counts=[0,0];query_counts=[0,0]
for case in range(1537):
 cfg=[rng.randrange(3),rng.randrange(3),rng.choice((0xffffffff,41,42,999)),rng.choice((0,*MOV)),rng.choice((0,0x4567)),*[rng.choice((0,1,256,257)) for _ in range(2)],*[rng.choice((-1,0,1,2)) for _ in range(2)],rng.choice((-1,0,1)),*[rng.choice((0,16)) for _ in range(2)],*[rng.randrange(3)==0 for _ in range(2)],rng.randrange(2),*[rng.choice((7,8)) for _ in range(2)],*[rng.choice((0,1,256,257)) for _ in range(2)],rng.choice((0,1,2,257)),rng.randrange(3),rng.choice((41,42,999)),rng.choice((0,1,255,256)),rng.randrange(1,13) if case>=1024 else 0,*[rng.choice((0,16,0xffffffff)) for _ in range(2)],*[rng.randrange(3)==0 for _ in range(2)],301 if case%5 else 0,302 if case%7 else 0];assert len(cfg)==30
 if case==1536:
  for index,value in ((0,0),(1,1),(2,0xffffffff),(3,0),(9,0),(14,1),(15,7),(17,0),(19,1),(21,999),(23,6)):cfg[index]=value
 basis=(identity,(0,0,1,0,1,0,-1,0,0),(.36,.48,.8,-.8,.6,0,-.48,-.64,.6))[cfg[19]%3]
 traces[u]=[];traces[x]=[];snapshots=[];u.mem_write(S-0x1000,b'\xa5'*0x1000)
 raw=bytearray(b'\xa5'*768);raw[0x30:0x34]=w(cfg[21]);raw[0x3c:0x48]=f(0,0,4);raw[0x290:0x29c]=w(*cfg[2:5]);u.mem_write(G,bytes(raw))
 shared=bytearray(528);shared[12:24]=w(*cfg[2:5]);shared[128:132]=w(cfg[21]);shared[152:164]=f(0,0,4);x.mem_write(G,bytes(shared));before={u:{},x:{}}
 for i in range(4):
  n=i%2;flags=cfg[(10 if i<2 else 24)+n];outside=cfg[(12 if i<2 else 26)+n];model=cfg[28+n] if i>=2 else 0;token=B+0x2000+i*0x1000;bounds=(20,20,20,30,30,30) if outside else (-10,-10,-10,10,10,10)
  data=bytearray(b'\xa5'*768);data[0x2c:0x30]=w(41+n);data[0x3c:0x48]=f(1,2,3);data[0x48:0x6c]=f(*basis);data[0x7c:0x84]=w(flags,model);data[0x190:0x1a8]=f(*bounds)
  data[0x28c:0x290]=w(token+0x1000 if n+1<cfg[0 if i<2 else 1] else (0x64e6e0 if i<2 else 0x5cb060));data[0x294:0x298]=w(101+n);u.mem_write(token,bytes(data));before[u][token]=data
  obj=w(token,flags)+f(0)+w(model)+f(1,2,3,*basis,*bounds)+w(41+n,101+n);a=(M if i<2 else A)+n*96;x.mem_write(a,obj);before[x][a]=obj
 for cpu in (u,x):cpu.mem_write(CAM,f(0,0,-4))
 x.mem_write(PLAYER,w(PLAYER)+bytes(92));x.mem_write(LIST,w(M,cfg[0],A,cfg[1]));x.mem_write(BE,w(*callbacks,77));x.mem_write(OUT,w(0xa5a5a5a5))
 u.mem_write(PLAYER+0x200,w(73));u.mem_write(0x5cb054,w(PLAYER if cfg[14] else 0));u.mem_write(0x64e96c,w(M if cfg[0] else 0x64e6e0));u.mem_write(0x5cb2ec,w(A if cfg[1] else 0x5cb060));u.mem_write(0x6460e8,w(99));u.mem_write(0x5cab9d,bytes([cfg[22]&255]))
 original_visible=run(u,0x414e00,(G,CAM))&255;status=run(x,entry,(G,CAM,LIST,LIST+8,PLAYER if cfg[14] else 0,99,cfg[22],BE,OUT));visible=r(x,OUT)
 failed=cfg[23] and cfg[23]<=len(traces[u]);expected_trace=traces[u][:cfg[23]] if failed else traces[u]
 assert traces[x]==expected_trace,(case,[(struct.unpack('<2I',t[:8]),t[8:].hex()) for t in traces[x]],[(struct.unpack('<2I',t[:8]),t[8:].hex()) for t in expected_trace])
 assert status==(0xffffffff if failed else 0) and visible==(0xa5a5a5a5 if failed else original_visible),(case,status,visible,original_visible)
 cache=snapshots[cfg[23]-1] if failed else bytes(u.mem_read(G+0x290,12));assert bytes(x.mem_read(G+12,12))==cache,(case,'cache')
 raw[0x290:0x29c]=u.mem_read(G+0x290,12);shared[12:24]=cache;assert bytes(u.mem_read(G,768))==raw and bytes(x.mem_read(G,528))==shared
 for cpu in (u,x):
  for a,data in before[cpu].items():assert bytes(cpu.mem_read(a,len(data)))==data
 commands+=w(*cfg);answers+=w(status,visible)+cache+w(len(traces[x]))+b''.join(traces[x]);errors+=bool(failed);visible_counts[original_visible]+=1
 if failed:error_operations.add(struct.unpack('<I',traces[x][-1][:4])[0])
 for t in traces[x]:
  op=struct.unpack('<I',t[:4])[0]
  if op in (5,6):query_counts[op-5]+=1
pc=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--glare-search'],input=commands);assert pc==answers,'PC/NXDK mismatch'
assert errors and corrections and all(query_counts) and all(visible_counts)
assert error_operations==set(range(7)),error_operations
report=dict(result='PASS',cases=1537,callback_errors=errors,error_operations=sorted(error_operations),cached_query_corrections=corrections,original_outcomes=visible_counts,solid_model_queries=query_counts,original_sha256=sha,scope='Full original414e00 plus actual415280/AABB/math/constructors versus PC and compiled NXDK. Only lookup/room/state/association/solid-query/model backends supplied. Ordered callback query bytes, cache updates, all other owner bytes; failure prefixes preserve output and completed cache invalidations. Cached-solid original uninitialized origin/basis/radius/flags normalized to explicit local-ray defaults in comparison; that deliberate correction is checked separately from untouched query bytes. No native scene/render binding claim.')
(root/'artifacts/glare-search-shared.json').write_text(json.dumps(report,indent=2));print(report)
