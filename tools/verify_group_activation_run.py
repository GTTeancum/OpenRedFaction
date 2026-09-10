"""Whole original controller activation versus combined shared runtime path."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
base=0x30000000;stack=base+0xe000;stop=base+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(b,(len(im)+4095)//4096*4096);m.mem_write(b,im);m.mem_map(base,65536);return m
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
u=machine(original);x=machine(root/'build/xbox/main.exe');traces={}
entry=int(re.search(r'_rf_group_activation_run\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def effect(m,a,s,which):
 sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<7I',m.mem_read(sp,28));native=a<base
 if native:ret,value,pos,scalar=args[:4];source=struct.unpack('<I',m.mem_read(base+0x314,4))[0]
 else:ret,context,value,pos,scalar=args[:5];source=struct.unpack('<I',m.mem_read(base+0x4400,4))[0]
 if which==1:assert (args[5] if native else args[5])==0
 else:
  assert value==0x20001 and scalar==0x41200000
  if native:assert args[4]==args[5]==0
 position=struct.unpack('<3I',m.mem_read(pos,12));assert position==struct.unpack('<3I',f(1,2,3))
 t=traces[native];t[0]+=1
 for v in (which,value,source,*position,scalar):t[1]=((t[1]^v)*16777619)&0xffffffff
 t[2].append((which,value,source));m.reg_write(UC_X86_REG_EAX,value+100 if which==1 else 0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
for m,address,kind in [(u,0x5056a0,1),(u,0x408280,2),(x,base+0xf100,1),(x,base+0xf200,2)]:m.hook_add(UC_HOOK_CODE,effect,user_data=kind,begin=address,end=address)
rng=random.Random(0x46aba0);commands=bytearray();expected=bytearray();starts=0
offsets=[0x318,0x2e8,0x2f8,0x2fc,0x300,0x30c]
for n in range(1024):
 flags=rng.randrange(0x4000);state=w(flags,n%6,n%2,rng.choice([-1,-1,0,1]),0x3e800000,-1)
 actorflags=rng.choice([0,8,0x4000,0x4008]);localflags=rng.choice([0,0x800]);g1=rng.choice([0,1,2,256]);g2=rng.choice([0,1,2,257]);commands.extend(state+w(actorflags,localflags,g1,g2))
 u.mem_write(base,bytes(0x4000));u.mem_write(0x7394cc,bytes(4096))
 for i in range(3):u.mem_write(0x7394cc+4*i,w(base+i*0x1000));u.mem_write(base+i*0x1000+0x2c,w(((i+1)<<16)|i))
 u.mem_write(base+0x24,w(8));u.mem_write(base+0x29c,w(2));u.mem_write(base+0x2cc,w(1,1,base+0x800));u.mem_write(base+0x800,w(0x30002));u.mem_write(base+0x3c,f(1,2,3));u.mem_write(base+0x314,w(77))
 for i,o in enumerate(offsets):u.mem_write(base+o,state[i*4:i*4+4])
 u.mem_write(base+0x2d8,w(11,12,13,14));u.mem_write(base+0x31c,w(21,22,23,24))
 u.mem_write(base+0x1000+0x7c,w(actorflags));u.mem_write(base+0x1000+0x810,w(localflags));u.mem_write(base+0x1000+0x200,w(-1));u.mem_write(base+0x1000+0x6cc,w(55));u.mem_write(base+0x1000+0x190,f(100,100,100,101,101,101));u.mem_write(base+0x1000+0x28c,w(0x5cb060))
 u.mem_write(base+0x2000+0x24,w(9));u.mem_write(base+0x2000+0x190,f(-1,-1,-1,1,1,1));u.mem_write(base+0x3000+0x190,f(-.5,-.5,-.5,.5,.5,.5));u.mem_write(base+0x3000+0x28c,w(0x872128))
 for a,v in [(0x5cb2ec,base+0x1000),(0x5cb054,base+0x1000),(0x8723b4,base+0x3000),(0x64e63c,0x64e3b0)]:u.mem_write(a,w(v))
 u.mem_write(0x7cabd4,bytes([g1&255]));u.mem_write(0x7cabb0,bytes([g2&255]));u.mem_write(stack,w(stop,0x10000,99,0x20001));u.reg_write(UC_X86_REG_ESP,stack);traces[True]=[0,2166136261,[]]
 u.emu_start(0x46aba0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 source=bytes(u.mem_read(base+0x314,4));started=int(source==w(99));starts+=started
 want=w(0)+b''.join(bytes(u.mem_read(base+o,4)) for o in offsets)+bytes(u.mem_read(base+0x16cc,4))+source+bytes(u.mem_read(base+0x31c,16))+bytes(u.mem_read(base+0x307c,4))+bytes(u.mem_read(base+0x31a8,4))+w(started,*traces[True][:2]);expected.extend(want)
 x.mem_write(base,bytes(0x9000));x.mem_write(base+16,w(base+0x6000,0x30002));x.mem_write(base+0x6000,w(9,0x30002,base+0x6100));x.mem_write(base+0x6100+212,f(-1,-1,-1,1,1,1))
 x.mem_write(base+0x4100,state);x.mem_write(base+0x4200+68,f(1,2,3));x.mem_write(base+0x4300,w(11,12,13,14,21,22,23,24));x.mem_write(base+0x4400,w(77));x.mem_write(base+0x4500,w(0x20001,0,0,actorflags,localflags));x.mem_write(base+0x4500+28,w(-1));x.mem_write(base+0x5004,w(base+0x4500))
 x.mem_write(base+0x6300+20,w(1));x.mem_write(base+0x6300+24,f(-.5,-.5,-.5,.5,.5,.5));x.mem_write(base+0x6400,w(0x30002));x.mem_write(base+0x6500,w(55,0xa5a5a5a5))
 x.mem_write(base+0x4000,w(base+0x4100,base+0x4200,base+0x4300,base+0x4400,base+0x5000,base+0x4500,base,base+0x6300,1,base+0x6400,1,0,0,0,0,base+0xf100,base+0xf200,0,g1,g2))
 x.mem_write(stack,w(stop,base+0x4000,0x10000,2,99,0x20001,base+0x6500,base+0x6504));x.reg_write(UC_X86_REG_ESP,stack);traces[False]=[0,2166136261,[]]
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x4100,24))+bytes(x.mem_read(base+0x6500,4))+bytes(x.mem_read(base+0x4400,4))+bytes(x.mem_read(base+0x4310,16))+bytes(x.mem_read(base+0x6308,8))+bytes(x.mem_read(base+0x6504,4))+w(*traces[False][:2])
 assert got==want,('NXDK',n,got.hex(),want.hex());assert traces[False]==traces[True]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-activation-run'],input=commands);assert actual==expected
report=dict(result='PASS',cases=1024,started=starts,original_sha256=digest,scope='Entire original 46aba0 with real lookup, actor, motion, gather and wake helpers. Only audio5056a0 and AI408280 backends intercepted. Exact PC/NXDK combined states and effect order including old source observed by callbacks. Valid controller, two keys, one mover/secondary wake object; live actor snapshot commits, audio/AI execution and scene motion excluded.')
(root/'artifacts/group-activation-run-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
