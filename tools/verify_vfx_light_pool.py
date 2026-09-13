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
 x.emu_start(sym(name),STOP,count=10000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)



rng=random.Random(0x4d9130);inputs=[];responses=[];P=0xc4e7d8;cache_checks=0
assert call('rf_vfx_light_pool_init',[OWNER,FACE,PTR,32])==0
o.mem_write(P,bytes(1100*268));o.mem_write(0xc96768,w(0xc96768,0xc96768));o.mem_write(0xc4e6b8,w(0xc4e6b8,0xc4e6b8));o.mem_write(0xc96880,w(0));o.mem_write(0xc96874,w(0));o.mem_write(0xc96878,w(0))
def original(address,args):
 o.mem_write(STACK,w(STOP,*args));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(address,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP;return o.reg_read(UC_X86_REG_EAX)
for i in range(1024):
 live=[j for j in range(32) if read(x,FACE+j*80)];op=0 if not live or (len(live)<28 and rng.random()<.4) else rng.choice([1,2,2,3,4]);id=rng.choice(live) if live else 0;world=rng.randrange(2)
 definition=w(2+i%3,i%4,i%512,i%512)+f(*[rng.uniform(-10,10) for _ in range(9)],.5,.8,.3,5,1,.5,1.5,.25);position=f(*[rng.uniform(-15,15) for _ in range(3)]);data=w(op,id,world)+definition+position;inputs.append(data);x.mem_write(B,data);o.mem_write(B,data);x.mem_write(OWNER+12,w(1,7));o.mem_write(0xc96890,w(1));o.mem_write(0xc9687c,w(7));o.mem_write(0x879af8,bytes([world]));x.mem_write(OUT,w(id))
 if op==0:
  status=call('rf_vfx_light_pool_create',[OWNER,B+12,world,OUT]);id=read(x,OUT);kind=read(o,B+12);color=[read(o,B+12+a) for a in (68,52,56,60)];tail=[read(o,B+20),0,read(o,B+16)]
  if kind==2:address=0x4d8ed0;args=[B+28,read(o,B+76),*color,*tail]
  elif kind==4:address=0x4d9050;args=[B+28,B+40,read(o,B+76),*color,*tail]
  else:address=0x4d8f80;args=[B+28,B+52,read(o,B+84),read(o,B+88),read(o,B+76),*color,read(o,B+20),0,read(o,B+92),read(o,B+16),read(o,B+24)]
  assert original(address,args)==id
 elif op==1:status=call('rf_vfx_light_pool_retain',[OWNER,id]);original(0x4d91b0,[id])
 elif op==2:status=call('rf_vfx_light_pool_release',[OWNER,id]);original(0x4d9130,[id,0])
 elif op==3:status=call('rf_vfx_light_pool_move',[OWNER,id,B+96]);original(0x4d91d0,[id,B+96])
 else:status=call('rf_vfx_light_pool_enable',[OWNER,id,world]);original(0x4d92f0,[id,world])
 assert status==0
 responses.append(w(status,id)+bytes(x.mem_read(OWNER,36))+bytes(x.mem_read(FACE,32*80))+bytes(x.mem_read(PTR,32*16)))
 assert [read(x,OWNER+a) for a in (4,8,12,16)]==[read(o,a) for a in (0xc96878,0xc96874,0xc96890,0xc9687c)],i
 for j in range(32):
  p=P+j*268;assert read(x,FACE+j*80)==read(o,p+8),(i,j,'type')
  assert read(x,PTR+j*16)==read(o,p+0x58),(i,j,'references')
  if read(o,p+8):
   assert bytes(x.mem_read(FACE+j*80+8,12))==bytes(o.mem_read(p+12,12))
   assert bytes(x.mem_read(FACE+j*80+76,1))==bytes(o.mem_read(p+0x4c,1))
 for list_id,head in [(0,0xc96768),(1,0xc4e6b8)]:
  expected=[];p=read(o,head)
  while p!=head:expected.append((p-P)//268);p=read(o,p);assert len(expected)<=32
  actual=[];slot=read(x,OWNER+20+list_id*4)
  while slot!=0xffffffff:actual.append(slot);slot=read(x,PTR+slot*16+8);assert len(actual)<=32
  assert actual==expected,(i,list_id,actual,expected)
  assert read(x,OWNER+28+list_id*4)==(expected[-1] if expected else 0xffffffff)
 # Rebuild room caches from each linked list and compare original slot IDs.
 for gate in (0,1):
  x.mem_write(CTX,w(0,0,32,UV,0));x.mem_write(B+200,f(-4,-4,-4,4,4,4));assert call('rf_vfx_light_pool_cache',[OWNER,gate,B+200,B+212,CTX])==0
  o.mem_write(CTX+8,f(-4,-4,-4,4,4,4));o.mem_write(CTX+0x1bc,w(0,32,UV,(read(o,0xc96874)-1)&0xffffffff));o.mem_write(0x879af8,bytes([gate]));original(0x4d96c0,[CTX])
  n=read(x,CTX+4);assert n==read(o,CTX+0x1bc)
  assert [read(x,UV+j*4) for j in range(n)]==[(read(o,UV+j*4)-P)//268 for j in range(n)]
  cache_checks+=1
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-light-pool'],input=b''.join(inputs));assert pc==b''.join(responses)
# Invalid handle / overflow refs preserve owner and arrays.
snapshot=bytes(x.mem_read(OWNER,44))+bytes(x.mem_read(FACE,2560))+bytes(x.mem_read(PTR,512))
for name,args in [('rf_vfx_light_pool_release',[OWNER,32]),('rf_vfx_light_pool_create',[OWNER,B+12,2,OUT])]:
 assert call(name,args)!=0;assert bytes(x.mem_read(OWNER,44))+bytes(x.mem_read(FACE,2560))+bytes(x.mem_read(PTR,512))==snapshot
# Full stock-capacity pool, then exhaustion and first-free reuse.
A=0x31000000;x.mem_map(A,0x20000);AS=A+0x1000;AL=AS+1100*80
assert call('rf_vfx_light_pool_init',[A,AS,AL,1100])==0
x.mem_write(B,inputs[0][12:96])
for j in range(1100):
 assert call('rf_vfx_light_pool_create',[A,B,1,OUT])==0 and read(x,OUT)==j
saved=bytes(x.mem_read(A,44));x.mem_write(OUT,w(12345));assert call('rf_vfx_light_pool_create',[A,B,1,OUT])!=0 and read(x,OUT)==12345 and bytes(x.mem_read(A,44))==saved
assert call('rf_vfx_light_pool_release',[A,17])==0
assert call('rf_vfx_light_pool_create',[A,B,1,OUT])==0 and read(x,OUT)==17 and read(x,A+4)==1100
report=dict(result='PASS',original_pc_nxdk_operation_sequence=1024,original_nxdk_pool_cache_checks=cache_checks,nxdk_guards=2,nxdk_full_capacity_insertions=1100,pool_storage_bytes_for_1100=44+1100*96,scope='Actual original constructors, retain/release, move/enable, linked list order and generation/active-state transitions; no hooks. PC/NXDK full owner serialization; original checks represented live fields, references and list order. Room cache rebuilt through each real list. Caller-owned storage, scene visibility owner empty; native source loading/visibility updates remain.')
(root/'artifacts/vfx-light-pool.json').write_text(json.dumps(report,indent=2));print(report)
