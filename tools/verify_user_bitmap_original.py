"""Audit original USERBMAP classification, creation and normal-load dispatch."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));data=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(data)+4095)&~4095);u.mem_write(base,data);u.mem_map(0,4096)
B=0x30000000;u.mem_map(B,65536);OWNER=B+0x1000;STACK=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
r=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
trace=[]
def text_at(a):
 out=bytearray()
 while u.mem_read(a,1)!=b'\0':out+=u.mem_read(a,1);a+=1
 return out.decode('ascii')
def hook(cpu,address,size,context):
 sp=rsp=cpu.reg_read(UC_X86_REG_ESP)
 if address==0x5102e0:result=OWNER;trace.append('allocate')
 elif address==0x50f580:trace.append(['lookup',text_at(r(sp+4))]);result=-1
 else:trace.append(['fallback',text_at(r(sp+4))]);result=12345
 cpu.reg_write(UC_X86_REG_EAX,result&0xffffffff);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for a in (0x5102e0,0x50f580,0x510470):u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def run(fn,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.emu_start(fn,STOP,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==STOP
 return u.reg_read(UC_X86_REG_EAX)
classifications={}
for name,expected in [('USERBMAP',3),('userbmap',3),('UsErBmAp',3),('USERBMAP.tga',2),('misc-static.tga',2),('foo.vbm',5),('foo.pcx',1),('foo.vaf',4),('foo.m2v',6),('missing',0)]:
 u.mem_write(B,name.encode()+b'\0');got=run(0x50fbf0,(B,));assert got==expected;classifications[name]=got
created=0
for fmt,bpp in [(0,1),(1,1),(2,1),(3,2),(4,2),(5,2),(6,3),(7,4),(8,2),(9,1)]:
 for width,height in [(1,1),(32,32),(64,128),(256,64),(65537,65538)]:
  initial=bytearray(b'\xa5'*108);struct.pack_into('<I',initial,0x24,12345);u.mem_write(OWNER,bytes(initial));trace=[]
  assert run(0x5119c0,(fmt,width,height))==12345 and trace==['allocate']
  expected=initial.copy();expected[:9]=b'USERBMAP\0'
  for off,value in [(0x20,-1),(0x30,(width&65535)*(height&65535)),(0x34,3),(0x38,0),(0x3c,fmt),(0x48,0),(0x50,0),(0x60,width*height*bpp)]:struct.pack_into('<I',expected,off,value&0xffffffff)
  struct.pack_into('<HH',expected,0x2c,width&65535,height&65535)
  for off,value in [(0x40,1),(0x44,0),(0x5d,0)]:expected[off]=value
  assert bytes(u.mem_read(OWNER,108))==expected,(fmt,width,height);created+=1
loads=0
u.mem_write(0x64ecbb,b'\0') # Ordinary loader mode; no forced generated-texture mode.
for name in ['USERBMAP','userbmap','UsErBmAp']:
 u.mem_write(B,name.encode()+b'\0');trace=[]
 assert run(0x50f6e0,(B,0xffffffff))==12345
 assert trace==[['lookup',name],['fallback',name]],trace;loads+=1
report=dict(result='PASS',classifications=classifications,constructor_cases=created,normal_load_cases=loads,original_sha256=sha,scope='Full original classifier and constructor with allocator supplied; full ordinary loader for uncached USERBMAP with cache miss and fallback supplied. Original descriptor uses type3, name USERBMAP, no pixel allocation in constructor. Normal loading dispatches to510470 missing-resource fallback. Fallback pixels and runtime face assignment remain unverified.')
(root/'artifacts/user-bitmap-original.json').write_text(json.dumps(report,indent=2));print(report)
