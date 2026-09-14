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



rng=random.Random(0x4d8130);inputs=[];responses=[];live_inputs=[];live_responses=[];P=0xc4e7d8;hits=[0,0,0]
for i in range(4096):
 kind=2+i%3;position=[rng.uniform(-10,10) for _ in range(3)];end=[rng.uniform(-10,10) for _ in range(3)];axis=[rng.uniform(-1,1) for _ in range(3)]
 radius=rng.uniform(.01,10);angle=rng.uniform(.1,6.1);lo=[rng.uniform(-10,0) for _ in range(3)];hi=[v+rng.uniform(0,15) for v in lo]
 data=w(kind,0,0,0)+f(*position,*end,*axis,1,1,1,radius,1,.05,angle,1)+f(*lo,*hi);inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*156)
 assert call('rf_visibility_light_volume',[B,OUT])==0;assert call('rf_visibility_light_bounds',[OUT,B+84,B+96,OUT+152])==0
 got=bytes(x.mem_read(OUT,156));responses.append(w(0)+got)
 o.mem_write(B,data);o.mem_write(P,bytes(268));o.mem_write(0xc96768,w(0xc96768,0xc96768));o.mem_write(0xc4e6b8,w(0xc4e6b8,0xc4e6b8));o.mem_write(0xc96880,w(0));o.mem_write(0x879af8,b'\0');o.mem_write(0xc96874,w(0));o.mem_write(0xc96878,w(0));o.mem_write(0x1818b84,w(0))
 color=[read(o,B+a) for a in (68,52,56,60)];tail=[0,0,0]
 if kind==2:address=0x4d8ed0;args=[B+16,read(o,B+64),*color,*tail]
 elif kind==4:address=0x4d9050;args=[B+16,B+28,read(o,B+64),*color,*tail]
 else:address=0x4d8f80;args=[B+16,B+40,read(o,B+72),read(o,B+76),read(o,B+64),*color,0,0,read(o,B+80),0,0]
 o.mem_write(STACK,w(STOP,*args));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(address,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 planes=bytes(120)
 if kind==3:
  o.mem_write(STACK,w(STOP,P,0,0));o.reg_write(UC_X86_REG_ESP,STACK);o.emu_start(0x4d86d0,0x4d89f8,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x4d89f8
  planes=b''.join(bytes(o.mem_read(P+0x94+j*16,16))+bytes(o.mem_read(P+0xf4+j*4,4)) for j in range(6))
 volume=w(kind)+bytes(o.mem_read(P+12,12))+(bytes(o.mem_read(P+24,12)) if kind==4 else bytes(12))+bytes(o.mem_read(P+0x3c,4))+planes
 o.mem_write(STACK,w(STOP,P,B+84,B+96));o.reg_write(UC_X86_REG_ESP,STACK);o.emu_start({2:0x4d8130,3:0x4d81d0,4:0x4d8250}[kind],STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 hit=o.reg_read(UC_X86_REG_EAX)&255;hits[kind-2]+=hit
 assert got==volume+w(hit),(i,kind,got.hex(),(volume+w(hit)).hex())
 # Move the original live light after construction; keep a segment's endpoint
 # fixed as4d91d0 does, and preserve its already-biased radius.
 if i<384:
  moved=f(*(v+13.25 for v in position));o.mem_write(B+200,moved)
  o.mem_write(STACK,w(STOP,0,B+200));o.reg_write(UC_X86_REG_ESP,STACK)
  o.emu_start(0x4d91d0,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP
  assert call('rf_vfx_light_create',[B,FACE])==0;x.mem_write(FACE+8,moved)
  source=bytes(x.mem_read(FACE,76));live_inputs.append(source+data[76:80]+data[84:108])
  x.mem_write(OUT,b'\xa5'*156)
  assert call('rf_visibility_light_source_volume',[FACE,read(x,B+76),OUT])==0
  assert call('rf_visibility_light_bounds',[OUT,B+84,B+96,OUT+152])==0
  moved_got=bytes(x.mem_read(OUT,156));live_responses.append(w(0)+moved_got)
  planes=bytes(120)
  if kind==3:
   o.mem_write(STACK,w(STOP,P,0,0));o.reg_write(UC_X86_REG_ESP,STACK)
   o.emu_start(0x4d86d0,0x4d89f8,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x4d89f8
   planes=b''.join(bytes(o.mem_read(P+0x94+j*16,16))+bytes(o.mem_read(P+0xf4+j*4,4)) for j in range(6))
  volume=w(kind)+bytes(o.mem_read(P+12,12))+(bytes(o.mem_read(P+24,12)) if kind==4 else bytes(12))+bytes(o.mem_read(P+0x3c,4))+planes
  o.mem_write(STACK,w(STOP,P,B+84,B+96));o.reg_write(UC_X86_REG_ESP,STACK)
  o.emu_start({2:0x4d8130,3:0x4d81d0,4:0x4d8250}[kind],STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP
  assert moved_got==volume+w(o.reg_read(UC_X86_REG_EAX)&255),(i,kind,'moved source')

for offset,value in [(0,w(1)),(64,f(-1)),(16,f(float('nan')))]:
 bad=bytearray(data);bad[offset:offset+4]=value;inputs.append(bytes(bad));x.mem_write(B,bytes(bad));x.mem_write(OUT,b'\xa5'*156)
 status=call('rf_visibility_light_volume',[B,OUT]);assert status and bytes(x.mem_read(OUT,156))==b'\xa5'*156;responses.append(w(status)+b'\xa5'*156)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--light-volume'],input=b''.join(inputs))==b''.join(responses)
moved_cases=len(live_inputs)
for sample,offset,value in [(0,0,w(1)),(0,56,f(-1)),(0,8,f(float('nan'))),(2,20,f(float('nan'))),(1,32,f(float('nan')))]:
 bad=bytearray(live_inputs[sample]);bad[offset:offset+4]=value;x.mem_write(B,bytes(bad));x.mem_write(OUT,b'\xa5'*156)
 status=call('rf_visibility_light_source_volume',[B,read(x,B+76),OUT]);assert status and bytes(x.mem_read(OUT,156))==b'\xa5'*156
 live_inputs.append(bytes(bad));live_responses.append(w(status)+b'\xa5'*156)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--light-source-volume'],input=b''.join(live_inputs))==b''.join(live_responses)
report=dict(result='PASS',original_pc_nxdk_cases=4096,original_pc_nxdk_moved_source_cases=moved_cases,live_source_guards=5,guards=3,hits=hits,scope='Actual source constructors, spotlight width/planes and point/cone/segment box predicates without hooks; all152 prepared volume bytes and bounds decisions match PC/NXDK. World-space supplied definitions, room traversal/native owner composition excluded.')
(root/'artifacts/light-volume.json').write_text(json.dumps(report,indent=2));print(report)
