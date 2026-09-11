"""Compare bounded corpse slot allocation with original pool and type7 gate."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(0,4096);u.mem_map(0x30000000,65536);return u
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(original);x=machine(root/'build/xbox/main.exe');b=0x30000000;stack=b+0xe000;stop=b+0xf000;output=b+0x8000
read=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
def call(m,address,args=(),ecx=0):
 m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.reg_write(UC_X86_REG_ECX,ecx);m.emu_start(address,stop,count=100000)
 assert m.reg_read(UC_X86_REG_EIP)==stop
 return m.reg_read(UC_X86_REG_EAX)
mapping=(root/'build/xbox/main.map').read_text()
entries={name:int(re.search('_rf_corpse_pool_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for name in ('init','acquire','release')}
u.mem_write(b,bytes([0xa5])*0x5cec);call(u,0x48b7b0,(0,),b);call(x,entries['init'],(b,))
payload=bytearray(u.mem_read(b,0x5cd0));active=set();rng=random.Random(0x48b870);cases=[];expected=[];full_checks=0;reuses=0;seen=set()
def index(pointer):
 if not pointer:return 0xffffffff
 assert b<=pointer<b+30*0x318 and (pointer-b)%0x318==0
 return (pointer-b)//0x318
def step(op,slot=0,invalid=False):
 global full_checks,reuses
 cases.append((op,slot));result=0xffffffff
 if op==0:
  result=index(call(u,0x48b870,(),b));status=0 if result!=0xffffffff else -3
  if status:
   assert len(active)==30
   # Verify the real type7 caller rejects saturation, even with heap fallback enabled.
   u.mem_write(0x70e430,w(30));u.mem_write(0x879af8,b'\1')
   assert call(u,0x487100,(7,0xffffffff))==0
   u.mem_write(0x879af8,b'\0');full_checks+=1
  else:
   assert result not in active
   if result in seen:reuses+=1
   seen.add(result);active.add(result)
 else:
  if invalid:status=-4
  else:
   assert slot in active;head=read(u,b+0x5cd0);call(u,0x48b8f0,(b+slot*0x318,),b)
   payload[slot*0x318:slot*0x318+4]=w(head);active.remove(slot);status=0
 assert bytes(u.mem_read(b,0x5cd0))==bytes(payload),'original payload changed beyond free links'
 metadata=[index(read(u,b+j*0x318)) for j in range(30)]+[index(read(u,b+0x5cd0)),read(u,b+0x5ce8),read(u,b+0x5ce4),sum(1<<j for j in active)]
 assert metadata[31]==len(active) and read(u,b+0x5cdc)==30-len(active)
 want=w(status,result,*metadata);expected.append(want)
 x.mem_write(output,w(0xffffffff))
 got=call(x,entries['acquire'],(b,output)) if op==0 else call(x,entries['release'],(b,slot))
 assert w(got,read(x,output))+bytes(x.mem_read(b,136))==want,('NXDK',len(cases)-1)
for cycle in range(20):
 while len(active)<30:step(0)
 step(0);step(0)
 slots=list(active);rng.shuffle(slots)
 for slot in slots:step(1,slot)
 step(1,slots[-1],True)
for i in range(4096):
 if active and rng.randrange(100)<48:step(1,rng.choice(sorted(active)))
 else:step(0)
for slot in sorted(active):step(1,slot)
for slot in (0,29,30,0xffffffff):step(1,slot,True)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-pool'],input=b''.join(w(*c) for c in cases))
assert len(pc)==144*len(cases)
for i,want in enumerate(expected):assert pc[i*144:(i+1)*144]==want,('PC',i)
report=dict(result='PASS',operations=len(cases),saturation_checks=full_checks,reused_slots=reuses,peak=read(u,b+0x5ce4),final_live=len(active),metadata_bytes=136,original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Original48b7b0/48b870/48b8f0 and saturated487100 type7 gate, shared PC/NXDK slot order, occupancy and peak. No heap fallback; original caller caps30 even when fallback enabled. Payload preserved except original free-link words; shared metadata lives separately. No corpse construction, resources or native XEMU invocation.')
(root/'artifacts/corpse-pool-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
