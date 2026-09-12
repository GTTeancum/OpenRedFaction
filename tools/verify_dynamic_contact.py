"""Original49d7e0 positive-inverse-mass branch, real lookup/predicates/math; no hooks."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda v:struct.pack('<'+'f'*len(v),*v)
base=0x30000000;stack=base+0xe000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(base,0x10000);return u
u=load(exe);x=load(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_physics_dynamic_contact\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x49dcf6);commands=[];expected=[];flattened=skipped=0
for case in range(4096):
 state=bytearray(rng.getrandbits(8) for _ in range(308))
 v=[rng.randrange(-10000,10001)/64 for _ in range(9)]
 n=[rng.randrange(-64,65)/32,rng.choice((0.,-0.,.5,-.5,.95,-.95,.9499999,-.9499999,.9500001,-.9500001,1.,-1.)),rng.randrange(-64,65)/32]
 if case%13==0:n[0]=n[2]=0
 mode=(0,1,2,3,8,15,0xffffffff)[case%7]
 present=int(case%5!=0);other=rng.choice((0,1,255,256,257));self=rng.choice((0,1,255,256,257))
 state[184:196]=floats(v[:3]);command=bytes(state)+floats(n+v[3:])+pack(mode,present,other,self);assert len(command)==360
 actor=bytearray(rng.getrandbits(8) for _ in range(0x900));actor[0x7c:0x80]=pack(8 if self&255 else 0)
 actor[0x144:0x150]=floats(v[:3]);actor[0x1c0:0x1cc]=floats(n);actor[0x1d8:0x1e4]=floats(v[6:9]);actor[0x8a0:0x8ac]=floats(v[3:6])
 actor[0x1d4:0x1d8]=floats([.25]);actor[0x1ec:0x1f0]=pack(0);actor[0x858:0x85c]=pack(base+0x3000)
 actor[0x1e4:0x1e8]=pack(0x10000 if present else (0x20000 if case%2 else 0xffffffff))
 u.mem_write(base,bytes(actor));u.mem_write(base+0x2000,bytes(0x300));u.mem_write(base+0x202c,pack(0x10000));u.mem_write(base+0x207c,pack(8 if other&255 else 0))
 u.mem_write(base+0x3004,pack(mode));u.mem_write(0x7394cc,pack(base+0x2000))
 u.mem_write(stack,pack(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(0x49d7e0,0x49ddef,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x49ddef
 out=bytes(u.mem_read(base,len(actor)));impact=bytes(u.mem_read(u.reg_read(UC_X86_REG_ESP)+0x10,4))
 velocity=out[0x144:0x150];normal=out[0x1c0:0x1cc]
 actor[0x144:0x150]=velocity;actor[0x1c0:0x1cc]=normal;assert out==actor,('original unrelated mutation',case)
 if not present or ((other&255) and not(self&255)):skipped+=1;assert impact==pack(0) and velocity==state[184:196] and normal==floats(n)
 if normal!=floats(n):flattened+=1
 want=bytearray(state);want[184:196]=velocity;result=bytes(want)+normal+impact
 x.mem_write(base,b'\xa5'*16+command+b'\x5a'*16);x.mem_write(base+0x2000,pack(0xa5a5a5a5))
 x.mem_write(stack,pack(stop,base+16,base+16+308,base+16+320,base+16+332,mode,present,other,self,base+0x2000))
 x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0,('return',case)
 got=bytes(x.mem_read(base+16,320))+bytes(x.mem_read(base+0x2000,4))
 assert got==result,('NXDK',case,got[184:196].hex(),result[184:196].hex(),got[308:].hex(),result[308:].hex())
 assert bytes(x.mem_read(base,16))==b'\xa5'*16 and bytes(x.mem_read(base+376,16))==b'\x5a'*16
 assert bytes(x.mem_read(base+16+320,40))==command[320:]
 commands.append(command);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--dynamic-contact'],input=b''.join(commands))
assert pc==b''.join(expected),'PC mismatch'
guards=0
valid=bytearray(commands[1]);valid[348:360]=pack(1,0,0)
for offset in list(range(184,196,4))+list(range(308,344,4)):
 for bits in (0x7fc00000,0x7f800000):
  bad=bytearray(valid);bad[offset:offset+4]=pack(bits)
  x.mem_write(base,bytes(bad));x.mem_write(base+0x2000,pack(0xa5a5a5a5))
  mode=struct.unpack_from('<I',bad,344)[0]
  x.mem_write(stack,pack(stop,base,base+308,base+320,base+332,mode,1,0,0,base+0x2000))
  x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
  assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0xfffffffc
  assert bytes(x.mem_read(base,360))==bad and bytes(x.mem_read(base+0x2000,4))==pack(0xa5a5a5a5)
  guards+=1
report=dict(result='PASS',cases=len(commands),nonfinite_guards=guards,flattened=flattened,skipped=skipped,original_sha256=sha,scope='Original49d7e0 entry through49ddef for positive inverse mass, actual handle lookup, player predicates, normalization and vector helpers, no hooks. PC/NXDK exact body/normal/impact, original unrelated bytes and NXDK guards/source preservation. Mode1 threshold and zero-horizontal-normal fallback, stale/missing contact objects, player suppression and low-byte predicate facts. Damage tail and other inverse-mass/liquid/rotation branches excluded.')
(root/'artifacts/dynamic-contact-verification.json').write_text(json.dumps(report,indent=2));print(report)
