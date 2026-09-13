"""Prepared spotlight plane/AABB test against actual original and both builds."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OWNER=B+0x1000;OUT=B+0x2000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe')
mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_visibility_light_roots\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(entry,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
from unicorn import UC_HOOK_CODE
ROOM=B+0x3000;FACES=B+0x4000;TABLE=B+0x5000;GROUP=B+0x5100;CALLBACK=STOP-64
trace=[];xtrace=[];fail=0
def old_bounds(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);ret,source,minimum,maximum=struct.unpack('<4I',cpu.mem_read(sp,16))
 if minimum==ROOM+0x48:hit=1
 else:
  ident,hit=struct.unpack('<2f',cpu.mem_read(minimum,8));trace.append(int(ident))
 cpu.reg_write(UC_X86_REG_EAX,int(hit));cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
def new_bounds(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);ret,ctx,minimum,maximum,out=struct.unpack('<5I',cpu.mem_read(sp,20))
 ident,hit=struct.unpack('<2f',cpu.mem_read(minimum,8));xtrace.append(int(ident))
 if fail!=len(xtrace):cpu.mem_write(out,w(int(hit)))
 cpu.reg_write(UC_X86_REG_EAX,0xffffffff if fail==len(xtrace) else 0);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
o.hook_add(UC_HOOK_CODE,old_bounds,begin=0x4d8130,end=0x4d8130);x.hook_add(UC_HOOK_CODE,new_bounds,begin=CALLBACK,end=CALLBACK)
rng=random.Random(0x4d8a29);commands=[];expected=[]
for case in range(1024):
 rooms=[];primary=rng.sample(range(8),3);children=[rng.randrange(8) for _ in range(6)]
 o.mem_write(ROOM,bytes(512));o.mem_write(OWNER,bytes(268));o.mem_write(OWNER+8,w(2));o.mem_write(ROOM+0x9c,w(3,3,TABLE));o.mem_write(TABLE,w(*(FACES+i*128 for i in primary)))
 for i in range(8):
  bounds=f(i,1 if case%8==0 else rng.randrange(2),0,i+1,2,1);root_token=0x1000+i;first=2*(i%3);count=rng.randrange(3)
  rooms.append(bounds+w(root_token,first,count));at=FACES+i*128;o.mem_write(at,bytes(128));o.mem_write(at,bytes([0,case%256]));o.mem_write(at+8,bounds);o.mem_write(at+0x3c,w(root_token));o.mem_write(at+0x6c,w(count,count,GROUP+i*16));o.mem_write(GROUP+i*16,w(*(FACES+j*128 for j in children[first:first+count])))
 data=b''.join(rooms)+w(*primary,*children);commands.append(data);trace.clear()
 o.mem_write(STACK,w(STOP,OWNER,ROOM,0));o.reg_write(UC_X86_REG_ESP,STACK);o.emu_start(0x4d86d0,0x4d8ae9,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x4d8ae9
 count=o.reg_read(UC_X86_REG_EBP);sp=o.reg_read(UC_X86_REG_ESP);roots=bytes(o.mem_read(sp+0x1cc,count*4));output=w(0,count)+roots+b'\xa5'*(36-len(roots));expected.append(output+w(len(trace),*trace,*([0]*(65-len(trace)))))
 x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*40);xtrace.clear()
 assert call(x,entry,[B,8,B+288,3,B+300,6,OUT+4,9,OUT,CALLBACK,0])==0
 assert bytes(x.mem_read(OUT,40))==output[4:] and xtrace==trace,(case,trace,xtrace)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--light-dirty-roots'],input=b''.join(commands))==b''.join(expected)
# Capacity failure preserves selected; a rejected parent does not inspect children.
x.mem_write(B,data);x.mem_write(B+primary[0]*36+4,f(1));x.mem_write(OUT,b'\xa5'*40);xtrace.clear()
assert call(x,entry,[B,8,B+288,3,B+300,6,OUT+4,0,OUT,CALLBACK,0])==0xfffffffc
assert bytes(x.mem_read(OUT,40))==b'\xa5'*40 and xtrace==[primary[0]]
x.mem_write(B+primary[0]*36+4,f(0));x.mem_write(B+primary[0]*36+28,w(0xffffffff,0xffffffff));x.mem_write(OUT,w(123));xtrace.clear()
assert call(x,entry,[B,8,B+288,1,B+300,6,OUT+4,9,OUT,CALLBACK,0])==0
assert bytes(x.mem_read(OUT,4))==w(0) and xtrace==[primary[0]]
report=dict(result='PASS',original_pc_nxdk_cases=1024,nxdk_guards=2,scope='Original4d86d0 through root collection, with real primary/detail arrays and accessors; only bounds predicate supplied. Exact root order and predicate order on PC/NXDK, including parent misses, shared children, immediate-only descent and ignored room skip bytes. Traversal/native ownership excluded.')
(root/'artifacts/light-dirty-roots.json').write_text(json.dumps(report,indent=2));print(report)
