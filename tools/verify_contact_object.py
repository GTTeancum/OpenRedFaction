"""Compare original427550 object-contact branch with shared PC/NXDK."""
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
entry=int(re.search(r'\s_rf_entity_contact_object_dispatch\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
wire=[];hash_value=count=0
word=lambda cpu,at:struct.unpack('<I',bytes(cpu.mem_read(at,4)))[0]
def record(event,args):
 global hash_value,count
 count+=1
 for v in [event]+args+[0]*(4-len(args)):hash_value=((hash_value^v)*16777619)&0xffffffff
def hook(cpu,at,size,original):
 sp=cpu.reg_read(UC_X86_REG_ESP);args=lambda n:[word(cpu,sp+4+i*4) for i in range(n)]
 event=({0x40a0e0:1,0x410c70:2,0x41a000:3,0x459560:4} if original else {B+0x8000:1,B+0x8100:2,B+0x8200:3,B+0x8300:4})[at]
 a=args(4)
 if original:
  if event in (1,2):record(event,[a[0]])
  elif event==3:record(event,[word(cpu,a[0]+0x2c),word(cpu,a[1]+0x2c)])
  else:record(event,[word(cpu,a[0]+0x2c),word(cpu,a[1]+0x2c),a[2],a[3]])
  cpu.reg_write(UC_X86_REG_EAX,(B+0x2000 if wire[4] else 0) if event==1 else (B+0x4000 if wire[5] else 0) if event==2 else wire[6] if event==3 else 0)
 else:
  if event in (1,2):record(event,[a[1]])
  elif event==3:record(event,[a[1],a[2]])
  else:record(event,args(5)[1:])
  if event==1:cpu.mem_write(a[2],w(B+0x200 if wire[4] else 0))
  if event==2:cpu.mem_write(a[2],w(wire[5]))
  if event==3:cpu.mem_write(a[3],w(wire[6]))
  cpu.reg_write(UC_X86_REG_EAX,0xffffffff if wire[7]==event else 0)
 cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for at in (0x40a0e0,0x410c70,0x41a000,0x459560):u.hook_add(UC_HOOK_CODE,hook,True,begin=at,end=at)
for at in (B+0x8000,B+0x8100,B+0x8200,B+0x8300):x.hook_add(UC_HOOK_CODE,hook,False,begin=at,end=at)
def run_x():
 global hash_value,count
 x.mem_write(B+0x100,w(wire[0],0,0,wire[1]));x.mem_write(B+0x200,w(wire[2],wire[3]));x.mem_write(B+0x300,w(0,B+0x8000,B+0x8100,B+0x8200,B+0x8300));x.mem_write(B+0x400,w(0xa5a5a5a5))
 x.mem_write(STACK,w(STOP,B+0x100,wire[2],B+0x300,B+0x400));x.reg_write(UC_X86_REG_ESP,STACK);hash_value=2166136261;count=0
 x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 return w(x.reg_read(UC_X86_REG_EAX),word(x,B+0x400),hash_value,count)
rng=random.Random(0x427817);commands=[];expected=[];routes={}
for n in range(2048):
 wire=[rng.getrandbits(32),rng.choice([0,8,0x108]),rng.getrandbits(32),n%12 if n%12<11 else 0xffffffff,int(n%7!=0),(n//7)%2,[0,1,2,255,256,257][(n//12)%6],0]
 u.mem_write(B,bytes(0x5000));u.mem_write(B+0x2c,w(wire[0]));u.mem_write(B+0x7c,w(wire[1]));u.mem_write(B+0x1e4,w(wire[2]));u.mem_write(B+0x1d4,f(1));u.mem_write(B+0x2024,w(wire[3]));u.mem_write(B+0x202c,w(wire[2]));u.mem_write(B+0x4298,w(0x12345678));u.mem_write(0x5afb78,w(0x12345678))
 u.mem_write(STACK,w(STOP));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ECX,B);u.reg_write(UC_X86_REG_FPCW,0x37f);hash_value=2166136261;count=0
 u.emu_start(0x427550,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 decision=u.reg_read(UC_X86_REG_EAX);result=w(0,decision,hash_value,count);routes[decision]=routes.get(decision,0)+1
 assert run_x()==result,('NXDK',n,wire,result.hex(),run_x().hex())
 commands.append(w(*wire));expected.append(result)
for failure in (1,2,3,4):
 wire=[123,0,456,{1:0,2:4,3:0,4:1}[failure],1,0,1,failure];result=run_x()
 assert result[:8]==w(0xffffffff,0xa5a5a5a5)
 commands.append(w(*wire));expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--contact-object'],input=b''.join(commands));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=2048,callback_failure_cases=4,decisions=routes,scope='Full427550 with1ec=0 and contact1d4 nonzero, at40a0e0 lookup,410c70 current-clutter lookup,41a000 actor and459560 pickup boundaries. Actual4895d0 and switch/precedence. Exact decisions and ordered arguments; failed callbacks preserve decision. Other427550 branches and callback internals excluded.')
(root/'artifacts/contact-object.json').write_text(json.dumps(report,indent=2));print(report)
