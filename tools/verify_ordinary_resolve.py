"""Compare original49fe40 ordinary orchestration with shared PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe')



from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX
entry=int(re.search(r'\s_rf_ordinary_motion_resolve\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
mapping=[(0,0x88,12),(12,0x98,236),(248,0x190,32),(280,0x1c0,16),(296,0x1e4,4),(300,0x1ec,8),(308,0x870,48),(356,0x1434,24),(380,0x864,12),(392,0x48,36),(428,0x7e0,36)]
decision=mutation=fail=trace=0
snapshot=None
def word(cpu,at):return struct.unpack('<I',bytes(cpu.mem_read(at,4)))[0]
def hook(cpu,at,size,original):
 global trace,snapshot
 addresses={0x427550:1,0x49d7e0:2} if original else {B+0x8000:1,B+0x8100:2}
 if at not in addresses:return
 event=addresses[at];trace=trace*10+event
 flags=B+(0x1a8 if original else 272);time=B+(0x1b0 if original else 464)
 if mutation:
  cpu.mem_write(flags,w(word(cpu,flags)^(0x20000080 if event==1 else 0x10000008)))
  cpu.mem_write(B+((0x144 if event==1 else 0x148) if original else (184 if event==1 else 188)),f(7 if event==1 else -9))
  cpu.mem_write(time,f(17 if event==1 else 19))
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if original:
  assert (cpu.reg_read(UC_X86_REG_ECX) if event==1 else word(cpu,sp+4))==B
  cpu.reg_write(UC_X86_REG_EAX,decision if event==1 else 0)
 else:
  assert word(cpu,sp+8)==B and word(cpu,sp+12)==B+464
  if event==1:cpu.mem_write(word(cpu,sp+16),w(decision))
  cpu.reg_write(UC_X86_REG_EAX,0xffffffff if fail==event else 0)
 if not original and fail==event:snapshot=bytes(cpu.mem_read(B,468))
 cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for address in (0x427550,0x49d7e0):u.hook_add(UC_HOOK_CODE,hook,True,begin=address,end=address)
for address in (B+0x8000,B+0x8100):x.hook_add(UC_HOOK_CODE,hook,False,begin=address,end=address)
rng=random.Random(0x49fe40);commands=[];expected=[];counts={}
for n in range(2048):
 state=bytearray(f(*[rng.uniform(-2,2) for _ in range(116)]));flags=[0,0x400000,0x20000000,0x20400000][n%4]
 state[272:276]=w(flags);state[292:296]=f([0,.25,.75,1,1.1][n%5]);state[244:248]=f(1)
 state[380:392]=f(rng.uniform(-1,1),rng.uniform(-5,5),rng.uniform(-5,5));state[356:380]=f(*([-2]*3+[2]*3))
 time=[0,1/60,.1][n%3];refs=[rng.randrange(4) for _ in range(3)];classflags=[0,0x200,0x1000,0x400000][n%4]
 decision=[0,1,2,3,0x102][(n//5)%5];mutation=n%2;fail=0;dt=1/60
 command=bytes(state)+f(time)+w(*refs,classflags)+f(dt)+w(decision,mutation,fail);assert len(command)==500;commands.append(command)
 u.mem_write(B,bytes(0x5000));u.mem_write(B+0x858,w(B+0x4000));u.mem_write(B+0x4014,w(*refs));u.mem_write(B+0x294,w(B+0x3000));u.mem_write(B+0x3724,w(classflags));u.mem_write(B+0x1b0,f(time));u.mem_write(0x5a4014,f(dt))
 for offset,at,size in mapping:u.mem_write(B+at,bytes(state[offset:offset+size]))
 u.mem_write(STACK,w(STOP,B));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x37f);trace=0
 u.emu_start(0x49fe40,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 result=bytearray(state)
 for offset,at,size in mapping:result[offset:offset+size]=u.mem_read(B+at,size)
 expected.append(w(0)+bytes(result)+bytes(u.mem_read(B+0x1b0,4))+w(trace));counts[trace]=counts.get(trace,0)+1
 x.mem_write(B,command);x.mem_write(B+0x6000,w(0,B+0x8000,B+0x8100));x.mem_write(STACK,w(STOP,B,B+464,classflags,struct.unpack('<I',f(dt))[0],B+468,B+0x6000));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);trace=0
 x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,468))+w(trace)
 assert got==expected[-1],('NXDK',n,[(i,a,b) for i,(a,b) in enumerate(zip(got,expected[-1])) if a!=b][:12])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_eye_probe.exe'),'--ordinary-resolve'],input=b''.join(commands));assert len(actual)==476*len(commands)
max_error=0;different=0
for n,result in enumerate(expected):
 got=actual[n*476:(n+1)*476];assert got[:4]==result[:4] and got[468:]==result[468:]
 for i in range(116):
  a=got[4+i*4:8+i*4];b=result[4+i*4:8+i*4]
  if 52<=i*4<88 or 112<=i*4<184 or 392<=i*4<464:
   af,bf=struct.unpack('<f',a)[0],struct.unpack('<f',b)[0];err=abs(af-bf);max_error=max(max_error,err);assert err<=1e-15,('PC matrix',n,i,af,bf)
  else:assert a==b,('PC state',n,i,a.hex(),b.hex())
 different+=got!=result
failure_commands=[];failure_expected=[]
for failure in (1,2):
 for mut in (0,1):
  command=bytearray(commands[0]);command[272:276]=w(0);command[292:296]=f(.5);command[464:468]=f(.1);command[488:500]=w(2,mut,failure)
  decision=2;mutation=mut;fail=failure;trace=0;snapshot=None
  x.mem_write(B,bytes(command));x.mem_write(B+0x6000,w(0,B+0x8000,B+0x8100));x.mem_write(STACK,w(STOP,B,B+464,word(x,B+480),word(x,B+484),B+468,B+0x6000));x.reg_write(UC_X86_REG_ESP,STACK)
  x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP and snapshot is not None
  assert x.reg_read(UC_X86_REG_EAX)==0xffffffff and bytes(x.mem_read(B,468))==snapshot
  failure_commands.append(bytes(command));failure_expected.append(w(0xffffffff)+snapshot+w(trace))
failed=subprocess.check_output([str(root/'build/pc/Release/rf_eye_probe.exe'),'--ordinary-resolve'],input=b''.join(failure_commands))
assert failed==b''.join(failure_expected)
report=dict(result='PASS',callback_failure_cases=len(failure_commands),original_nxdk_exact_cases=len(commands),pc_cases=len(commands),callback_traces=counts,pc_matrix_differing_cases=different,pc_matrix_max_absolute_error=max_error,scope='Full49fe40 ordinary branch with explicit427550/49d7e0 callback boundaries; all numeric callees original. Exact NXDK464 state bytes, remaining time and callback order. PC matrix tolerance1e-15. Callback internals, rigid branch and live scheduling excluded.')
(root/'artifacts/ordinary-resolve.json').write_text(json.dumps(report,indent=2));print(report)
