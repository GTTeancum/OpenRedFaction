"""Compare original4278e0 collision sound with shared PC/NXDK."""
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
entry=int(re.search(r'\s_rf_entity_contact_sound\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
word=lambda cpu,at:struct.unpack('<I',bytes(cpu.mem_read(at,4)))[0]
wire=b'';hash_value=count=0
mapping=[(0,0x8c0,4),(4,0x144,12),(16,0x60,12),(28,0x1c0,12),(40,0x1478,4),(44,0x4000,12)]
def record(event,values):
 global hash_value,count
 count+=1
 for value in [event]+values:hash_value=((hash_value^value)*16777619)&0xffffffff
def hook(cpu,at,size,original):
 sp=cpu.reg_read(UC_X86_REG_ESP);args=lambda n:[word(cpu,sp+4+i*4) for i in range(n)]
 event=({0x505c00:1,0x434da0:2,0x5056a0:3} if original else {B+0x8000:1,B+0x8100:2,B+0x8200:3})[at]
 active,sample,voice,mutation,fail=struct.unpack_from('<5I',wire,60)
 if event<3:
  a=args(1)[0] if original else args(3)[1];record(event,[a]);result=active if event==1 else sample
  if event==1 and mutation:
   for shared,raw,data in [(16,0x60,f(0,0,-1)),(28,0x1c0,f(0,0,1)),(44,0x4000,f(17))]:cpu.mem_write(B+(raw if original else shared),data)
   cpu.mem_write(0x5cc4f8 if original else B+56,w(19))
  if original:cpu.reg_write(UC_X86_REG_EAX,result)
  else:cpu.mem_write(args(3)[2],w(result))
 else:
  if original:
   sample_arg,position,volume,defaults,flags=args(5);assert volume==0x3f800000 and defaults==0x173c378 and flags==0
  else:_,sample_arg,position,_=args(4)
  record(3,[sample_arg]+[word(cpu,position+i*4) for i in range(3)]+[0x3f800000,0])
  if original:cpu.reg_write(UC_X86_REG_EAX,voice)
  else:cpu.mem_write(args(4)[3],w(voice))
 if not original:cpu.reg_write(UC_X86_REG_EAX,0xffffffff if fail==event else 0)
 cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for at in (0x505c00,0x434da0,0x5056a0):u.hook_add(UC_HOOK_CODE,hook,True,begin=at,end=at)
for at in (B+0x8000,B+0x8100,B+0x8200):x.hook_add(UC_HOOK_CODE,hook,False,begin=at,end=at)
def run_x(command):
 global wire,hash_value,count
 wire=command;x.mem_write(B,wire);x.mem_write(B+0x6000,w(0,B+56,B+0x8000,B+0x8100,B+0x8200));x.mem_write(STACK,w(STOP,B,B+44,B+0x6000));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);hash_value=2166136261;count=0
 x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,60))+w(hash_value,count)
rng=random.Random(0x4278e0);commands=[];expected=[];counts={}
for n in range(2048):
 speed=rng.choice([0,1,2,5]);velocity=[rng.uniform(-2,2) for _ in range(3)]
 normal=[0,0,rng.choice([-.5,-.4,-.3999999,0,-1])];forward=[0,0,1]
 if n%3==0:normal=[rng.uniform(-1,1) for _ in range(3)];forward=[rng.uniform(-1,1) for _ in range(3)]
 wire=f(speed,*velocity,*forward,*normal)+w(rng.getrandbits(32))+f(1,2,3)+w(rng.getrandbits(32),rng.choice([0,0,1,256]),rng.getrandbits(32),rng.getrandbits(32),(n//5)%2,0)
 assert len(wire)==80;u.mem_write(B,bytes(0x5000))
 for at,raw,size in mapping:u.mem_write(B+raw,wire[at:at+size])
 u.mem_write(0x5cc4f8,wire[56:60]);u.mem_write(STACK,w(STOP,B,B+0x4000));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x37f);hash_value=2166136261;count=0
 u.emu_start(0x4278e0,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 after=bytearray(wire[:60])
 for at,raw,size in mapping:after[at:at+size]=u.mem_read(B+raw,size)
 after[56:60]=u.mem_read(0x5cc4f8,4);result=w(0)+bytes(after)+w(hash_value,count);counts[count]=counts.get(count,0)+1
 command=wire;assert run_x(command)==result,('NXDK',n);commands.append(command);expected.append(result)
for fail in (1,2,3):
 command=f(0,1,0,0,0,0,1,0,0,-1)+w(123)+f(1,2,3)+w(7,0,9,11,1,fail)
 result=run_x(command);assert result[:4]==w(0xffffffff) and result[44:48]==w(123) and struct.unpack_from('<I',result,68)[0]==fail
 commands.append(command);expected.append(result)
for at in (0,4,16):
 command=bytearray(f(0,1,0,0,0,0,1,0,0,-1)+w(123)+f(1,2,3)+w(7,0,9,11,0,0));command[at:at+4]=f(float('nan'));command=bytes(command)
 result=run_x(command);assert result[:4]==w(0xfffffffc) and result[4:64]==command[:60]
 commands.append(command);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--contact-sound'],input=b''.join(commands));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=2048,callback_failure_cases=3,finite_failure_cases=3,callback_counts=counts,scope='Full4278e0 with real40a180 squared speed and40a0b0 Z+Y+X dot. Voice-playing, selection and playback boundaries supplied; callback mutation proves fresh direction/position/group reads. Exact fields, call order/arguments and voice publication. No live voice binding or full427550 surface branch claim.')
(root/'artifacts/contact-sound.json').write_text(json.dumps(report,indent=2));print(report)
