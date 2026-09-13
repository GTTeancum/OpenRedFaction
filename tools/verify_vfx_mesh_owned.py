"""Owned VFX mesh PC/NXDK composition, budgets and rollback."""
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
o=machine(exe);x=machine(root/'build/xbox/main.exe');stream=dict(data=b'',at=0)
lookup_names=[]
def file_service(u,a,size,data):
 sp=u.reg_read(UC_X86_REG_ESP);pop=0;result=0
 if a==0x52cf60:
  target=read(u,sp+4);amount=read(u,sp+8);assert read(u,sp+12)==read(u,sp+16)==0
  chunk=stream['data'][stream['at']:stream['at']+amount];assert len(chunk)==amount,(hex(a),stream['at'],amount,len(stream['data']))
  u.mem_write(target,chunk);stream['at']+=amount;pop=16
 elif a==0x50f6a0:
  pointer=read(u,sp+4);name=bytes(u.mem_read(pointer,33)).split(b'\0')[0];lookup_names.append(name);result=0xffffffff
 else:assert a==0x524530
 u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_EIP,read(u,sp));u.reg_write(UC_X86_REG_ESP,sp+4+pop)
for a in (0x52cf60,0x524530,0x50f6a0):o.hook_add(UC_HOOK_CODE,file_service,begin=a,end=a)
PARAM=B+0x7000;COUNTS=B+0x8000;BLEND=B+0x9000;COLOR=B+0xa000;ALPHA=B+0xb000;SAMPLE=B+0xc000
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
def call(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)

HEAP=0x31000000;x.mem_map(HEAP,0x400000);live={};calls=[];fail_alloc=False

def allocator(u,a,size,data):
 sp=u.reg_read(UC_X86_REG_ESP);arg=read(u,sp+4);ret=0
 if a==sym('malloc'):
  calls.append(('malloc',arg))
  if not fail_alloc:assert not live and arg<=0x400000;live[HEAP]=arg;ret=HEAP;u.mem_write(HEAP,b'\xcc'*arg)
 else:
  calls.append(('free',arg))
  if arg:assert arg in live;del live[arg]
 u.reg_write(UC_X86_REG_EAX,ret);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(u,sp))
for name in ('malloc','free'):x.hook_add(UC_HOOK_CODE,allocator,begin=sym(name),end=sym(name))
vertex_checks=0;uv_checks=0;morph_checks=0;vector_key_checks=0;rotation_key_checks=0;transform_checks=0;keyed_checks=0
def shared(v,global_count,budget,data,fail=False):
 global fail_alloc,vertex_checks,uv_checks,morph_checks,vector_key_checks,rotation_key_checks,transform_checks,keyed_checks
 fail_alloc=fail;calls.clear();assert not live;x.mem_write(B,data or b'\0');x.mem_write(PTR,f(0,0,0,0,0,0,1));x.mem_write(OUT,w(0))
 status=call('rf_vfx_mesh_open',[B,len(data),v,global_count,PTR,budget,OUT]);result=w(status);pointer=read(x,OUT)
 if not status:
  assert pointer==HEAP and live;header=bytes(x.mem_read(pointer,300));n=struct.unpack_from('<I',header,148)[0];size,allocated=struct.unpack_from('<II',header,292);frames,owned=struct.unpack('<II',x.mem_read(pointer+300,8))
  assert allocated==308+n*112+size==live[HEAP] and allocated<=budget and size==len(data)
  x.mem_write(B,b'\xa5'*len(data));result+=header+bytes(x.mem_read(frames,n*112))+bytes(x.mem_read(owned,size));assert result[-size:]==data
  vertices=struct.unpack_from('<I',header,132)[0];flags=struct.unpack_from('<I',header,184)[0]
  for frame in range(n):
   source=frames+(frame if flags&4 else 0)*112;offset=read(x,source+32);vectors=bytes(x.mem_read(source,24))
   for vertex in range(vertices):
    assert call('rf_vfx_mesh_vertex',[pointer,frame,vertex,SAMPLE])==0;actual=bytes(x.mem_read(SAMPLE,12));raw=bytes(x.mem_read(owned+offset+vertex*6,6))
    o.mem_write(B,raw);o.mem_write(STACK,w(STOP,OUT,B)+vectors);o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x53cca0,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP
    assert actual==bytes(o.mem_read(OUT,12));vertex_checks+=1
  x.mem_write(SAMPLE,b'\xa5'*12);assert call('rf_vfx_mesh_vertex',[pointer,n,0,SAMPLE])!=0 and bytes(x.mem_read(SAMPLE,12))==b'\xa5'*12
  for frame in range(n):
   last=min(frame+1,n-1);x.mem_write(B+0x100,f(frame,.25)+w(frame,last,1))
   for vertex in range(vertices):
    x.mem_write(SAMPLE,b'\xa5'*32);status=call('rf_vfx_mesh_morph',[pointer,B+0x100,vertex,SAMPLE])
    if not flags&4:assert status==0xfffffffd and bytes(x.mem_read(SAMPLE,32))==b'\xa5'*32;continue
    assert status==0;actual=bytes(x.mem_read(SAMPLE,32));a=frames+frame*112;b=frames+last*112;av=owned+read(x,a+32)+vertex*6;bv=owned+read(x,b+32)+vertex*6
    assert call('rf_vfx_morph_read',[a,av,b,bv,0x3e800000,frame!=last,SAMPLE+64])==0;assert actual==bytes(x.mem_read(SAMPLE+64,32));morph_checks+=1
  for frame in range(n):
   last=min(frame+1,n-1);x.mem_write(B+0x100,f(frame,.25)+w(frame,last,1))
   for vertex in range(vertices):
    x.mem_write(SAMPLE,b'\xa5'*32);status=call('rf_vfx_mesh_transform',[pointer,B+0x100,vertex,SAMPLE])
    if flags&4 or struct.unpack_from('<I',header,204)[0]&2:assert status==0xfffffffd and bytes(x.mem_read(SAMPLE,32))==b'\xa5'*32;continue
    assert status==0;actual=bytes(x.mem_read(SAMPLE,32));av=owned+read(x,frames+32)+vertex*6
    assert call('rf_vfx_transform_sample',[frames,av,frames+frame*112+48,frames+last*112+48,0x3e800000,frame!=last,flags,SAMPLE+64])==0
    assert actual==bytes(x.mem_read(SAMPLE+64,32));transform_checks+=1
  for track in (0,2):
   for time in (-1,0,10,1000):
    x.mem_write(SAMPLE,b'\xa5'*12);status=call('rf_vfx_mesh_vector_key',[pointer,track,time&0xffffffff,SAMPLE])
    if not struct.unpack_from('<I',header,204)[0]&2:assert status==0xfffffffd and bytes(x.mem_read(SAMPLE,12))==b'\xa5'*12;continue
    assert status==0;actual=bytes(x.mem_read(SAMPLE,12));count=struct.unpack_from('<I',header,252+track*4)[0];offset=struct.unpack_from('<I',header,264+track*4)[0]
    o.mem_write(B,bytes(x.mem_read(owned+offset,count*40)) or b'\0');o.mem_write(OWNER,bytes(20));o.mem_write(OWNER+(0 if track==0 else 4),struct.pack('<H',count));o.mem_write(OWNER+(8 if track==0 else 16),w(B));o.mem_write(STACK,w(STOP,OUT,time));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,OWNER);o.reg_write(UC_X86_REG_FPCW,0x37f)
    o.emu_start(0x569f70 if track==0 else 0x56a3f0,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP;assert actual==bytes(o.mem_read(OUT,12));vector_key_checks+=1
  for time in (-1,0,10,1000):
   x.mem_write(SAMPLE,b'\xa5'*16);status=call('rf_vfx_mesh_rotation_key',[pointer,time&0xffffffff,SAMPLE])
   if not struct.unpack_from('<I',header,204)[0]&2:assert status==0xfffffffd and bytes(x.mem_read(SAMPLE,16))==b'\xa5'*16;continue
   assert status==0;actual=bytes(x.mem_read(SAMPLE,16));count=struct.unpack_from('<I',header,256)[0];offset=struct.unpack_from('<I',header,268)[0]
   assert call('rf_vfx_rotation_key_sample',[owned+offset,size-offset,count,time&0xffffffff,SAMPLE+64])==0;assert actual==bytes(x.mem_read(SAMPLE+64,16));rotation_key_checks+=1
  for time in (-1,0,10,1000):
   for vertex in range(vertices):
    x.mem_write(SAMPLE,b'\xa5'*32);status=call('rf_vfx_mesh_keyed',[pointer,time&0xffffffff,vertex,SAMPLE])
    if flags&4 or not struct.unpack_from('<I',header,204)[0]&2:assert status==0xfffffffd and bytes(x.mem_read(SAMPLE,32))==b'\xa5'*32;continue
    assert status==0;actual=bytes(x.mem_read(SAMPLE,32));key=B+0x300
    for track,target in ((0,key),(2,key+28)):
     count=struct.unpack_from('<I',header,252+track*4)[0]
     if count:assert call('rf_vfx_mesh_vector_key',[pointer,track,time&0xffffffff,target])==0
     else:x.mem_write(target,bytes(x.mem_read(frames+48+(0 if track==0 else 28),12)))
    assert call('rf_vfx_mesh_rotation_key',[pointer,time&0xffffffff,key+12])==0
    av=owned+read(x,frames+32)+vertex*6
    assert call('rf_vfx_keyed_sample',[frames,av,pointer+212,key,flags,SAMPLE+64])==0
    assert actual==bytes(x.mem_read(SAMPLE+64,32));keyed_checks+=1
  faces=struct.unpack_from('<I',header,136)[0]
  for frame in range(n):
   last=min(frame+1,n-1);x.mem_write(B+0x100,f(frame,.25)+w(frame,last,1));a=frame if flags&0x100 else 0;b=last if flags&0x100 else 0
   for face in range(faces):
    assert call('rf_vfx_mesh_uv',[pointer,B+0x100,face,SAMPLE])==0;actual=bytes(x.mem_read(SAMPLE,24));uv=[]
    for selected in (a,b):
     offset=read(x,frames+selected*112+40);words=struct.unpack('<6I',x.mem_read(owned+offset+face*24,24));uv.append(w(words[0],words[2],words[4],words[1],words[3],words[5]))
    o.mem_write(B,uv[0]+uv[1]);o.mem_write(OWNER,bytes(0x124));o.mem_write(OWNER+0x114,w(0x100 if a!=b else 0));o.mem_write(OWNER+0xa4,w(2));o.mem_write(OWNER+0xb4,w(1));o.mem_write(OWNER+0xd4,w(PTR));o.mem_write(PTR,w(B,B+24));o.mem_write(FACE,bytes(0x98));o.mem_write(FACE+0x84,w(OUT));o.mem_write(STACK,bytes(0x100));o.mem_write(STACK+0x28,f(.25));o.mem_write(STACK+0x6c,w(1));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,OWNER);o.reg_write(UC_X86_REG_ESI,FACE);o.reg_write(UC_X86_REG_EDX,0x80000000);o.reg_write(UC_X86_REG_FPCW,0x37f)
    o.emu_start(0x54010a,0x5402fa,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x5402fa;assert actual==bytes(o.mem_read(OUT,24));uv_checks+=1
  x.mem_write(B+0x100,bytes(20));x.mem_write(SAMPLE,b'\xa5'*24);assert call('rf_vfx_mesh_uv',[pointer,B+0x100,0,SAMPLE])==0xfffffffd and bytes(x.mem_read(SAMPLE,24))==b'\xa5'*24


 else:assert pointer==0 and not live
 call('rf_vfx_mesh_close',[OUT]);call('rf_vfx_mesh_close',[OUT]);assert read(x,OUT)==0 and not live
 return result
inputs=[];names=[];probe=str(root/'build/pc/Release/rf_entity_assets_probe.exe')
inv=json.loads((root/'artifacts/inventory.json').read_text());archive=next(a for a in inv['files'] if a['path']=='meshes.vpp')
for asset in json.loads((root/'artifacts/vfx-header.json').read_text())['assets']:
 v=int(asset['version'],16);e=next(e for e in archive['vpp']['entries'] if e['name']==asset['name'])
 with (root/'Installed_Game/meshes.vpp').open('rb') as file:file.seek(e['offset']);data=file.read(e['size'])
 at=asset['header_bytes']
 while at<len(data):
  tag,length=struct.unpack_from('<II',data,at);end=at+4+length
  if tag==0x4f584653:inputs.append((v,100,1048576,data[at+8:end]));names.append(asset['name'])
  at=end
responses=[];footprints=[];invalid=[]
for case,(v,g,b,d) in enumerate(inputs):
 got=shared(v,g,b,d);assert got[:4]==w(0),(case,names[case],got.hex());responses.append(got)
 size=struct.unpack_from('<I',got,300)[0];footprints.append(size)
 assert shared(v,g,size,d)==got
 invalid.append((v,g,size-1,d));assert shared(v,g,b,d,True)==w(-1)
 for n in [0,1,len(d)//2,len(d)-1]:invalid.append((v,g,b,d[:n]))
 invalid.append((v,g,b,d+b'\0'))
 if v>=0x40000:invalid.append((v,0,b,d))
 face_at=struct.unpack_from('<I',got,144)[0];stride=120 if v<0x3000d else 96
 for relative in (stride-20,stride-12):
  changed=bytearray(d);struct.pack_into('<I',changed,face_at+relative,0xffffffff);invalid.append((v,g,b,bytes(changed)))
 edge_at=struct.unpack_from('<I',got,204)[0]
 if struct.unpack_from('<I',d,edge_at+16)[0]:
  changed=bytearray(d);struct.pack_into('<I',changed,edge_at+20,0xffffffff);invalid.append((v,g,b,bytes(changed)))

for v,g,b,d in invalid:
 got=shared(v,g,b,d);assert got[:4]!=w(0);responses.append(got)
pc=subprocess.check_output([probe,'--vfx-mesh-owned'],input=b''.join(w(v,g,b,len(d))+f(0,0,0,0,0,0,1)+d for v,g,b,d in inputs+invalid));assert pc==b''.join(responses)
report=dict(result='PASS',authored_pc_nxdk_meshes=len(inputs),failure_cases=len(invalid),allocation_failures=len(inputs),exact_budget_cases=len(inputs),owned_bytes=footprints,authored_vertex_checks=vertex_checks,authored_uv_checks=uv_checks,authored_morph_checks=morph_checks,authored_vector_key_checks=vector_key_checks,authored_rotation_key_checks=rotation_key_checks,authored_transform_checks=transform_checks,authored_keyed_checks=keyed_checks,scope='Composed previously original-verified decoders; all14 authored payloads, owned source independence, PC/NXDK header/frame/payload equality, exact/short budgets, malloc failure and rollback, repeat close. Original vertex expansion and UV branch comparisons included; morph and keyed geometry accessor composition included; no bitmap binding/full playback/native XEMU.')
(root/'artifacts/vfx-mesh-owned.json').write_text(json.dumps(report,indent=2));print(report)
