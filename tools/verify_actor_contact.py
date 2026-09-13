"""Compare original41a000 actor contact decision with shared PC/NXDK."""
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
entry=int(re.search(r'\s_rf_entity_actor_contact_decide\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
occupant=destroy=0
T=B+0x2000
def word(cpu,at):return struct.unpack('<I',bytes(cpu.mem_read(at,4)))[0]
def hook(cpu,at,size,data):
 global destroy
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if at==0x429790:
  assert word(cpu,sp+4)==T;destroy+=1;cpu.reg_write(UC_X86_REG_EAX,0)
 else:
  assert word(cpu,sp+4)==B and word(cpu,sp+8)==0xffffffff;cpu.reg_write(UC_X86_REG_EAX,occupant)
 cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for at in (0x429790,0x42a130):u.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
rng=random.Random(0x41a000);commands=[];expected=[];counts={}
for n in range(2048):
 kind=rng.choice([0,1,1,2,3]);flags=rng.choice([0,0x200,0x8000,0x8200]);contact=rng.choice([0,3,3,8]);occupant=rng.choice([0,1,2,255,256,257])
 velocity=[rng.uniform(-.5,.5) for _ in range(3)] if n%3==0 else ([0]*3 if n%3==1 else [1,0,0])
 angular=[rng.uniform(-.5,.5) for _ in range(3)] if n%4==0 else ([0]*3 if n%4==1 else [0,1,0])
 mass=rng.choice([0,29.999,30,31,100]);armor=rng.choice([-1,0,1]);tf=rng.choice([0,0x2000000]);f814=rng.choice([0,0x20]);objflags=rng.choice([0,8,0x100])
 command=w(kind,flags,contact,occupant)+f(*velocity,*angular,mass,armor)+w(tf,f814,objflags);assert len(command)==60;commands.append(command)
 u.mem_write(B,bytes(0x6000));u.mem_write(B+0x294,w(B+0x4000));u.mem_write(B+0x41b4,w(kind));u.mem_write(B+0x4724,w(flags));u.mem_write(B+0x1d0,w(contact));u.mem_write(B+0x144,command[16:40])
 u.mem_write(T+0x294,w(B+0x5000));u.mem_write(B+0x5724,w(tf));u.mem_write(T+0x98,command[40:44]);u.mem_write(T+0x38,command[44:48]);u.mem_write(T+0x814,w(f814));u.mem_write(T+0x7c,w(objflags))
 u.mem_write(STACK,w(STOP,B,T));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x37f);destroy=0
 u.emu_start(0x41a000,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 result=w(0,u.reg_read(UC_X86_REG_EAX)&255,destroy);expected.append(result);key=(u.reg_read(UC_X86_REG_EAX)&255,destroy);counts[str(key)]=counts.get(str(key),0)+1
 x.mem_write(B,command);x.mem_write(B+0x100,b'\xa5'*8);x.mem_write(STACK,w(STOP,B,B+0x100,B+0x104));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+0x100,8))==result,('NXDK',n,command.hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--actor-contact'],input=b''.join(commands));assert actual==b''.join(expected)
guards=[]
for at in (16,28,40):
 command=bytearray(w(1,0,0,0)+f(*([0]*6),1,0)+w(0,0,0));command[at:at+4]=f(float('nan'));guards.append(bytes(command))
 x.mem_write(B,bytes(command));x.mem_write(B+0x100,b'\xa5'*8);x.mem_write(STACK,w(STOP,B,B+0x100,B+0x104));x.reg_write(UC_X86_REG_ESP,STACK)
 x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(B+0x100,8))==b'\xa5'*8
bad=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--actor-contact'],input=b''.join(guards));assert bad==(w(0xfffffffc)+b'\xa5'*8)*len(guards)
# The scene passes-1 to42a130; execute its direct flag path without predicate hooks.
q=load(exe)
for n in range(1024):
 flags=rng.getrandbits(32);q.mem_write(B+0x810,w(flags));q.mem_write(STACK,w(STOP,B,0xffffffff));q.reg_write(UC_X86_REG_ESP,STACK)
 q.emu_start(0x42a130,STOP,count=100);assert q.reg_read(UC_X86_REG_EIP)==STOP
 assert (q.reg_read(UC_X86_REG_EAX)&255)==((flags>>16)&1)
report=dict(result='PASS',original_direct_flag_cases=1024,cases=len(commands),pc_nxdk_failure_guards=len(guards),decisions=counts,scope='Full41a000 with actual429990/486c90,42cca0,40cac0,4895d0 and40a180.42a130 predicate supplied and429790 destruction recorded at boundary. Original AL response, destruction request and target checked against PC/NXDK. Effects inside429790 and live contact dispatch excluded.')
(root/'artifacts/actor-contact.json').write_text(json.dumps(report,indent=2));print(report)
