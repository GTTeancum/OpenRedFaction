"""Complete original sphere-trigger predicate against reconstructed PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data)
 m.mem_map(base,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
u=machine(original);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_trigger_sphere_contact\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
rng=random.Random(0x4bf620);commands=bytearray();expected=bytearray();hits=guards=0
for n in range(4096):
 scale=2.0**rng.randrange(-30,31);center=[rng.uniform(-10,10)*scale for _ in range(3)]
 radius=rng.uniform(-5,5)*scale;actor=[a+rng.uniform(-6,6)*scale for a in center]
 if n%4==0:
  center=[0.,0.,0.];radius=scale;actor=[scale,0.,0.]
  bits=struct.unpack('<I',struct.pack('<f',scale))[0]+(n//4%3)-1
  actor[n//12%3]=struct.unpack('<f',w(bits))[0]
  if n//12%3:actor[0]=0.
 if n%17==0:radius=0.;actor=center[:]
 if n%101==0:radius=math.nan
 if n%103==0:actor[1]=math.inf
 wire=struct.pack('<7f',*center,radius,*actor);commands.extend(wire)
 if not all(math.isfinite(a) for a in struct.unpack('<7f',wire)):
  want=w(-2,0xa5a5a5a5);guards+=1
 else:
  trigger=bytearray(0x100);obj=bytearray(0x100);trigger[0x3c:0x48]=wire[:12];trigger[0x78:0x7c]=wire[12:16];obj[0x3c:0x48]=wire[16:]
  u.mem_write(base,bytes(trigger));u.mem_write(base+0x200,bytes(obj));u.mem_write(stack,w(stop,base,base+0x200));u.reg_write(UC_X86_REG_ESP,stack)
  u.emu_start(0x4bf620,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
  hit=u.reg_read(UC_X86_REG_EAX)&255;assert hit in (0,1);hits+=hit;want=w(0,hit)
  assert bytes(u.mem_read(base,0x100))==trigger and bytes(u.mem_read(base+0x200,0x100))==obj
 expected.extend(want);x.mem_write(base,wire);x.mem_write(base+0x100,w(0xa5a5a5a5));x.mem_write(stack,w(stop,base,struct.unpack('<I',wire[12:16])[0],base+16,base+0x100));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x100,4))==want,('NXDK',n)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--trigger-sphere'],input=commands)
assert actual==expected
report=dict(result='PASS',cases=4096,original_cases=4096-guards,port_guards=guards,hits=hits,original_sha256=digest,scope='Complete 4bf620 and vector callees without interception, explicit 53-bit nearest x87. Exact PC/NXDK decisions; center contact, one-ULP inside/on/outside boundaries, signed and zero radius, scales 2^-30..2^30. Nonfinite guards preserve output. Box contact, live actor lookup and activation excluded.')
(root/'artifacts/trigger-sphere-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
