"""Compiled NXDK retained lighting scratch ownership with allocator hooks."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00

def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text()
def symbol(name):return int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',mp)[1],16)
entry=symbol('rf_geometry_lightmap_storage_open');close=symbol('rf_geometry_lightmap_storage_close')
H=0x40000000;x.mem_map(H,1024*1024);allocations=[];frees=[];fail=False
def allocation(u,a,size,ctx):
 sp=u.reg_read(UC_X86_REG_ESP);ret,n,width=struct.unpack('<3I',u.mem_read(sp,12));total=n*width;assert total<=1024*1024
 allocations.append(total)
 if not fail:u.mem_write(H,bytes(total))
 u.reg_write(UC_X86_REG_EAX,0 if fail else H);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
def release(u,a,size,ctx):
 sp=u.reg_read(UC_X86_REG_ESP);ret,p=struct.unpack('<2I',u.mem_read(sp,8));assert p in (0,H);frees.append(p)
 u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,allocation,begin=symbol('calloc'),end=symbol('calloc'))
x.hook_add(UC_HOOK_CODE,release,begin=symbol('free'),end=symbol('free'))
def call(address,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(address,STOP,count=10000000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
maximum=0
for n in range(1,65):
 x.mem_write(OWNER,bytes(48));before=len(allocations)
 args=[OWNER,n*64,n,n*4,n*2,1048576]
 assert call(entry,args)==0 and len(allocations)==before+1
 words=struct.unpack('<12I',x.mem_read(OWNER,48));storage,c0,c1,c2,pixels,polys,verts,normals,np,nv,nn,total=words
 assert storage==polys==H and (pixels,np,nv,nn)==(n*64,n,n*4,n*2)
 assert verts==H+n*8 and normals==verts+n*4*32 and c0==normals+n*2*20
 assert c1==c0+n*256 and c2==c1+n*256
 assert total==allocations[-1]+48 and c2+n*256==H+allocations[-1]
 assert bytes(x.mem_read(H,allocations[-1]))==bytes(allocations[-1])
 maximum=max(maximum,total)
 for j,pointer in enumerate((polys,verts,normals,c0,c1,c2)):x.mem_write(pointer,w(j+1))
 for j,pointer in enumerate((polys,verts,normals,c0,c1,c2)):assert bytes(x.mem_read(pointer,4))==w(j+1)
 saved=bytes(x.mem_read(OWNER,48));assert call(entry,args)!=0 and bytes(x.mem_read(OWNER,48))==saved
 call(close,[OWNER]);call(close,[OWNER]);assert bytes(x.mem_read(OWNER,48))==bytes(48)
 before=len(allocations);args[-1]=total-1
 assert call(entry,args)!=0 and len(allocations)==before and bytes(x.mem_read(OWNER,48))==bytes(48)
 args[-1]=total;assert call(entry,args)==0;call(close,[OWNER])
assert call(entry,[OWNER,0xffffffff,0xffffffff,0xffffffff,0xffffffff,0xffffffff])!=0
assert bytes(x.mem_read(OWNER,48))==bytes(48)
assert call(entry,[OWNER,1,0,0,0,60])==0;call(close,[OWNER])
fail=True
assert call(entry,[OWNER,1,0,0,0,60])!=0 and bytes(x.mem_read(OWNER,48))==bytes(48)
report=dict(result='PASS',nxdk_sizes=64,max_resident_bytes=maximum,allocator_calls=len(allocations),
 scope='Compiled NXDK single-allocation workspace layout, six disjoint arrays, zero scratch, exact/short budgets, occupied output, integer overflow, zero polygon capacities, allocation failure and repeated close. Allocator hooks; native full-scene memory remains separate.')
(root/'artifacts/geometry-lightmap-storage.json').write_text(json.dumps(report,indent=2));print(report)
