"""Compare original49fe40 ordinary partial prefix with shared PC/NXDK."""
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


from unicorn.x86_const import UC_X86_REG_ESI
entry=int(re.search(r'\s_rf_ordinary_motion_partial\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
mapping=[(0,0x88,12),(12,0x98,236),(248,0x190,32),(280,0x1c0,16),(296,0x1e4,4),(300,0x1ec,8),(308,0x870,48),(356,0x1434,24),(380,0x864,12),(392,0x48,36),(428,0x7e0,36)]
rng=random.Random(0x49ffd2);commands=[];expected=[]
for n in range(2048):
 state=bytearray(f(*[rng.uniform(-2,2) for _ in range(116)]))
 state[272:276]=w(0x400000 if n%2 else 0);state[292:296]=f([0,.01,.25,.75,.999][n%5])
 state[380:392]=f(rng.uniform(-1.5,1.5),rng.uniform(-6,6),rng.uniform(-6,6))
 if n%8==0:state[100:112]=state[88:100]
 if n%8==1:state[100:112]=f(*[v+.001 for v in struct.unpack('<3f',state[88:100])])
 time=[0,1/60,.1,1][n%4];refs=[rng.randrange(4) for _ in range(3)]
 command=bytes(state)+f(time)+w(*refs)+f(-99);commands.append(command)
 u.mem_write(B,bytes(0x4000));u.mem_write(STACK,bytes(0x400));u.mem_write(B+0x858,w(B+0x3000));u.mem_write(B+0x3014,w(*refs));u.mem_write(B+0x1b0,f(time))
 for at,off,size in mapping:u.mem_write(B+off,bytes(state[at:at+size]))
 u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x49ffd2,0x4a011b,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x4a011b
 result=bytearray(state)
 for at,off,size in mapping:result[at:at+size]=u.mem_read(B+off,size)
 expected.append(w(0)+bytes(result)+bytes(u.mem_read(STACK+0x58,4)))
 x.mem_write(B,command);x.mem_write(STACK,w(STOP,B,struct.unpack_from('<I',command,464)[0],B+468,B+480));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,464))+bytes(x.mem_read(B+480,4))
 assert actual==expected[-1],('NXDK',n,[(i,a,b) for i,(a,b) in enumerate(zip(actual,expected[-1])) if a!=b][:12])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_eye_probe.exe'),'--ordinary-partial'],input=b''.join(commands));assert len(actual)==472*len(commands)
max_error=0;different=0
for n,result in enumerate(expected):
 got=actual[n*472:(n+1)*472];assert got[:4]==result[:4]
 for i in range(117):
  a=got[4+i*4:8+i*4];b=result[4+i*4:8+i*4]
  if 52<=i*4<88 or 112<=i*4<148:
   af,bf=struct.unpack('<f',a)[0],struct.unpack('<f',b)[0];err=abs(af-bf);max_error=max(max_error,err);assert err<=1e-15,('PC matrix',n,i,af,bf)
  else:assert a==b,('PC state',n,i,a.hex(),b.hex())
 different+=got!=result
guards=[]
for at,value in [(272,w(0x4000)),(292,f(1)),(292,f(-.1)),(464,f(-1)),(88,f(float('nan')))]:
 command=bytearray(commands[1]);command[at:at+4]=value;guards.append(bytes(command))
 x.mem_write(B,bytes(command));x.mem_write(STACK,w(STOP,B,struct.unpack_from('<I',command,464)[0],B+468,B+480));x.reg_write(UC_X86_REG_ESP,STACK)
 x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 assert x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(B,484))==command
bad=subprocess.check_output([str(root/'build/pc/Release/rf_eye_probe.exe'),'--ordinary-partial'],input=b''.join(guards))
assert bad==b''.join(w(0xfffffffc)+command[:464]+command[480:] for command in guards)
report=dict(result='PASS',pc_nxdk_failure_guards=len(guards),original_nxdk_exact_cases=len(commands),pc_cases=len(commands),pc_matrix_differing_cases=different,pc_matrix_max_absolute_error=max_error,scope='Original49ffd2..4a011b ordinary partial prefix, actual vector length/scaling,433c80,49f2a0,4a0d70,49cd30. All464 retained bytes plus remaining time; PC only matrix residual tolerance1e-15. Contact dispatch and subsequent flag/time publication excluded.')
(root/'artifacts/ordinary-partial.json').write_text(json.dumps(report,indent=2));print(report)
