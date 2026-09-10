"""Resolve trigger actor facts against unchanged original predicates."""
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
entry=int(re.search(r'_rf_trigger_actor_resolve\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
rng=random.Random(0x48aaf0);commands=bytearray();expected=bytearray()
def call(address,arg):
 u.mem_write(stack,w(stop,arg));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(address,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop;return u.reg_read(UC_X86_REG_EAX)
for n in range(2048):
 handles=[i+rng.choice([0,0x10000,0x80000000]) for i in range(4)]
 queries=handles+[-1,1024,0x20000,0x10003]
 nodes=[[handles[i],rng.choice([0,0,2,4,5]),rng.randrange(5),rng.getrandbits(8),rng.choice(queries)] for i in range(4)]
 owner=rng.choice(queries);attached=rng.choice(queries);players=[rng.choice(queries) for _ in range(4)]
 commands.extend(w(*(v for node in nodes for v in node),owner,attached,*players))
 u.mem_write(0x7394cc,bytes(4096));x.mem_write(base,bytes(4096))
 for i,(handle,kind,classification,flags,linked) in enumerate(nodes):
  p=base+0x2000+i*0x1000;info=p+0x800;u.mem_write(p,bytes(0x1000))
  for offset,value in ((0x2c,handle),(0x24,kind),(0x7c,flags),(0x200,linked),(0x294,info)):u.mem_write(p+offset,w(value))
  u.mem_write(info+0x1b4,w(classification));u.mem_write(info+0x44,w(classification));u.mem_write(0x7394cc+i*4,w(p))
  compact=base+0x2000+i*0x100;x.mem_write(compact,bytes(56));x.mem_write(compact,w(handle,kind,classification,flags));x.mem_write(compact+28,w(linked));x.mem_write(base+i*4,w(compact))
 u.mem_write(0x7c75cc,w(base+0x7000))
 for i,handle in enumerate(players):u.mem_write(base+0x7000+i*24,w(base+0x7000+((i+1)%4)*24,0,0,0,0,handle))
 actor=base+0x2000;entity=call(0x426fc0,handles[0]);owner_ptr=call(0x40a0e0,owner)
 facts=[handles[0],nodes[0][1],call(0x4895d0,actor)&255,call(0x48aaf0,actor)&255,int(bool(entity)),call(0x429990,entity)&255,call(0x48aaf0,owner_ptr)&255,call(0x4290d0,entity)&255,int(bool(call(0x410c70,attached)))]
 want=w(0,*facts);expected.extend(want)
 x.mem_write(base+0x1000,w(*players));x.mem_write(base+0x1100,bytes([0xa5])*36);x.mem_write(stack,w(stop,base,base+0x2000,owner,attached,base+0x1000,4,base+0x1100));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x1100,36));assert got==want,('NXDK',n,got.hex(),want.hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--trigger-actor'],input=commands);assert actual==expected
report=dict(result='PASS',cases=2048,original_sha256=digest,scope='All actor-fact predicates and original object/entity/player-list callees execute unchanged, no hooks. Exact PC/NXDK facts using shared compact entity registry. Covers generation mismatch, negative handles, wrong kinds/classes, object flag8, owners and player attachment chains. Caller-owned snapshots; live entity creation and update order excluded.')
(root/'artifacts/trigger-actor-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
