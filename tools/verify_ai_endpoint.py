"""Original4ce800/4ce860 endpoint connection order versus bounded PC/NXDK lists."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
B=0x30000000;Q=B+0x800;LISTS=[B+0x28,B+0x128,B+0x228,B+0x1000];ARRAYS=[B+0x2000+i*0x40 for i in range(4)];STACK=B+0xe000;STOP=B+0xf000;OUT=B+0x3000
r=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(B,0x10000);return m
u=load(exe);x=load(root/'build/xbox/main.exe');maps=(root/'build/xbox/main.map').read_text();entries=[int(re.search(r'\s_rf_entity_navigation_connect_'+name+r'\s+([0-9a-fA-F]+)',maps)[1],16) for name in ('start','goal')]
remove_entry=int(re.search(r"\s_rf_entity_navigation_disconnect_goal\s+([0-9a-fA-F]+)",maps)[1],16)
wire=[];calls=0;failure=False;CB=STOP+0x100

def ret(m,value=0,pop=0):
 sp=m.reg_read(UC_X86_REG_ESP);m.reg_write(UC_X86_REG_EAX,value&0xffffffff);m.reg_write(UC_X86_REG_EIP,r(m,sp));m.reg_write(UC_X86_REG_ESP,sp+4+pop)
def hook(m,address,size,context):
 global calls
 sp=m.reg_read(UC_X86_REG_ESP)
 if m is u:
  if address==0x45ec40:
   this=m.reg_read(UC_X86_REG_ECX);i=LISTS.index(this);n=r(m,this);assert n<8;m.mem_write(ARRAYS[i]+4*n,w(r(m,sp+4)));m.mem_write(this,w(n+1));ret(m,0,4)
  elif address==0x4ce570:
   assert r(m,sp+4)==Q and r(m,sp+8)==B+12 and r(m,sp+12)==77;calls+=1;ret(m,B+0x400 if wire[4] else 0,12)
 elif address==CB:
  assert r(m,sp+8)==B+12 and r(m,sp+12)==77;calls+=1
  if not failure:m.mem_write(r(m,sp+16),w(B+0x400 if wire[4] else 0))
  ret(m,-1 if failure else 0)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
def data(m):return [r(m,l) for l in LISTS]+[r(m,a+4*j) for a in ARRAYS for j in range(8)]
def setup(m):
 for i,(l,a) in enumerate(zip(LISTS,ARRAYS)):
  m.mem_write(l,w(wire[5+i],8,a) if m is u else w(a,wire[5+i],8));m.mem_write(a,w(*[100+i*16+j for j in range(8)]))
 m.mem_write(B+12,w(0x3f800000,0x40000000,0x40400000));m.mem_write(OUT,w(99))
def run(m,entry,args,pop=0):
 m.mem_write(STACK,w(STOP,*args));m.reg_write(UC_X86_REG_ESP,STACK);m.reg_write(UC_X86_REG_ECX,LISTS[3]);m.emu_start(entry,STOP,count=10000);assert m.reg_read(UC_X86_REG_EIP)==STOP and m.reg_read(UC_X86_REG_ESP)==STACK+4+pop;return m.reg_read(UC_X86_REG_EAX)
def shared():
 args=[LISTS[0],B+0x100 if wire[1] else 0,(B+0x100 if wire[3] else B+0x200) if wire[2] else 0,B+12,77,CB,0,OUT] if not wire[0] else [LISTS[3],LISTS[1] if wire[1] else 0,(LISTS[1] if wire[3] else LISTS[2]) if wire[2] else 0,B+0x300,OUT]
 return run(x,entries[0 if not wire[0] else 1],args)
rng=random.Random(0x4ce800);commands=[];expected=[];successes=[0,0];cleanups=0
for case in range(1536):
 wire=[case%3,(case//3)&1,(case//6)&1,(case//12)&1,(case//24)&1]+[rng.randrange(5) for i in range(4)]
 setup(u);u.mem_write(Q,w(B,B+0x100 if wire[1] else 0,(B+0x100 if wire[3] else B+0x200) if wire[2] else 0,B+0x300,B+0x100 if wire[1] else 0,(B+0x100 if wire[3] else B+0x200) if wire[2] else 0));calls=0
 result=run(u,0x4ce800 if not wire[0] else 0x4ce860,[Q,77] if not wire[0] else [Q],8 if not wire[0] else 4)&255;successes[int(wire[0]!=0)]+=result
 if wire[0]==2 and result:
  cleanups+=1
  for l in [LISTS[3],LISTS[1]]+([LISTS[1] if wire[3] else LISTS[2]] if wire[2] else []):
   u.mem_write(STACK,w(STOP,r(u,l)-1));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ECX,l);u.emu_start(0x4ce390,STOP,count=1000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 output=w(0,result,calls,*data(u));commands.append(w(*wire));expected.append(output);setup(x);calls=0;status=shared();
 if wire[0]==2 and result:assert run(x,remove_entry,[LISTS[3],LISTS[1],(LISTS[1] if wire[3] else LISTS[2]) if wire[2] else 0,B+0x300])==0
 actual=w(status,r(x,OUT),calls,*[r(x,l+4) for l in LISTS],*[r(x,a+4*j) for a in ARRAYS for j in range(8)]);assert actual==output,('NXDK',case)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-endpoint'],input=b''.join(commands));assert actual==b''.join(expected),'PC'
# Every participating goal list is capacity checked before any edge is published.
for which in (1,2,3):
 wire=[1,1,1,0,0,0,0,0,0];setup(x);x.mem_write(LISTS[which]+8,w(0));before=b''.join(bytes(x.mem_read(a,32)) for a in ARRAYS);assert shared()==0xfffffffc and r(x,OUT)==99 and all(r(x,l+4)==0 for l in LISTS) and b''.join(bytes(x.mem_read(a,32)) for a in ARRAYS)==before
wire=[1,1,1,1,0,0,0,0,0];setup(x);x.mem_write(LISTS[1]+8,w(1));assert shared()==0xfffffffc and r(x,LISTS[1]+4)==0 and r(x,LISTS[3]+4)==0
wire=[0,0,1,0,1,0,0,0,0];setup(x);failure=True;assert shared()==0xffffffff and r(x,OUT)==99 and r(x,LISTS[0]+4)==0;failure=False
wire=[0,1,1,0,0,0,0,0,0];setup(x);x.mem_write(LISTS[0]+8,w(1));assert shared()==0xfffffffc and r(x,OUT)==99 and r(x,LISTS[0]+4)==0
for bad in range(2):
 wire=[1,1,1,1,0,0,0,0,0];setup(x);assert shared()==0
 if bad==0:x.mem_write(ARRAYS[3],w(99))
 else:x.mem_write(LISTS[1]+4,w(1))
 before=[r(x,l+4) for l in LISTS];assert run(x,remove_entry,[LISTS[3],LISTS[1],LISTS[1],B+0x300])!=0 and [r(x,l+4) for l in LISTS]==before
report=dict(result='PASS',original_pc_nxdk_cases=1536,start_goal_successes=successes,compiled_guards=8,cleanup_cases=cleanups,scope='Full4ce800/4ce860 and actual4ce390 cleanup ordering with bounded append and nearest-selection boundaries supplied. PC/NXDK exact lists, stale backing slots, return and nearest-call count. Shared all-capacity preflight and aliased adjacency handling. No nearest selector or search wrapper claim.')
(root/'artifacts/ai-endpoint.json').write_text(json.dumps(report,indent=2));print(report)
