"""Compare original429790 contact destruction ordering with shared PC/NXDK."""
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
entry=int(re.search(r'\s_rf_entity_contact_destroy\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
mutation=fail=hash_value=count=0
word=lambda cpu,at:struct.unpack('<I',bytes(cpu.mem_read(at,4)))[0]
def record(values):
 global hash_value,count
 count+=1
 for v in values:hash_value=((hash_value^v)*16777619)&0xffffffff
def hook(cpu,at,size,original):
 sp=cpu.reg_read(UC_X86_REG_ESP);args=lambda n:[word(cpu,sp+4+i*4) for i in range(n)]
 event=({0x4892c0:1,0x434da0:2,0x5056a0:3} if original else {B+0x8000:1,B+0x8100:2,B+0x8200:3})[at]
 if event==1:
  if original:values=args(8)
  else:
   context,actor,request=args(3);assert actor==B;v=[word(cpu,request+i*4) for i in range(6)]
   values=[word(cpu,B),v[0],v[1],0xffffffff,v[2],v[3],v[4],v[5]]
  record([1]+values)
  if mutation:
   if original:cpu.mem_write(B+0x294,w(B+0x2000));cpu.mem_write(B+0x2174,w(17));cpu.mem_write(B+0x3c,f(9))
   else:cpu.mem_write(B+4,w(17));cpu.mem_write(B+8,f(9))
 elif event==2:
  group=args(1)[0] if original else args(3)[1];record([2,group]);sample=0xa5000000^group
  if mutation:cpu.mem_write(B+(0x40 if original else 12),f(-3))
  if original:cpu.reg_write(UC_X86_REG_EAX,sample)
  else:cpu.mem_write(args(3)[2],w(sample))
 else:
  if original:
   sample,pos,volume,defaults,flags=args(5);assert volume==0x3f800000 and defaults==0x173c378 and flags==0
  else:_,sample,pos=args(3)
  record([3,sample]+[word(cpu,pos+i*4) for i in range(3)]+[0x3f800000,0])
 if original and event==1:cpu.reg_write(UC_X86_REG_EIP,B+0x9000);return
 if not original:cpu.reg_write(UC_X86_REG_EAX,0xffffffff if fail==event else 0)
 cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for at in (0x4892c0,0x434da0,0x5056a0):u.hook_add(UC_HOOK_CODE,hook,True,begin=at,end=at)
for at in (B+0x8000,B+0x8100,B+0x8200):x.hook_add(UC_HOOK_CODE,hook,False,begin=at,end=at)
u.mem_write(B+0x9000,b'\xd9\xee\xc3')
def run_x(command):
 global mutation,fail,hash_value,count
 mutation,fail=struct.unpack_from('<2I',command,20);hash_value=2166136261;count=0
 x.mem_write(B,command);x.mem_write(B+0x6000,w(0,B+0x8000,B+0x8100,B+0x8200));x.mem_write(STACK,w(STOP,B,B+0x6000));x.reg_write(UC_X86_REG_ESP,STACK)
 x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,20))+w(hash_value,count)
rng=random.Random(0x429790);commands=[];expected=[]
for n in range(1024):
 actor=w(rng.getrandbits(32),rng.getrandbits(32))+f(*[rng.uniform(-100,100) for _ in range(3)]);mutation=n%2;fail=0;command=actor+w(mutation,fail)
 u.mem_write(B+0x2c,actor[:4]);u.mem_write(B+0x294,w(B+0x1000));u.mem_write(B+0x1174,actor[4:8]);u.mem_write(B+0x3c,actor[8:20]);u.mem_write(STACK,w(STOP,B));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x37f);hash_value=2166136261;count=0
 u.emu_start(0x429790,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP and count==3
 out=bytes(u.mem_read(B+0x2c,4))+bytes(u.mem_read(word(u,B+0x294)+0x174,4))+bytes(u.mem_read(B+0x3c,12))
 want=w(0)+out+w(hash_value,count);assert run_x(command)==want
 commands.append(command);expected.append(want)
for failure in (1,2,3):
 for mut in (0,1):
  command=commands[0][:20]+w(mut,failure);result=run_x(command);assert result[:4]==w(0xffffffff) and struct.unpack_from('<I',result,28)[0]==failure
  commands.append(command);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--contact-destroy'],input=b''.join(commands));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=1024,callback_failure_cases=6,scope='Full429790 ordering at4892c0 damage,434da0 selection and5056a0 playback boundaries. Exact target/damage arguments, post-damage class reselection, post-selection position, sample/volume/default parameters and flags. Mutation and failure order compared on PC/NXDK; callback internals and live scene ownership excluded.')
(root/'artifacts/contact-destroy.json').write_text(json.dumps(report,indent=2));print(report)
