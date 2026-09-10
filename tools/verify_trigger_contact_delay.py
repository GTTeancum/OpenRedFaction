"""Original contact-delay stage with resolved contact and captured activation."""
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
entry=int(re.search(r'_rf_trigger_contact_delay\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
from unicorn import UC_HOOK_CODE
period=1072800000;accepted=1;reject_geometry=False;fires=0
u.mem_write(0x64ecb9,bytes(2))
def boundary(m,address,size,data):
 global fires
 sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
 if address==0x4c06d0:value=1 if reject_geometry else accepted
 elif address==0x4bf620:value=accepted
 else:fires+=1;value=1
 m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
for a in (0x4c06d0,0x4bf620,0x4c0220):u.hook_add(UC_HOOK_CODE,boundary,begin=a,end=a)
rng=random.Random(0x4bfd50);fixtures=[]
for seconds in (-1.,0.,.000001,.00049,.0005,.001,.01,.5,1.,100.):
 for now in (0,100,period-1,period):
  for deadline in (-2,-1,0,100,period-1,period):
   for accept in (0,1):fixtures.append((seconds,deadline,now,accept))
for n in range(1024):fixtures.append((rng.uniform(0,10),rng.choice([-1,rng.randrange(period+1)]),rng.randrange(period+1),n%2))
commands=bytearray();expected=bytearray();total_fires=0
for n,(seconds,deadline,now,accepted) in enumerate(fixtures):
 reject_geometry=bool(n//2%2);fires=0;wire=struct.pack('<fiiI',seconds,deadline,now,accepted);commands.extend(wire)
 trigger=bytearray(0x400);trigger[0x2c0:0x2c4]=w(0xffffffff);trigger[0x2f8:0x300]=wire[:8]
 actor=bytearray(0x100);actor[0x2c:0x30]=w(123)
 u.mem_write(base,bytes(trigger));u.mem_write(base+0x400,bytes(actor));u.mem_write(0x5a3ed8,w(now));u.mem_write(stack,w(stop,base,base+0x400,0));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x4bfc60,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 ready=u.reg_read(UC_X86_REG_EAX)&255;assert ready==fires;total_fires+=fires
 final=bytes(u.mem_read(base+0x2fc,4));want=w(0)+final+w(ready);expected.extend(want)
 trigger[0x2fc:0x300]=final;assert bytes(u.mem_read(base,0x400))==trigger and bytes(u.mem_read(base+0x400,0x100))==actor
 x.mem_write(base,wire);x.mem_write(base+0x100,w(0xa5a5a5a5));x.mem_write(stack,w(stop,base,now,accepted,base+0x100));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+4,4))+bytes(x.mem_read(base+0x100,4));assert got==want,('NXDK',n,got.hex(),want.hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--trigger-contact-delay'],input=commands)
assert actual==expected
report=dict(result='PASS',cases=len(fixtures),ready=total_fires,original_sha256=digest,scope='Original 4bfc60 SP no-key path; eligibility and sphere contact supplied, activation captured. All contact-delay instructions, CRT conversion and timer helpers unchanged. Exact PC/NXDK deadline and proceed result; unrelated original state unchanged. Includes positive/zero/negative delays, zero-ms rounding, inactive deadlines, cancellation and clock wrap. Key gate and live activation not covered.')
(root/'artifacts/trigger-contact-delay-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
