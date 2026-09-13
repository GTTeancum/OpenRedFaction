"""VFX scale/quaternion/translation stages against original math helpers."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[i&0xffffffff for i in v])
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OWNER=B+0x1000;FACE=B+0x2000;UV=B+0x3000;PTR=B+0x4000;CTX=B+0x5000;OUT=B+0x6000;STACK=B+0xe000;STOP=B+0xff00
read=lambda u,a:struct.unpack('<I',u.mem_read(a,4))[0]
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(B,65536);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
o=machine(exe);x=machine(root/'build/xbox/main.exe')
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
def call(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)



rng=random.Random(0x4d86d0);inputs=[];responses=[];changed=0;P=0xc4e7d8
ROOM=B+0x3000;ROOMS=B+0x4000;NODES=B+0x5000;FACES=B+0x6000;GROUP=B+0x7000;TABLE=B+0x7100
for i in range(2048):
 kind=2+i%3;mode=(i//3)%2;update=(i//6)%2;path=(i//12)%2
 definition=w(kind,0,0,0)+f(*[rng.uniform(-10,10) for _ in range(6)],*[rng.uniform(-1,1) for _ in range(3)],1,1,1,rng.uniform(.1,10),1,.05,rng.uniform(.1,6.1),1)
 def bounds():
  lo=[rng.uniform(-20,0) for _ in range(3)];return f(*lo,*[v+rng.uniform(1,30) for v in lo])
 solid_bounds=bounds();rooms=[bounds()+w(0,0,1),bounds()+w(2,0,0)];nodes=[bounds()+w(j,1,1 if j==0 else 0xffffffff,0xffffffff) for j in range(3)]
 faces=[bounds()+w(rng.randrange(1<<24),rng.choice([-1,0,1])&0xffffffff,rng.choice([-1,0,1,2,3])&0xffffffff) for _ in range(3)];dirty=bytes(rng.randrange(4) for _ in range(4))
 data=definition+w(mode,update,path)+solid_bounds+b''.join(rooms)+b''.join(nodes)+b''.join(faces)+dirty;inputs.append(data)
 x.mem_write(B,data);assert call('rf_visibility_light_volume',[B,OUT])==0
 x.mem_write(B+0x7000,w(0,1));x.mem_write(PTR,solid_bounds+w(B+120,2,B+0x7000,path,B+0x7004,1,B+192,3,B+312,3,B+420,4,B+0x7010,4,B+0x7030,4))
 assert call('rf_visibility_light_solid',[OUT,PTR,mode,update])==0;got=bytes(x.mem_read(B+312,112));responses.append(w(0)+got)
 # Retained-world adapter: split the same forest into two room-local trees.
 x.mem_write(B,data);x.mem_write(B+272+24,w(0))
 world=B+0x8000;wr=B+0x8100;views=B+0x8200;owner=B+0x8300;scratch=B+0x8400
 for j in range(2):
  tree=w(0,B+192+(80 if j else 0),0,0,B+0x7030,1 if j else 2,1 if j else 2,4,0,0)
  x.mem_write(wr+j*80,tree+w(0)+rooms[j][:24]+w(j,0,0))
  x.mem_write(views+j*40,rooms[j][:24]+w(0,0,1-j,wr+j*80))
 x.mem_write(world,w(0,wr,views,B+0x7000,B+0x7004,2,path,1,0,0)+solid_bounds)
 x.mem_write(owner,w(B+312,B+420,3,4,0));x.mem_write(scratch,w(B+0x8500,B+0x8580,B+0x7010,2,4))
 assert call('rf_visibility_light_world',[OUT,world,owner,mode,update,scratch])==0
 assert bytes(x.mem_read(B+312,112))==got,('world',i)
 o.mem_write(B,data);o.mem_write(P,bytes(268));o.mem_write(0xc96768,w(0xc96768,0xc96768));o.mem_write(0xc4e6b8,w(0xc4e6b8,0xc4e6b8));o.mem_write(0xc96880,w(0));o.mem_write(0x879af8,b'\0');o.mem_write(0xc96874,w(0));o.mem_write(0xc96878,w(0));o.mem_write(0x1818b84,w(0))
 color=[read(o,B+a) for a in (68,52,56,60)];tail=[0,0,0]
 if kind==2:address=0x4d8ed0;args=[B+16,read(o,B+64),*color,*tail]
 elif kind==4:address=0x4d9050;args=[B+16,B+28,read(o,B+64),*color,*tail]
 else:address=0x4d8f80;args=[B+16,B+40,read(o,B+72),read(o,B+76),read(o,B+64),*color,0,0,read(o,B+80),0,0]
 o.mem_write(STACK,w(STOP,*args));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(address,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP

 o.mem_write(ROOM,bytes(512));o.mem_write(ROOM+0x48,solid_bounds);o.mem_write(ROOM+0x70,w(FACES));o.mem_write(ROOM+0x9c,w(path,path,TABLE));o.mem_write(TABLE,w(ROOMS));o.mem_write(TABLE+4,w(ROOMS+128))
 o.mem_write(ROOM+0xc0,w(4,4,TABLE+16));o.mem_write(TABLE+16,w(*(GROUP+j*16 for j in range(4))))
 for j in range(4):o.mem_write(GROUP+j*16,bytes(8)+dirty[j:j+1]+bytes(7))
 for j,r in enumerate(rooms):
  at=ROOMS+j*128;o.mem_write(at,bytes(128));o.mem_write(at+8,r[:24]);o.mem_write(at+0x3c,w(NODES+(0 if j==0 else 2)*48));o.mem_write(at+0x6c,w(1-j,1-j,TABLE+4))
 for j,n in enumerate(nodes):
  at=NODES+j*48;o.mem_write(at,bytes(48));o.mem_write(at,n[:24]);o.mem_write(at+0x18,w(FACES+j*96));o.mem_write(at+0x20,w(NODES+48 if j==0 else 0,0))
 for j,face in enumerate(faces):
  at=FACES+j*96;o.mem_write(at,bytes(96));o.mem_write(at+0x10,face[:24]);o.mem_write(at+0x28,face[24:28]);prop,index=struct.unpack('<ii',face[28:]);o.mem_write(at+0x34,struct.pack('<hh',prop,index));o.mem_write(at+0x54,w(at+96 if j<2 else 0))
 o.mem_write(0x879af8,bytes([mode]));o.mem_write(STACK,w(STOP,P,ROOM,update));o.reg_write(UC_X86_REG_ESP,STACK);o.emu_start(0x4d86d0,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 expected=b''.join(face[:24]+bytes(o.mem_read(FACES+j*96+0x28,4))+face[28:] for j,face in enumerate(faces))+bytes(o.mem_read(GROUP+j*16+8,1)[0] for j in range(4))
 assert got==expected,(i,kind,mode,update,path,got.hex(),expected.hex())
 changed+=got!=b''.join(faces)+dirty
# Fail before changing dirty metadata for invalid bindings or short scratch.
x.mem_write(OUT,w(2)+f(0,0,0,0,0,0,10000)+bytes(120))
for address,value in [(scratch+12,1),(scratch+16,0),(owner,0),(owner+4,0),(owner+8,2),(owner+8,4)]:
 saved=bytes(x.mem_read(address,4));before=bytes(x.mem_read(B+312,112));x.mem_write(address,w(value))
 x.mem_write(world+24,w(1))
 assert call('rf_visibility_light_world',[OUT,world,owner,1,1,scratch])!=0
 assert bytes(x.mem_read(B+312,112))==before
 x.mem_write(address,saved)
# Empty room trees contribute no faces; traversal keeps valid mapped storage.
x.mem_write(wr+80+20,w(0,0));x.mem_write(owner+8,w(2))
assert call('rf_visibility_light_world',[OUT,world,owner,1,1,scratch])==0
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--light-dirty-solid'],input=b''.join(inputs))==b''.join(responses)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--light-dirty-world'],input=b''.join(inputs))==b''.join(responses)
report=dict(result='PASS',world_adapter_pc_nxdk_cases=2048,nxdk_binding_guards=6,empty_tree_cases=1,original_pc_nxdk_cases=2048,changed_cases=changed,scope='Actual source constructors plus complete4d86d0 world-space updates without hooks. Point/cone/segment shapes, flat-solid and primary/detail trees, real geometry and face/shared state match PC/NXDK. Native ownership, alternate views and frame scheduling excluded.')
(root/'artifacts/light-dirty-solid.json').write_text(json.dumps(report,indent=2));print(report)
