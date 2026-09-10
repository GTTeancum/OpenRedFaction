"""Full original box-trigger contact versus PC/NXDK."""
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
entry=int(re.search(r'_rf_trigger_box_contact\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
u.mem_write(0x85682c,w(*(base+0x1000+i*16 for i in range(4))));u.mem_write(0x1754524,w(15))
rng=random.Random(0x4c0a80);commands=bytearray();expected=bytearray();counts=[0,0];guards=0
for n in range(8192):
 scale=2.0**rng.randrange(-8,9);angle=rng.uniform(-math.pi,math.pi);co=math.cos(angle);si=math.sin(angle)
 center=[rng.uniform(-10,10)*scale for _ in range(3)];matrix=[co,0,si,0,1,0,-si,0,co];size=[2*scale,2*scale,2*scale]
 if n%3==0:matrix=[1,0,0,0,1,0,0,0,1]
 if n%17==0:matrix=[0,1,0,0,0,1,1,0,0]
 if n%19==0:matrix=[0,0,1,1,0,0,0,1,0]
 def world(v):return [center[j]+sum(v[k]*matrix[k*3+j] for k in range(3)) for j in range(3)]
 px=rng.uniform(-1.5,1.5)*scale;py=rng.uniform(-1.5,1.5)*scale
 current=world([px,py,2*scale]);start=current[:];end=world([px,py,0])
 if n%7==0:end,current=current,end;start=current[:]
 if n%11==0:start=world([px,py,4*scale])
 if n%13==0:end=current[:]
 flags=0 if n%4==0 else 32
 if n%23==0:
  center=[0,0,0];matrix=[1,0,0,0,1,0,0,0,1];size=[2,2,2]
  current=[rng.choice([-1,-.75,0,.75,1]),rng.choice([-1,-.75,0,.75,1]),2];start=current[:];end=[*current[:2],0]
 if n<17:
  center=[0,0,0];matrix=[1,0,0,0,1,0,0,0,1];size=[2,2,2];flags=32
  current=[(-.75,0,.75)[n%3],(-.75,0,.75)[n//3%3],2];start=current[:];end=[*current[:2],0]
  if n==9:current=[0,0,0];start=current[:];end=[0,0,2]
  if n==10:end=current[:]
  if n==11:current=[0,0,2];start=[0,0,4];end=[0,0,0]
  if n>=12:
   current=[0,0,0];start=[0,0,1]
   threshold=struct.unpack('<I',struct.pack('<f',.00001))[0]
   end=[0,0,-struct.unpack('<f',w(threshold+(n%3)-1))[0]]
 if n%101==0:size[1]=-1
 if n%103==0:matrix[3]=math.nan
 wire=struct.pack('<15fI9f',*center,*matrix,*size,flags,*current,*start,*end);commands.extend(wire)
 if n%101==0 or n%103==0:want=w(-2,0xa5a5a5a5);guards+=1
 else:
  trigger=bytearray(0x400);actor=bytearray(0x200);trigger[0x3c:0x48]=wire[:12];trigger[0x48:0x6c]=wire[12:48];trigger[0x2c8:0x2d4]=wire[48:60];trigger[0x2b0:0x2b4]=wire[60:64]
  actor[0x3c:0x48]=wire[64:76];actor[0xe4:0xf0]=wire[76:88];actor[0xf0:0xfc]=wire[88:100]
  u.mem_write(base,bytes(trigger));u.mem_write(base+0x400,bytes(actor));u.mem_write(stack,w(stop,base,base+0x400));u.reg_write(UC_X86_REG_ESP,stack)
  u.emu_start(0x4c0a80,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
  hit=u.reg_read(UC_X86_REG_EAX)&255;assert hit in (0,1);counts[bool(flags)]+=hit;want=w(0,hit)
  assert bytes(u.mem_read(base,0x400))==trigger and bytes(u.mem_read(base+0x400,0x200))==actor
 expected.extend(want);x.mem_write(base,wire);x.mem_write(base+0x200,w(0xa5a5a5a5));x.mem_write(stack,w(stop,base,base+12,base+48,flags,base+64,base+76,base+88,base+0x200));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x200,4))
 assert got==want,('NXDK',n,got.hex(),want.hex(),wire.hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--trigger-box'],input=commands)
assert len(actual)==len(expected)
for n in range(8192):assert actual[n*8:(n+1)*8]==expected[n*8:(n+1)*8],('PC',n,actual[n*8:(n+1)*8].hex(),expected[n*8:(n+1)*8].hex())
report=dict(result='PASS',cases=8192,original_cases=8192-guards,port_guards=guards,hits_by_ordinary_directional=counts,original_sha256=digest,scope='Complete original 4c0a80 and all geometric callees, supplied scratch allocations and initialized static flags. Exact PC/NXDK decisions under 53-bit nearest x87. Ordinary and directional paths, distinct actor positions, reverse/stationary movement, face gap, boundaries, rotated principal axes and scales 2^-8..2^8. Live actor updates, dwell/keys and activation excluded.')
(root/'artifacts/trigger-box-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
