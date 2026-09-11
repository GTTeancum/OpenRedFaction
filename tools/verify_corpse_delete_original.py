"""Trace complete original type7 deletion through registry/pool recycling.

Resource backends are supplied; list, flag and pool mutations execute original
code. This is evidence for live ownership reconstruction, not a shared port test.
"""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;u.mem_map(b,65536);actor=b;previous=b+0x1000;following=b+0x2000;sound=b+0x3000
stack=b+0xe000;stop=b+0xf000;pool=0x708748;free_node=0x73a880+12*20
read=lambda address:struct.unpack('<I',u.mem_read(address,4))[0]
def put(address,value):u.mem_write(address,w(value))
hooks={0x48c9f0:1,0x4ffa80:1,0x459a20:1,0x42ed20:2,0x49f1d0:1,0x502b10:1,0x497d80:1}
trace=[];found=False
def hook(machine,address,size,unused):
 if address not in hooks:return
 esp=machine.reg_read(UC_X86_REG_ESP);args=tuple(read(esp+4+4*j) for j in range(hooks[address]));ecx=machine.reg_read(UC_X86_REG_ECX)
 trace.append((address,ecx if address==0x4ffa80 else 0,args))
 # None of these effects may observe a recycled actor/registry slot.
 assert read(0x7394cc+4*7)==actor
 if address==0x459a20:machine.reg_write(UC_X86_REG_EAX,sound if found else 0)
 if address==0x497d80:
  assert read(actor+0x268)==args[0]
  # Original must save next before releasing an emitter. Poison released storage.
  machine.mem_write(args[0],bytes([0xdd])*0x154)
 machine.reg_write(UC_X86_REG_ESP,esp+4+(4 if address==0x4ffa80 else 0))
 machine.reg_write(UC_X86_REG_EIP,read(esp))
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x486670);releases=0;emitter_releases=0
for case in range(1024):
 u.mem_write(b,bytes(0x8000));trace.clear();found=bool(case&1)
 flags=rng.getrandbits(32);model=0 if case%3==0 else 0x12340000+case;burn=0 if case%4==0 else 0x43210000+case
 count=case%5;sid=0xffffffff if case%7==0 else case
 put(actor+0x24,7);put(actor+0x2c,0x81230007);put(actor+0x7c,flags);put(actor+0x80,model)
 put(actor+0x2cc,sid);put(actor+0x2d0,burn);put(sound+0x7c,0x500)
 # Exercise sentinel-adjacent and ordinary neighbours in both lists.
 prev=0x5cabb8 if case%2==0 else previous;next_=0x5cabb8 if case%3==0 else following
 put(actor+0x290,prev);put(actor+0x28c,next_);put(prev+0x28c,actor);put(next_+0x290,actor)
 put(actor+0x14,previous);put(actor+0x10,following);put(previous+0x10,actor);put(following+0x14,actor)
 put(0x5caed0,13);put(0x73a850,79);put(0x7394cc+4*7,actor)
 put(0x7394c0,free_node);put(0x7394c4,free_node);u.mem_write(free_node,w(0x7394c0,0x7394c0,20))
 put(pool+0x5cd0,b+0x7000);put(pool+0x5cdc,17);put(pool+0x5ce8,13)
 emitters=[b+0x4000+i*0x200 for i in range(count)]
 put(actor+0x268,emitters[0] if emitters else 0)
 for i,emitter in enumerate(emitters):put(emitter+0x150,emitters[i+1] if i+1<count else 0)
 u.mem_write(stack,w(stop,actor));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x486670,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 want=[(0x48c9f0,0,(actor,)),(0x4ffa80,actor+0x2a4,(0x5caedc,)),(0x459a20,0,(sid,))]
 if burn:want.append((0x42ed20,0,(burn,0)))
 want.append((0x49f1d0,0,(actor+0x88,)))
 if model and not flags&0x400:want.append((0x502b10,0,(model,)));releases+=1
 want.extend((0x497d80,0,(e,)) for e in emitters);emitter_releases+=count
 want.append((0x4ffa80,actor+0x18,(0x73db20,)))
 assert trace==want,(case,trace,want)
 assert read(actor+0x2cc)==(0xffffffff if found else sid)
 assert read(sound+0x7c)==(0x502 if found else 0x500)
 assert read(actor+0x2d0)==burn # release backend owns any burn-owner clearing
 assert read(actor+0x28c)==read(actor+0x290)==0
 assert read(prev+0x28c)==next_ and read(next_+0x290)==prev
 assert read(actor+0x10)==read(actor+0x14)==0
 assert read(previous+0x10)==following and read(following+0x14)==previous
 assert read(actor+0x268)==0 and read(0x5caed0)==12 and read(0x73a850)==78
 assert read(0x7394cc+4*7)==0
 assert read(pool+0x5cd0)==actor and read(actor)==b+0x7000
 assert read(pool+0x5cdc)==18 and read(pool+0x5ce8)==12
 returned=0x73a880+12*7
 assert read(0x7394c4)==returned and read(free_node)==returned
 assert read(returned)==0x7394c0 and read(returned+4)==free_node
 if 'observe_case' in globals():observe_case(globals())
report=dict(result='PASS',cases=1024,model_releases=releases,emitter_releases=emitter_releases,original_sha256=digest,scope='Complete original486670 type7,416ff0,489fc0,4867b0,48b8f0 and48ab40. Supplied resource backends; real corpse/object list unlink, sound deletion mark, model-release gate, emitter traversal, registry removal and pool recycling. Not shared PC/NXDK or native XEMU gameplay.')
(root/'artifacts/corpse-delete-original.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
