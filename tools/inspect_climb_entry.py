"""Execute complete original climb entry; observe/skip only effect 48a9c0."""
import hashlib,itertools,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,65536);entity=base;cls=base+0x2000;region=base+0x4000;mode=base+0x6000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]);f=lambda *v:struct.pack('<'+'f'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
events=[]
def hook(cpu,address,size,data):
 if address==0x48a9c0:
  sp=cpu.reg_read(UC_X86_REG_ESP);args=struct.unpack('<7I',cpu.mem_read(sp+4,28))
  assert args==(entity,*struct.unpack('<3I',f(1,2,3)),18,0x3f800000,0)
  assert read(entity+0x13ec)==0 and read(entity+0x13f0)==region
  assert read(entity+0x858)==mode and read(entity+0x8c4)==0
  events.append('effect_18')
  cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
 elif address==0x427450:events.append('speed')
 elif address==0x4339d0:events.append('descriptor')
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x4281e0);results=[]
for climb,movement,kind,attached,region_kind,descriptor_enabled in itertools.product((0,1),range(16),(0,1),(-1,0),(0,1,2),(0,1)):
 seed=bytearray(rng.randbytes(0x1500));seed[0x3c:0x48]=f(1,2,3);seed[0x24:0x28]=w(0)
 seed[0x294:0x298]=w(cls);seed[0x75c:0x760]=w(-1);seed[0x858:0x85c]=w(mode)
 seed[0x8c4:0x8c8]=w(0);seed[0x1380:0x1384]=w(attached)
 u.mem_write(entity,bytes(seed));u.mem_write(cls,bytes(0x2000));u.mem_write(cls+0x724,w(climb*4));u.mem_write(cls+0x1b4,w(kind));u.mem_write(cls+0x50,f(3.5))
 u.mem_write(mode,w(1,movement));u.mem_write(region,w(region_kind)+bytes(60))
 u.mem_write(0x64ecb9,b'\0');u.mem_write(0x62fe90,bytes([descriptor_enabled]));u.mem_write(0x630050,w(77))
 u.mem_write(stack,w(stop,entity,region));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 events.clear();u.emu_start(0x4281e0,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 free=movement in (3,8) or (kind==1 and attached==-1)
 want=bytearray(seed)
 if climb:
  want[0x13ec:0x13f4]=w(0,region);want[0x8c0:0x8c8]=f(3.5)+w(1)
  want[0x858:0x860]=w(0x62fe90 if descriptor_enabled else 0x62fe50,region+0x10)
  want[0x8ac:0x8b0]=w(-1)
 assert bytes(u.mem_read(entity,len(seed)))==want
 assert read(0x630050)==((2 if descriptor_enabled else 0) if climb else 77)
 assert events==((['effect_18'] if free and region_kind==2 else [])+['speed','descriptor'] if climb else []),(climb,movement,kind,attached,region_kind,descriptor_enabled,events)
 results.append(dict(climb=climb,movement=movement,kind=kind,attachment=attached,region_kind=region_kind,descriptor_enabled=descriptor_enabled,events=list(events)))
report=dict(result='PASS',cases=len(results),original_sha256=sha,
 scope='Complete 4281e0 with unchanged capability predicate, free-motion predicate, speed setter and descriptor lookup. Only 48a9c0 effect is observed and skipped; its argument and intermediate entity state are checked. Complete actor bytes checked. Not live traversal or shared-C climb transition.',results=results)
(root/'artifacts/climb-entry-reference.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'original climb-entry cases')
