"""Compare projectile152-byte creation descriptors at original486da0 boundary."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
f=lambda v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;STACK=B+0xff000;STOP=B+0xfff00;OUT=B+0x4000;NAME=B+0x8000;OWNER=B+0x10000
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();c=Uc(UC_ARCH_X86,UC_MODE_32);a=p.OPTIONAL_HEADER.ImageBase;c.mem_map(a,(len(im)+4095)//4096*4096);c.mem_write(a,im);c.mem_map(B,0x100000);c.mem_map(0,4096);return c
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_projectile_descriptor_prepare\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def word(c,a):return struct.unpack('<I',c.mem_read(a,4))[0]
state={};boost=[0];trace=[]
def hook(c,a,size,data):
 if a==0x4c788a:boost[0]=1;return
 sp=c.reg_read(UC_X86_REG_ESP);arg=word(c,sp+4);trace.append(a)
 if a==0x426fc0:assert arg==123;value=OWNER
 elif a==0x48aa30:assert arg==OWNER;value=state['player']
 else:assert arg==OWNER;value=state['power']
 c.reg_write(UC_X86_REG_EAX,value);c.reg_write(UC_X86_REG_EIP,word(c,sp));c.reg_write(UC_X86_REG_ESP,sp+4)
for a in (0x426fc0,0x48aa30,0x429f40,0x4c788a):u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def run(c,a,args,until=STOP):
 c.mem_write(0,w(0));c.mem_write(STACK,w(STOP,*args));c.reg_write(UC_X86_REG_ESP,STACK);c.reg_write(UC_X86_REG_FPCW,0x27f);c.emu_start(a,until,count=50000);assert c.reg_read(UC_X86_REG_EIP)==until;return c.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4c77a0);inputs=[];outputs=[];boosted=spin=0
for k in range(2048):
 count=rng.randrange(65);weapon=rng.randrange(count+1);special=rng.choice([-1,weapon,(weapon+1)%65]);length=rng.choice([0,1,4,5,12]);model=rng.getrandbits(32)
 flags=rng.getrandbits(32);flags2=rng.getrandbits(32);bc=rng.uniform(.1,10);ac=rng.uniform(.1,10);speed=rng.choice([0.,-1.,49.999996185302734,50.,50.000003814697266,1000.]);scale=rng.choice([0.,.25,.5,2.]);network=rng.choice([0,0,1,2,256]);player=rng.choice([0,1,2,256]);power=rng.choice([0,1,2,257])
 pos=[rng.uniform(-100,100) for _ in range(3)];basis=[rng.uniform(-2,2) for _ in range(9)]
 source=w(length,NAME,model,flags,flags2)+f([bc,ac,speed,scale])+w(network,player,power,weapon,count,special)+f(pos+basis)
 d=0x85cd08+weapon*0x550;u.mem_write(d,bytes(0x550));u.mem_write(d+0x14,w(NAME));u.mem_write(d+0x18,w(model));u.mem_write(NAME,b'x'*length+b'\0');u.mem_write(d+0xbc,f([bc,speed]));u.mem_write(d+0xac,f([ac]));u.mem_write(d+0x264,w(flags,flags2));u.mem_write(0x872448,w(count));u.mem_write(0x872118,w(special));u.mem_write(0x64ecb9,bytes([network&255]));u.mem_write(0x5a2564,f([scale]));u.mem_write(B+0x1000,f(pos));u.mem_write(B+0x2000,f(basis));state.update(player=player,power=power);boost[0]=0;trace.clear()
 run(u,0x4c77a0,[weapon,123,B+0x1000,B+0x2000,0,0],0x4c7a11)
 sp=u.reg_read(UC_X86_REG_ESP);args=[word(u,sp+4*i) for i in range(6)];assert args[:3]==[2,0xffffffff,123] and args[4:]==[0x100000,0]
 descriptor=bytes(u.mem_read(args[3],152));expected=w(0)+descriptor+w(boost[0]);boosted+=boost[0];spin+=word(u,args[3]+16)==2
 wanted=[0x426fc0]
 if not network&255:
  if flags&0x400:wanted+=[0x48aa30]
  if (not flags&0x400 or not player&255) and flags&0x40000000:wanted+=[0x429f40]
 assert trace==wanted
 x.mem_write(B,source);x.mem_write(OUT,b'\xa5'*156);status=run(x,entry,[B,OUT]);got=w(status)+bytes(x.mem_read(OUT,156));assert got==expected,(k,next((i for i,(a,b) in enumerate(zip(got,expected)) if a!=b),None),got.hex(),expected.hex())
 inputs.append(source);outputs.append(expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--projectile-descriptor'],input=b''.join(inputs))==b''.join(outputs)
guards=[]
for offset,value,status in [(48,0xffffffff,-3),(48,65,-3),(28,0x7fc00000,-4),(60,0x7f800000,-4),(72,0x7fc00000,-4)]:
 wire=bytearray(inputs[0]);struct.pack_into('<i',wire,52,64);struct.pack_into('<I',wire,offset,value)
 x.mem_write(B,bytes(wire));x.mem_write(OUT,b'\xa5'*156);assert run(x,entry,[B,OUT])==status&0xffffffff
 expected=w(status)+b'\xa5'*156;assert bytes(x.mem_read(OUT,156))==expected[4:]
 assert subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--projectile-descriptor'],input=wire)==expected
 guards.append(1)
for args in ([0,OUT],[B,0]):assert run(x,entry,args)==0xfffffffc
report=dict(result='PASS',cases=2048,pc_nxdk_guards=5,nxdk_null_guards=2,boosted=boosted,spinning=spin,original_sha256=digest,scope='Original4c77a0 through call486da0, all string/vector/spin/predicate4c90f0 callees unhooked; only owner lookup and player/powerup predicates supplied with checked arguments/order. All152 descriptor bytes and boost state match PC/NXDK at027f. Includes inclusive weapon-count boundary, name length4/5 and speed50. Allocation and subsequent projectile lifecycle excluded.')
(root/'artifacts/projectile-descriptor.json').write_text(json.dumps(report,indent=2));print(report)
