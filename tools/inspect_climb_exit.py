"""Complete original climb exit; supply only collision result and ground boundary."""
import hashlib,itertools,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,65536);cls=base+0x2000;name=base+0x4000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]);f=lambda *v:struct.pack('<'+'f'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
events=[];blocked=0
names=[bytes(u.mem_read(read(0x596384+i*4),64)).split(b'\0')[0] for i in range(16)]
def hook(cpu,address,size,data):
 if address in (0x499ed0,0x4a0840):
  sp=cpu.reg_read(UC_X86_REG_ESP);events.append('query' if address==0x499ed0 else 'ground')
  if address==0x499ed0:cpu.reg_write(UC_X86_REG_EAX,blocked)
  cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
 elif address==0x427450:events.append('speed')
 elif address==0x433a00:events.append('named_descriptor')
u.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x4280b0);results=[]
for walk,crouched,blocked,index,enabled,forced in itertools.product((0,1),(0,1),(0,1),(-1,0,1,3),(0,1),(-1,0)):
 seed=bytearray(rng.randbytes(0x1500));flags=rng.getrandbits(32)&~0x400;flags|=crouched*0x400
 seed[0x294:0x298]=seed[0x29c:0x2a0]=w(cls);seed[0x2c:0x30]=w(-1);seed[0x3c:0x48]=f(1,2,3)
 seed[0x184:0x190]=w(0,0,0);seed[0x75c:0x760]=w(forced);seed[0x810:0x814]=w(flags)
 u.mem_write(base,bytes(seed));u.mem_write(cls,bytes(0x2000));u.mem_write(cls+0x724,w(walk));u.mem_write(cls+0x50,f(3.5,.5));u.mem_write(cls+0xf74,f(.625))
 u.mem_write(cls+0x34,w(name));u.mem_write(name,(names[index] if index>=0 else b'unknown movement')+b'\0')
 u.mem_write(0x7c75cc,w(0));u.mem_write(0x64ecb9,b'\0');u.mem_write(0x630050,w(77))
 if index>=0:u.mem_write(0x62fe50+index*32,bytes([enabled]))
 u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 events.clear();u.emu_start(0x4280b0,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 stopped=walk and crouched and blocked;want=bytearray(seed);selected=77
 if not stopped:
  want[0x8c0:0x8c8]=f(1.75 if forced!=-1 else 3.5)+w(0 if forced!=-1 else 1)
  if walk:
   if crouched:want[0x810:0x814]=w(flags&~0x400)
   want[0x13ec:0x13f0]=w(0);want[0x148:0x14c]=w(0)
   if index>=0:selected=index if enabled else 0
   want[0x858:0x860]=w(0 if index<0 else 0x62fe50+selected*32,0x73a858)
 assert bytes(u.mem_read(base,len(seed)))==want
 assert read(0x630050)==selected
 expected=(['query'] if stopped else ((['query','ground'] if crouched else [])+['speed','named_descriptor']) if walk else ['speed'])
 assert events==expected
 results.append(dict(walk=walk,crouched=crouched,blocked=blocked,default_index=index,enabled=enabled,forced_action=forced,events=list(events)))
report=dict(result='PASS',cases=len(results),original_sha256=sha,
 scope='Complete original 4280b0 and unchanged callees, including standing, speed setter and named descriptor lookup. Collision result supplied; ground refresh skipped. Complete actor preservation and descriptor global checked; no live geometry or exit integration.',results=results)
(root/'artifacts/climb-exit-reference.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'original climb-exit cases')
