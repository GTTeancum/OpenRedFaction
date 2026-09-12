"""Original4ffa80 string ownership versus bounded corpse names on PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
b=0x30000000;source=b+0x6000;stack=b+0x1e000;stop=b+0x1f000

def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32)
 u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(b,0x20000);return u
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');read=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
live={};trace=[];failed=False
mapping=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
xmalloc=sym('malloc');xfree=sym('free')
def heap(cpu,address,size,data):
 if address not in ((0x573619,0x57360e) if cpu is u else (xmalloc,xfree)):return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=read(cpu,sp+4)
 if address in (0x573619,xmalloc):
  assert 0<arg<=256;trace.append(('allocate',arg))
  pointer=0 if failed else next(b+0x8000+512*i for i in range(4) if b+0x8000+512*i not in live)
  if pointer:live[pointer]=arg;cpu.mem_write(pointer,bytes([0xa5])*arg)
  cpu.reg_write(UC_X86_REG_EAX,pointer)
 else:
  assert arg in live;trace.append(('free',live.pop(arg)));cpu.mem_write(arg,bytes([0xdd])*256)
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(cpu,sp))
u.hook_add(UC_HOOK_CODE,heap);x.hook_add(UC_HOOK_CODE,heap)
def call(cpu,address,args=(),ecx=0):
 cpu.mem_write(stack,w(stop,*args));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,ecx)
 cpu.emu_start(address,stop,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==stop;return cpu.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4ffa80);commands=[];expected=[];traces=[]
for i in range(1024):
 kind=i%2;mode=1 if i%11==0 else 2 if i%7==0 else 0;length=rng.choice([0,1,3,3,15,31,127,255]);text=bytes(rng.randrange(97,123) for _ in range(length))
 commands.append(w(kind,length,mode)+text);u.mem_write(source,text+b'\0');pointer=source if mode==0 else 0 if mode==1 else read(u,b+kind*8+4);trace.clear()
 assert call(u,0x4ffa80,(pointer,),b+kind*8)==b+kind*8
 length=read(u,b+kind*8);pointer=read(u,b+kind*8+4);payload=bytes(u.mem_read(pointer,length+1)) if pointer else b''
 total=sum(read(u,b+j*8)+1 for j in range(2) if read(u,b+j*8+4))
 expected.append(w(0,length,bool(pointer),total)+payload);traces.append(trace[:])
 u.mem_write(source,bytes([0xa5])*256)
 if pointer:assert bytes(u.mem_read(pointer,length+1))==payload
for kind in (0,1):call(u,0x4ffa80,(0,),b+kind*8)
assert not live
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-name'],input=b''.join(commands));assert pc==b''.join(expected),'PC name mismatch'
init=sym('rf_corpse_owners_init');acquire=sym('rf_corpse_owners_acquire');assign=sym('rf_corpse_name_assign');recycle=sym('rf_corpse_owners_recycle');base=19224;seed=b+0x5000;out=seed+128;names=b+136+620
assert call(x,init,(b,base+1024))==0
x.mem_write(seed,f(10,3,0,0,0,1,0,0,0,1,0,0,0,1,1)+w(0,0,0x33))
material=struct.unpack('<3I',f(.25,.5,2));assert call(x,acquire,(b,seed,*material,out))==0
for i,(wire,want) in enumerate(zip(commands,expected)):
 kind,length,mode=struct.unpack('<3I',wire[:12]);x.mem_write(source,wire[12:]+b'\0');pointer=source if mode==0 else 0 if mode==1 else read(x,names+kind*8+4);trace.clear()
 assert call(x,assign,(b,0,kind,pointer))==0 and trace==traces[i],('trace',i,trace,traces[i])
 length=read(x,names+kind*8);pointer=read(x,names+kind*8+4);payload=bytes(x.mem_read(pointer,length+1)) if pointer else b''
 got=w(0,length,bool(pointer),read(x,b+base-8)-base-24)+payload;assert got==want,('NXDK name',i)
 x.mem_write(source,bytes([0xa5])*256)
 if pointer:assert bytes(x.mem_read(pointer,length+1))==payload
for kind in (0,1):assert call(x,assign,(b,0,kind,0))==0
# Budget failure preserves the old name; heap failure after replacement frees it.
x.mem_write(source,b'kept\0');assert call(x,assign,(b,0,1,source))==0;pointer=read(x,names+12)
x.mem_write(b+base-4,w(base+29));trace.clear();x.mem_write(source,b'longer replacement\0')
assert call(x,assign,(b,0,1,source))==0xfffffffc and not trace and read(x,names+12)==pointer and bytes(x.mem_read(pointer,5))==b'kept\0'
trace.clear();assert call(x,recycle,(b,0))==0xfffffffc and not trace and read(x,b+124)==1
x.mem_write(b+base-4,w(base+1024));failed=True;trace.clear()
assert call(x,assign,(b,0,1,source))==0xfffffffc and trace==[('free',5),('allocate',19)]
failed=False;assert bytes(x.mem_read(names+8,8))==bytes(8) and read(x,b+base-8)==base+24
assert call(x,recycle,(b,0))==0 and not live and read(x,b+base-8)==base
report=dict(result='PASS',assignments=1024,original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),xbox_fixed_bytes=base,scope='Complete original4ffa80 with heap supplied vs PC/NXDK two owned corpse names. Null/empty/self/same-length/replacement assignments, exact allocation/free traces, borrowed source overwrite, accounting, short budget, failed replacement allocation and recycle guard. Live constructor/deleter effect binding remains open.')
(root/'artifacts/corpse-name-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
