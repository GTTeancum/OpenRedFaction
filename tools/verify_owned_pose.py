"""Verify moved character pose storage and animation references on PC/NXDK."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
print(subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--owned-pose'],text=True).strip())
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);b=0x30000000;u.mem_map(b,0x10000)
source=b;owned=b+0x2000;matrices=b+0x4000;stamps=b+0x5000;clips=b+0x6000;model=b+0x7000;resources=b+0x7100;heap=b+0x8000;stack=b+0xe000;stop=b+0xf000
mapping=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
take=sym('rf_entity_pose_take');close=sym('rf_entity_owned_pose_close');initialize=sym('rf_motion_playback_initialize');malloc=sym('malloc');free=sym('free');read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
failed=False;live=False;calls=[]
def hook(cpu,address,size,data):
 global live
 if address not in (malloc,free):return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=read(sp+4);calls.append((address,arg))
 if address==malloc:
  assert not live and 50<=arg<=4700;live=not failed
  if live:cpu.mem_write(heap,bytes([0xa5])*arg)
  cpu.reg_write(UC_X86_REG_EAX,heap if live else 0)
 else:assert live and arg==heap;live=False;cpu.mem_write(heap,bytes([0xdd])*4700)
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp))
u.hook_add(UC_HOOK_CODE,hook)
def call(address,*args):
 u.mem_write(stack,w(stop,*args));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(address,stop,count=1000000)
 assert u.reg_read(UC_X86_REG_EIP)==stop;return u.reg_read(UC_X86_REG_EAX)
registered='--registered' in sys.argv
with_overrides='--overrides' in sys.argv
overrides=b+0xa000;stride=94 if with_overrides else 50
node=b+0x7200;published=b+0x7220;head=b+0x7240
if registered:
 registered_take=sym('rf_entity_registered_pose_take');register=sym('rf_model_skeletal_register');retire=sym('rf_model_skeletal_retire')
def take_pose(budget):
 return call(registered_take,node,published,resources,budget,owned) if registered else call(take,source,resources,budget,owned)
cases=0
for bones in range(1,51):
 for count in (0,8,16):
  assert not live;u.mem_write(source,bytes(304));u.mem_write(owned,bytes(312));u.mem_write(source,w(0,bones));call(initialize,source+8)
  u.mem_write(source+8,w(count));u.mem_write(source+256,f(.25));u.mem_write(source+268,w(3));u.mem_write(source+292,w(matrices,stamps))
  if with_overrides:u.mem_write(source+300,w(overrides));u.mem_write(overrides,bytes([0x5a])*bones*44)
  u.mem_write(clips,bytes(16*36));u.mem_write(model,w(clips,0,16));u.mem_write(resources,w(model,0,0,1,16,0,0,0))
  for i in range(16):u.mem_write(source+12+i*12,w(i));u.mem_write(clips+i*36+32,w(2))
  data=f(*[j*.25 for j in range(bones*12)]);generations=struct.pack('<'+'H'*bones,*range(3,bones+3))
  u.mem_write(matrices,data);u.mem_write(stamps,generations);before=bytes(u.mem_read(source,304));budget=312+bones*stride;calls.clear()
  if registered:
   u.mem_write(node,w(1,source+8,0,0));u.mem_write(published,w(source));u.mem_write(head,w(0))
   assert call(registered_take,node,published,resources,budget,owned)==0xfffffffc and not calls
   assert call(register,node,head,1)==0
  assert take_pose(budget-1)==0xfffffffc and not calls and bytes(u.mem_read(source,304))==before and bytes(u.mem_read(owned,312))==bytes(312)
  failed=True;assert take_pose(budget)==0xfffffffc and calls==[(malloc,bones*stride)] and not live
  assert bytes(u.mem_read(source,304))==before and bytes(u.mem_read(stamps,bones*2))==generations
  if registered:assert bytes(u.mem_read(node,16))==w(1,source+8,node,node) and read(published)==source
  failed=False;calls.clear();assert take_pose(budget)==0 and calls==[(malloc,bones*stride)]
  if registered:assert bytes(u.mem_read(node,16))==w(1,owned+8,node,node) and read(published)==owned and read(head)==node
  assert bytes(u.mem_read(owned,292))==before[:292] and read(owned+292)==heap and read(owned+296)==heap+bones*(92 if with_overrides else 48) and read(owned+300)==(heap+bones*48 if with_overrides else 0) and read(owned+304)==heap and read(owned+308)==budget
  assert read(source)==0xffffffff and read(source+8)==0 and bytes(u.mem_read(source+292,8))==w(matrices,stamps) and bytes(u.mem_read(stamps,bones*2))==bytes(bones*2)
  assert all(read(clips+i*36+32)==2 for i in range(16))
  u.mem_write(matrices,bytes([0xdd])*bones*48);u.mem_write(stamps,bytes([0xdd])*bones*2)
  assert bytes(u.mem_read(heap,bones*stride))==data+(bytes([0x5a])*bones*44 if with_overrides else b'')+generations
  if with_overrides:
   assert bytes(u.mem_read(overrides,bones*44))==bytes(bones*44)
   u.mem_write(overrides,bytes([0xdd])*bones*44)
   assert bytes(u.mem_read(read(owned+300),bones*44))==bytes([0x5a])*bones*44
  saved=bytes(u.mem_read(owned,312));calls.clear()
  if count:
   u.mem_write(clips+32,w(0));assert call(close,owned,resources)==0xfffffffc and not calls and bytes(u.mem_read(owned,312))==saved
   u.mem_write(clips+32,w(2))
  if registered:
   assert call(retire,node,head,1,clips,16)==0 and read(head)==0 and bytes(u.mem_read(node+8,8))==bytes(8)
  assert call(close,owned,resources)==0 and calls==[(free,heap)] and not live and bytes(u.mem_read(owned,312))==bytes(312)
  assert all(read(clips+i*36+32)==(1 if i<count else 2) for i in range(16))
  calls.clear();assert call(close,owned,0)==0 and not calls;cases+=1
report=dict(result='PASS',registered=registered,cases=cases,heap_failure_cases=cases,short_budget_cases=cases,max_owned_bytes=312+50*stride,overrides=with_overrides,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Port storage handoff for existing rf_entity_pose: independent matrices/stamps, exact playback/controller copy, source consumed, shared motion reference counts unchanged until one close, other owners preserved. PC/NXDK machine code; no original model-allocator equivalence or live scene/XEMU binding. Immutable geometry/materials/catalog remain level-owned.')
if registered:report['scope']='Port registered pose handoff: registration links/head unchanged, published and active pointers redirected, source consumed, budget/allocation errors preserve owners; retirement drains moved references once before owned-cache close. PC/NXDK machine code, no live corpse or native XEMU claim.'
(root/('artifacts/'+('registered-pose' if registered else 'owned-pose')+('-overrides' if with_overrides else '')+'-verification.json')).write_text(json.dumps(report,indent=2)+'\n');print(report)
