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
mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_visibility_light_tree\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(entry,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
from unicorn import UC_HOOK_CODE
ROOM=B+0x3000;FACES=B+0x4000;TABLE=B+0x5000;GROUP=B+0x5100;CALLBACK=STOP-64
trace=[];xtrace=[];fail=0
def old_bounds(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);ret,source,minimum,maximum=struct.unpack('<4I',cpu.mem_read(sp,16))
 if minimum in (B+0x7108,B+0x7208):hit=1
 else:
  ident,hit=struct.unpack('<2f',cpu.mem_read(minimum,8));trace.append(int(ident))
 cpu.reg_write(UC_X86_REG_EAX,int(hit));cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
def new_bounds(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);ret,ctx,minimum,maximum,out=struct.unpack('<5I',cpu.mem_read(sp,20))
 ident,hit=struct.unpack('<2f',cpu.mem_read(minimum,8));xtrace.append(int(ident))
 if fail!=len(xtrace):cpu.mem_write(out,w(int(hit)))
 cpu.reg_write(UC_X86_REG_EAX,0xffffffff if fail==len(xtrace) else 0);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
o.hook_add(UC_HOOK_CODE,old_bounds,begin=0x4d8130,end=0x4d8130);x.hook_add(UC_HOOK_CODE,new_bounds,begin=CALLBACK,end=CALLBACK)
NODES=B+0x6000;ROOTS=B+0x7000
rng=random.Random(0x4d8af8);commands=[];expected=[]
children={i:(i*2+1,i*2+2) for i in range(7)}
for case in range(1024):
 mode=[0,1,255,256][case%4];update=[0,1,255,256][case//4%4];dirty=bytes(rng.randrange(4) for _ in range(8));faces=[];nodes=[]
 o.mem_write(ROOM,bytes(512));o.mem_write(OWNER,bytes(268));o.mem_write(OWNER+8,w(2));o.mem_write(ROOM+0x9c,w(2,2,ROOTS));o.mem_write(ROOTS,w(B+0x7100,B+0x7200))
 for at,root_index in [(B+0x7100,0),(B+0x7200,0 if case%2 else 7)]:o.mem_write(at,bytes(128));o.mem_write(at+0x3c,w(NODES+root_index*48))
 o.mem_write(ROOM+0xc0,w(8,8,TABLE));o.mem_write(TABLE,w(*(GROUP+i*16 for i in range(8))))
 for i in range(8):o.mem_write(GROUP+i*16,bytes(8)+dirty[i:i+1]+bytes(7))
 for i in range(15):
  bounds=f(i,rng.randrange(2),0,i+1,2,1);flags=rng.randrange(1<<24);prop=rng.choice([-1,0,1]);index=rng.choice([-1,*range(8)])
  faces.append(bounds+w(flags,prop&0xffffffff,index&0xffffffff));at=FACES+i*96;o.mem_write(at,bytes(96));o.mem_write(at+0x10,bounds);o.mem_write(at+0x28,w(flags));o.mem_write(at+0x34,struct.pack('<hh',prop,index))
  nbounds=f(100+i,1 if case%4 in (0,1) else rng.randrange(2),0,101+i,2,1);left,right=children.get(i,(0xffffffff,0xffffffff));nodes.append(nbounds+w(i,1,left,right));at=NODES+i*48
  o.mem_write(at,bytes(48));o.mem_write(at,nbounds);o.mem_write(at+0x18,w(FACES+i*96));o.mem_write(at+0x20,w(0 if left==0xffffffff else NODES+left*48,0 if right==0xffffffff else NODES+right*48))
 data=w(mode,update)+b''.join(nodes)+b''.join(faces)+dirty+w(0,0 if case%2 else 7);commands.append(data);o.mem_write(0x879af8,bytes([mode&255]));trace.clear();call(o,0x4d86d0,[OWNER,ROOM,update])
 outfaces=b''.join(face[:24]+bytes(o.mem_read(FACES+i*96+0x28,4))+face[28:] for i,face in enumerate(faces));outdirty=bytes(o.mem_read(GROUP+i*16+8,1)[0] for i in range(8))
 expected.append(w(0)+outfaces+outdirty+w(len(trace),*trace,*([0]*(65-len(trace)))))
 x.mem_write(B,data);x.mem_write(ROOM,w(0,0 if case%2 else 7));xtrace.clear()
 assert call(x,entry,[B+8,15,ROOM,2,B+608,15,B+1148,8,mode,update,ROOM+32,15,CALLBACK,0])==0
 assert bytes(x.mem_read(B+608,548))==outfaces+outdirty and xtrace==trace,(case,trace,xtrace)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--light-dirty-tree'],input=b''.join(commands))==b''.join(expected)
# Reject undersized initial scratch before callbacks or face writes.
x.mem_write(B,data);x.mem_write(ROOM,w(0,7));xtrace.clear();saved=bytes(x.mem_read(B+608,548))
assert call(x,entry,[B+8,15,ROOM,2,B+608,15,B+1148,8,mode,update,ROOM+32,1,CALLBACK,0])==0xfffffffc
assert not xtrace and bytes(x.mem_read(B+608,548))==saved
# Reject an accepted node's invalid child without writing its faces.
x.mem_write(B+8+7*40+4,f(1));x.mem_write(B+8+7*40+32,w(15));xtrace.clear()
assert call(x,entry,[B+8,15,ROOM,2,B+608,15,B+1148,8,mode,update,ROOM+32,15,CALLBACK,0])==0xfffffffc
assert xtrace==[107] and bytes(x.mem_read(B+608,548))==saved
report=dict(result='PASS',original_pc_nxdk_cases=1024,nxdk_guards=2,scope='Full original4d86d0 room-root path with two accepted roots and15 geometry nodes, actual child stack and face linked lists. Bounds predicate supplied; exact node/face call order, pruning, face flags and shared dirty bytes match PC/NXDK. Root bounds accepted, detail-room selection and native ownership excluded.')
(root/'artifacts/light-dirty-tree.json').write_text(json.dumps(report,indent=2));print(report)
