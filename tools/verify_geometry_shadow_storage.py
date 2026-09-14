"""Replay original4f25a0 shadow polygon area against PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text()
def symbol(name):return int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',mp)[1],16)
entry=symbol('rf_geometry_shadow_storage_open');close=symbol('rf_geometry_shadow_storage_close');H=0x40000000;x.mem_map(H,1024*1024);allocations=[];frees=[]
def allocation(u,a,size,ctx):
 sp=u.reg_read(UC_X86_REG_ESP);ret,n,width=struct.unpack('<3I',u.mem_read(sp,12));total=n*width;assert total<=1024*1024;allocations.append(total);u.mem_write(H,bytes(total));u.reg_write(UC_X86_REG_EAX,H);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
def release(u,a,size,ctx):
 sp=u.reg_read(UC_X86_REG_ESP);ret,p=struct.unpack('<2I',u.mem_read(sp,8));assert p in (0,H);frees.append(p);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,allocation,begin=symbol('calloc'),end=symbol('calloc'));x.hook_add(UC_HOOK_CODE,release,begin=symbol('free'),end=symbol('free'))
def call(address,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(address,STOP,count=10000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
maximum=0
for count in range(1,64):
 x.mem_write(OWNER,bytes(84));before=len(allocations);assert call(entry,[OWNER,3,12,16,128,64,64,count,1048576])==0;assert len(allocations)==before+1
 words=struct.unpack('<21I',x.mem_read(OWNER,84));storage,masks,intersection=words[:3];assert storage==H and words[17]==OWNER+44 and words[18:20]==(count,4164)
 total=words[20];maximum=max(maximum,total);assert total==allocations[-1]+84
 assert bytes(x.mem_read(masks,count*4164))==bytes([255])*(count*4164);assert bytes(x.mem_read(intersection,1024))==bytes(1024)
 for i in range(count):x.mem_write(masks+i*4164+4160,bytes([i]))
 for i in range(count):assert x.mem_read(masks+i*4164,1)[0]==255
 call(close,[OWNER]);assert bytes(x.mem_read(OWNER,84))==bytes(84);call(close,[OWNER])
 before=len(allocations);assert call(entry,[OWNER,3,12,16,128,64,64,count,total-1])!=0 and len(allocations)==before and bytes(x.mem_read(OWNER,84))==bytes(84)
 assert call(entry,[OWNER,3,12,16,128,64,64,count,total])==0;call(close,[OWNER])
report=dict(result='PASS',nxdk_mask_counts=63,max_resident_bytes=maximum,allocator_calls=len(allocations),scope='Compiled NXDK owner layout, one allocation, mask isolation, zero scratch, exact/short budgets and repeated close with emulated calloc/free hooks. Native allocator and whole-scene memory remain unverified.')
(root/'artifacts/geometry-shadow-storage.json').write_text(json.dumps(report,indent=2));print(report)
