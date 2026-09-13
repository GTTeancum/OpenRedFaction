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
vertex_checks=0
def shared(v,global_count,budget,data,fail=False):
 global fail_alloc,vertex_checks
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
report=dict(result='PASS',authored_pc_nxdk_meshes=len(inputs),failure_cases=len(invalid),allocation_failures=len(inputs),exact_budget_cases=len(inputs),owned_bytes=footprints,authored_vertex_checks=vertex_checks,scope='Composed previously original-verified decoders; all14 authored payloads, owned source independence, PC/NXDK header/frame/payload equality, exact/short budgets, malloc failure and rollback, repeat close. No bitmap binding/interpolation/native XEMU.')
(root/'artifacts/vfx-mesh-owned.json').write_text(json.dumps(report,indent=2));print(report)
