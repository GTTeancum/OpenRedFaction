"""Original player-triggered mover AI alert gate with real entity predicates."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP
base=0x30000000;stack=base+0xe000;stop=base+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(b,(len(im)+4095)//4096*4096);m.mem_write(b,im);m.mem_map(base,65536);return m
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
u=machine(original);x=machine(root/'build/xbox/main.exe');requests=[]
entry=int(re.search(r'_rf_entity_controller_alert\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def alert(m,a,s,c):
 sp=m.reg_read(UC_X86_REG_ESP);ret,actor,position,radius,mode,extra=struct.unpack('<6I',m.mem_read(sp,24))
 assert actor==query and position==base+0x603c and radius==0x41200000 and mode==extra==0
 requests.append(actor);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,alert,begin=0x408280,end=0x408280)
rng=random.Random(0x46acb8);commands=bytearray();expected=bytearray();fired=0
for n in range(2048):
 handles=[((i+1)<<16)|i for i in range(3)];queries=handles+[-1,1024,0x90000]
 query=rng.choice(queries);local=n%4;gate1=rng.choice([0,1,2,255,256,257]);gate2=rng.choice([0,1,2,255,256,257])
 nodes=[[handles[i],rng.choice([0,0,4,8]),rng.choice([0,1,4]),rng.choice([0,8,0x4008]),rng.choice([0,0x800]),rng.choice(queries)] for i in range(3)]
 commands.extend(w(query,local,gate1,gate2,*(v for row in nodes for v in row)))
 u.mem_write(0x7394cc,bytes(4096));x.mem_write(base,bytes(4096))
 for i,(handle,kind,cls,flags,flags810,linked) in enumerate(nodes):
  ptr=base+0x1000+i*0x1000;info=ptr+0x900;u.mem_write(ptr,bytes(4096))
  for off,value in ((0x24,kind),(0x2c,handle),(0x7c,flags),(0x810,flags810),(0x200,linked),(0x294,info)):
   u.mem_write(ptr+off,w(value))
  u.mem_write(info+0x1b4,w(cls));u.mem_write(info+0x44,w(cls));u.mem_write(0x7394cc+i*4,w(ptr))
  compact=base+0x1000+i*0x100;x.mem_write(compact,bytes(56));x.mem_write(compact,w(handle,kind,cls,flags,flags810));x.mem_write(compact+28,w(linked));x.mem_write(base+i*4,w(compact))
 u.mem_write(0x5cb054,w(base+0x1000+local*0x1000 if local<3 else 0));u.mem_write(0x7cabd4,bytes([gate1&255]));u.mem_write(0x7cabb0,bytes([gate2&255]))
 u.mem_write(stack,w(base+0x6000));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,base+0x6000);u.reg_write(UC_X86_REG_EBP,query&0xffffffff);requests.clear()
 u.emu_start(0x46acb8,0x46ad06,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x46ad06
 assert len(requests)<=1;want=w(0,len(requests));expected.extend(want);fired+=len(requests)
 x.mem_write(base+0x2000,w(0xa5a5a5a5));x.mem_write(stack,w(stop,base,query,base+0x1000+local*0x100 if local<3 else 0,gate1,gate2,base+0x2000));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x2000,4))==want,('NXDK',n)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--controller-alert'],input=commands);assert actual==expected
report=dict(result='PASS',cases=2048,requests=fired,original_sha256=digest,scope='Original 46acb8..46ad06 and actual actor/local linked-entity predicates execute; only downstream 408280 AI alert intercepted. Actor, controller position, radius10 and zero modes checked. Exact PC/NXDK decision with missing/stale/type/class/flag and low-byte gates. AI stimulus execution and live entity snapshots excluded.')
(root/'artifacts/controller-alert-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
