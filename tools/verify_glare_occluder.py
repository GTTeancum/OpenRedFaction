"""Full original415280 occluder filter/query versus PC and compiled NXDK."""
import hashlib,json,re,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(0x30000000,0x10000);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_glare_occluder_test\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
B=0x30000000;O=B;G=B+0x1000;CAM=B+0x2000;BE=CAM+0x100;OUT=BE+0x100;S=B+0xe000;STOP=B+0xf000;CB=STOP+0x100
calls={u:[],x:[]};value=0;error=0
def hook(cpu,address,length,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);r=lambda a:struct.unpack('<I',cpu.mem_read(a,4))[0];arg=lambda n:r(sp+4+n*4)
 if cpu is u:
  assert arg(0)==123 and arg(3)==1;query=arg(1);result=value
 else:
  assert arg(0)==77 and arg(1)==O and arg(4)==1;query=arg(2);result=0xffffffff if error else 0
  if not error:cpu.mem_write(arg(5),w(value))
 calls[cpu].append(bytes(cpu.mem_read(query,80)));cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook,begin=0x5031f0,end=0x5031f0);x.hook_add(UC_HOOK_CODE,hook,begin=CB,end=CB)
def run(cpu,fn,args):
 cpu.mem_write(S,w(STOP,*args));cpu.reg_write(UC_X86_REG_ESP,S);cpu.emu_start(fn,STOP,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==STOP;return cpu.reg_read(UC_X86_REG_EAX)
rng=random.Random(415280);commands=bytearray();expected=bytearray();hits=0
for case in range(420):
 calls[u]=[];calls[x]=[];value=(0,1,255,256,257,0xffffffff)[case%6];error=int(case>=408)
 mode=case%7 if not error else 0;flags=0 if mode==1 else 0x10;model=0 if mode==2 else 123;excluded=O if mode==3 else 0;parent=42 if mode==4 else 41
 position=[rng.uniform(-100,100) for _ in range(3)];matrix=[rng.uniform(-2,2) for _ in range(9)]
 lo=(-1,-1,-1);hi=(1,1,1)
 ends=((0,0,0,2,0,0),(-2,0,0,2,0,0),(2,2,2,3,3,3),(-2,1,0,2,1,0),(0,0,0,0,0,0),(-3,-2,0,3,2,0))[case//7%6]
 obj=w(O,flags)+f(1)+w(model)+f(*position,*matrix,*lo,*hi);assert len(obj)==88
 orig=bytearray(b'\xa5'*768);orig[0x2c:0x30]=w(42);orig[0x3c:0x48]=f(*position);orig[0x48:0x6c]=f(*matrix);orig[0x7c:0x84]=w(flags,model);orig[0x190:0x1a8]=f(*lo,*hi)
 glare=bytearray(b'\xa5'*768);glare[0x30:0x34]=w(parent);glare[0x3c:0x48]=f(*ends[:3]);u.mem_write(O,bytes(orig));u.mem_write(G,bytes(glare));u.mem_write(CAM,f(*ends[3:]));u.mem_write(0x5cb054,w(excluded))
 shared=bytearray(528);shared[128:132]=w(parent);shared[152:164]=f(*ends[:3]);x.mem_write(O,obj);x.mem_write(G,bytes(shared));x.mem_write(CAM,f(*ends[3:]));x.mem_write(BE,w(CB,0,77));x.mem_write(OUT,w(0xa5a5a5a5))
 status=run(x,entry,(O,42,excluded,G,CAM,BE,OUT));blocked=struct.unpack('<I',x.mem_read(OUT,4))[0]
 if not error:
  original=run(u,0x415280,(O,G,CAM))&255
  assert status==0 and blocked==original,(case,status,blocked,original)
  assert calls[x]==calls[u],(case,[a.hex() for a in calls[x]],[a.hex() for a in calls[u]])
  assert bytes(u.mem_read(O,768))==orig and bytes(u.mem_read(G,768))==glare
 else:
  assert status==(0xffffffff if calls[x] else 0) and blocked==(0xa5a5a5a5 if calls[x] else 0)
 assert bytes(x.mem_read(O,88))==obj and bytes(x.mem_read(G,528))==shared
 commands+=obj+w(42,excluded,parent,value,error)+f(*ends)
 expected+=w(status,blocked,len(calls[x]))+(calls[x][0] if calls[x] else b'\xa5'*80);hits+=len(calls[x])
pc=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--glare-occluder'],input=commands);assert pc==expected,'PC/NXDK mismatch'
report=dict(result='PASS',cases=420,model_queries=hits,original_sha256=digest,scope='408 full original415280 comparisons, actual segment-box and query constructors; only model collision backend supplied. PC and compiled NXDK query bytes, filters, low-byte result, immutable owners, plus 12 callback-error cases. Cached world search and live glare rendering remain open.')
(root/'artifacts/glare-occluder.json').write_text(json.dumps(report,indent=2));print(report)
