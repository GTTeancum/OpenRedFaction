"""Execute the compiled NXDK campaign model handoff and teardown (no XEMU claim)."""
import hashlib,json,re,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im)
b=0x30000000;u.mem_map(b,0x10000)
source=b;owner=b+0x1000;owned=b+0x2000;cache=b+0x4000;clips=b+0x6000;matrices=b+0x8000;stamps=b+0x9000;model=b+0xa000;position=b+0xa100;basis=b+0xa200;stack=b+0xe000;stop=b+0xf000
mapping=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def put(a,*v):u.mem_write(a,w(*v))
retire=sym('rf_scene_model_retire');take=sym('rf_scene_model_detach');close=sym('campaign_models_close');initialize=sym('rf_motion_playback_initialize')
calloc=sym('calloc');malloc=sym('malloc');free=sym('free');fail=0;live=set();calls=[]
owners=sym('campaign_model_owners');count=sym('campaign_model_owner_count');head=sym('campaign_model_head');owned_count=sym('campaign_model_owned_count');owned_bytes=sym('campaign_model_owned_bytes');resources=sym('campaign_playback_resources');diagnostic=sym('rf_scene_npc_models')
def hook(cpu,address,size,data):
 if address not in (calloc,malloc,free):return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=read(sp+4)
 if address==calloc:
  assert arg*read(sp+8)==308 and owned not in live
  result=0 if fail==1 else owned
  if result:u.mem_write(owned,bytes(308));live.add(owned)
 elif address==malloc:
  assert 50<=arg<=2500 and cache not in live
  result=0 if fail==2 else cache
  if result:u.mem_write(cache,bytes([0xa5])*arg);live.add(cache)
 else:
  assert arg in live,(hex(arg),live);live.remove(arg);result=0
  u.mem_write(arg,bytes([0xdd])*(80 if arg==owner else 308 if arg==owned else 2500))
 calls.append((address,arg));cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp))
u.hook_add(UC_HOOK_CODE,hook)
def call(address,*args):
 put(stack,stop,*args);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(address,stop,count=1000000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 return u.reg_read(UC_X86_REG_EAX)
cases=0
for bones in range(1,51):
 for active in (0,8,16):
  assert not live;live.add(owner);u.mem_write(source,bytes(300));u.mem_write(owner,bytes(80));put(source,0,bones);call(initialize,source+8)
  put(source+8,active);put(source+292,matrices,stamps);u.mem_write(clips,bytes(16*36))
  for i in range(16):put(source+12+i*12,i);put(clips+i*36+32,2)
  data=f(*[i*.25 for i in range(bones*12)]);u.mem_write(matrices,data);u.mem_write(stamps,bytes(bones*2))
  put(model,clips,0,16);put(resources,model,0,0,1,16,0,0,0)
  put(owner,1,source+8,owner,owner,source);put(owner+68,7,9,0)
  put(owners,owner);put(count,1);put(head,owner);put(owned_count,0);put(owned_bytes,0);put(diagnostic,1,80,0,0)
  u.mem_write(position,f(10,20,30));u.mem_write(basis,f(1,0,0,0,1,0,0,0,1))
  before=bytes(u.mem_read(owner,80));old=bytes(u.mem_read(source,300));calls.clear()
  put(owned_count,30);assert call(take,0,position,basis,11)==0xfffffffc and not calls;put(owned_count,0)
  for fail in (1,2):
   assert call(take,0,position,basis,11)==0xfffffffc
   assert live=={owner} and bytes(u.mem_read(owner,80))==before and bytes(u.mem_read(source,300))==old
   assert read(owned_count)==read(owned_bytes)==0
  fail=0;assert call(take,0,position,basis,11)==0
  assert read(owner+16)==owned and read(owner+4)==owned+8 and read(owner+76)==owned
  assert bytes(u.mem_read(owner+8,8))==w(owner,owner) and read(head)==owner
  assert bytes(u.mem_read(owner+20,48))==f(10,20,30,1,0,0,0,1,0,0,0,1) and bytes(u.mem_read(owner+68,8))==w(7,11)
  assert read(owned_count)==1 and read(owned_bytes)==308+bones*50
  assert read(source)==0xffffffff and read(source+8)==0
  assert call(take,0,position,basis,11)==0xfffffffc
  u.mem_write(matrices,bytes([0xdd])*bones*48);assert bytes(u.mem_read(cache,bones*48))==data
  assert all(read(clips+i*36+32)==2 for i in range(16))
  saved=bytes(u.mem_read(owner,80));put(owned_bytes,1)
  assert call(retire,0)==0xfffffffc and bytes(u.mem_read(owner,80))==saved
  put(owned_bytes,308+bones*50)
  if active:
   put(clips+32,0xffffffff);assert call(retire,0)==0xfffffffc and bytes(u.mem_read(owner,80))==saved
   put(clips+32,2)
  assert call(retire,0)==0 and live=={owner} and read(owner)==read(owner+16)==read(owner+76)==0
  assert read(owners)==owner and read(count)==1 and read(owned_count)==read(owned_bytes)==0
  calls.clear();assert call(retire,0)==0 and not calls
  call(close)
  assert not live and read(owners)==read(count)==read(head)==read(owned_count)==read(owned_bytes)==0
  assert bytes(u.mem_read(diagnostic,16))==w(1,80,1,0)
  assert all(read(clips+i*36+32)==(1 if i<active else 2) for i in range(16))
  cases+=1
report=dict(result='PASS',cases=cases,allocation_failures=cases*2,capacity_rejections=cases,repeat_rejections=cases,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Compiled campaign detach, individual retirement and level teardown with controlled heap boundaries; registered pose/placement preserved, source consumed, exact reference retirement and both allocations freed. No live death dispatch or XEMU transferred-model claim.')
(root/'artifacts/campaign-model-retire.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
