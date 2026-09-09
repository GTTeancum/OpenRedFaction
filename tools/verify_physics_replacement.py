"""Original class sphere installation block versus linked NXDK owned bodies."""
import hashlib,json,random,re,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_FPCW
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
read=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
base=0x30000000;cls=base+0x2000;source=base+0x4000;old=base+0x6000;new=base+0x8000;stack=base+0xd000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 cpu=Uc(UC_ARCH_X86,UC_MODE_32);cpu.mem_map(origin,(len(b)+4095)//4096*4096);cpu.mem_write(origin,b);cpu.mem_map(base,0x10000);return cpu
u=load(exe);x=load(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
u.mem_map(0,4096)
symbol=lambda n:int(re.search(r'_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entry=symbol('rf_physics_body_replace_spheres');malloc=symbol('malloc');free=symbol('free')
fail=False;calls=[]
def heap(cpu,address,size,data):
 alloc,release=(0x573619,0x57360e) if cpu==u else (malloc,free)
 if address not in (alloc,release):return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=read(cpu,sp+4)
 if cpu==x:calls.append((address,arg))
 if address==alloc:
  assert 0<arg<=384;cpu.mem_write(new,bytes([0xa5])*arg);cpu.reg_write(UC_X86_REG_EAX,0 if cpu==x and fail else new)
 else:assert arg in (0,old,new)
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(cpu,sp))
u.hook_add(UC_HOOK_CODE,heap);x.hook_add(UC_HOOK_CODE,heap)
def call(args):
 x.mem_write(stack,pack(stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop;return x.reg_read(UC_X86_REG_EAX)
def represented(b):
 return b[:12]+b[0x10:0xfc]+b[0x108:0x128]+b[0x138:0x148]+b[0x15c:0x160]+b[0x164:0x16c]
rng=random.Random(0x42405d);failures=0
for case in range(180):
 count=case%9;previous=(0,1,8)[case//9%3];flags=rng.getrandbits(32)
 body=bytearray(rng.randbytes(0x170));body[0x5c:0x68]=floats(*[rng.uniform(-100,100) for _ in range(3)])
 body[0xfc:0x108]=pack(previous,16 if previous else 0,old if previous else 0);body[0x120:0x124]=pack(flags)
 spheres=b''.join(floats(*[rng.uniform(-4,4) for _ in range(3)],rng.uniform(0,2),(.5,-1,0,float('nan'))[(case+i)%4])+pack(i+0x12340000) for i in range(count))
 class_records=b''.join(spheres[i*24+12:i*24+16]+bytes(12)+spheres[i*24+16:i*24+24]+spheres[i*24:i*24+12]+pack(i) for i in range(count))
 old_records=bytes([0xa5])*(previous*24)
 u.mem_write(base,bytes(0x1500));u.mem_write(base+0x88,bytes(body));u.mem_write(base+0x29c,pack(cls));u.mem_write(old,old_records or bytes(24))
 u.mem_write(cls+0xcec,pack(count)+class_records);u.mem_write(stack,bytes(256));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_FPCW,0x37f)
 try:u.emu_start(0x42405d,0x424168,count=100000)
 except Exception as error:raise RuntimeError((case,hex(u.reg_read(UC_X86_REG_EIP)))) from error
 assert u.reg_read(UC_X86_REG_EIP)==0x424168
 actual=bytes(u.mem_read(base+0x88,0x170));assert read(u,base+0x184)==count
 owned=bytes(u.mem_read(read(u,base+0x18c),count*24)) if count else b'';assert owned==spheres
 # Original writes only array metadata, radius/bounds and spring flag.
 for a,z in [(0,0xf8),(0x124,0x170)]:assert actual[a:z]==body[a:z]
 seed=bytes(represented(body))+pack(old if previous else 0,previous,12+previous*24,324+previous*24)
 x.mem_write(base,seed);x.mem_write(old,old_records or bytes(24));x.mem_write(source,spheres or bytes(24));calls.clear()
 peak=324+(previous+count)*24
 assert call([base,source,count,peak-1])==0xfffffffc and bytes(x.mem_read(base,324))==seed and not calls
 if count:
  fail=True;assert call([base,source,count,peak])==0xfffffffc;fail=False;failures+=1
  assert bytes(x.mem_read(base,324))==seed and bytes(x.mem_read(old,len(old_records)))==old_records;calls.clear()
 assert call([base,source,count,peak])==0
 assert bytes(x.mem_read(base,308))==represented(actual),case
 assert read(x,base+312)==count and read(x,base+320)==324+count*24
 assert (bytes(x.mem_read(read(x,base+308),count*24)) if count else b'')==spheres
 assert calls==([(malloc,count*24)] if count else [])+[(free,old if previous else 0)]
report=dict(result='PASS',original_nxdk_cases=180,nxdk_allocation_failures=failures,scope='Original 42405d..424168 and callees, only heap supplied; all represented body bytes and sphere records compared. Peak-budget and allocation failure preservation. Excludes prior pose/class construction, later crouch setup and live XEMU integration.')
(root/'artifacts/physics-replacement-verification.json').write_text(json.dumps(report,indent=2));print(report)
