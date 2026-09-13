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
mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_visibility_light_faces\s+([0-9a-fA-F]+)',mp)[1],16)
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
rng=random.Random(0x4d8c29);commands=[];expected=[]
for case in range(2048):
 mode=[0,1,255,256][case%4];update=[0,1,255,256][case//4%4];dirty=bytes(rng.randrange(256) for _ in range(8));faces=[]
 o.mem_write(ROOM,bytes(512));o.mem_write(OWNER,bytes(268));o.mem_write(OWNER+8,w(2));o.mem_write(ROOM+0x70,w(FACES));o.mem_write(ROOM+0xc0,w(8,8,TABLE));o.mem_write(TABLE,w(*(GROUP+i*16 for i in range(8))))
 for i in range(8):o.mem_write(GROUP+i*16,bytes(8)+dirty[i:i+1]+bytes(7))
 for i in range(16):
  bounds=f(i,rng.randrange(2),0,i+1,2,1);flags=rng.randrange(1<<24);prop=rng.choice([-32768,-1,0,1,32767]);index=rng.choice([-32768,-1,*range(8)])
  face=bounds+w(flags,prop&0xffffffff,index&0xffffffff);faces.append(face)
  at=FACES+i*96;o.mem_write(at,bytes(96));o.mem_write(at+0x10,bounds);o.mem_write(at+0x28,w(flags));o.mem_write(at+0x34,struct.pack('<hh',prop,index));o.mem_write(at+0x54,w(at+96 if i<15 else 0))
 data=w(mode,update,0)+b''.join(faces)+dirty;commands.append(data);o.mem_write(0x879af8,bytes([mode&255]));trace.clear()
 assert call(o,0x4d86d0,[OWNER,ROOM,update]) is not None
 outfaces=[]
 for i,face in enumerate(faces):outfaces.append(face[:24]+bytes(o.mem_read(FACES+i*96+0x28,4))+face[28:])
 outdirty=bytes(o.mem_read(GROUP+i*16+8,1)[0] for i in range(8));tracewire=w(len(trace),*trace,*([0]*(17-len(trace))))
 response=w(0)+b''.join(outfaces)+outdirty+tracewire;expected.append(response)
 x.mem_write(B,data);xtrace.clear();assert call(x,entry,[B+12,16,B+12+576,8,mode,update,CALLBACK,0])==0
 assert bytes(x.mem_read(B+12,584))==b''.join(outfaces)+outdirty and xtrace==trace,case
# A later callback failure leaves earlier shared-dirty updates committed.
faces=[f(i,1,0,i+1,2,1)+w(0,0,i%8) for i in range(16)]
data=w(1,1,2)+b''.join(faces)+bytes(8);commands.append(data);x.mem_write(B,data);fail=2;xtrace.clear()
assert call(x,entry,[B+12,16,B+588,8,1,1,CALLBACK,0])==0xffffffff
assert xtrace==[0,1] and bytes(x.mem_read(B+588,8))==bytes([3,0,0,0,0,0,0,0])
expected.append(w(0xffffffff)+b''.join(faces)+bytes([3,0,0,0,0,0,0,0])+w(2,0,1,*([0]*15)))
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--light-dirty-faces'],input=b''.join(commands))==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=2048,callback_failure_cases=1,scope='Full original4d86d0 flat-solid path, with actual face property, index, shared dirty byte and linked-list traversal. Bounds predicate supplied and call order checked, parent bounds accepted. Includes shared indices, signed-word boundaries and low-byte mode/update. Room tree/root traversal and shape math excluded; shape tests have separate verification.')
(root/'artifacts/light-dirty-faces.json').write_text(json.dumps(report,indent=2));print(report)
