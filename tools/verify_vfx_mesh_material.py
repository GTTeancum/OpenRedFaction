"""Owned embedded VFX material sampling against original scalar routines."""
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
  if not fail_alloc:
   ret=HEAP if not live else (max(a+n for a,n in live.items())+15)&~15
   assert ret+arg<=HEAP+0x400000;live[ret]=arg;u.mem_write(ret,b'\xcc'*arg)
 else:
  calls.append(('free',arg))
  if arg:assert arg in live;del live[arg]
 u.reg_write(UC_X86_REG_EAX,ret);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(u,sp))
for name in ('malloc','free'):x.hook_add(UC_HOOK_CODE,allocator,begin=sym(name),end=sym(name))
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
responses=[];checks=0;guards=0;TRAMP=B+0xf000
for version,g,budget,data in inputs:
 x.mem_write(B,data);x.mem_write(PTR,f(0,0,0,0,0,0,1));x.mem_write(OUT,w(0));assert call('rf_vfx_mesh_open',[B,len(data),version,g,PTR,budget,OUT])==0
 mesh=read(x,OUT);owned=read(x,mesh+304);frames=read(x,mesh+300);count=read(x,mesh+148);materials=read(x,mesh+284);at=read(x,mesh+288);result=b''
 for material in range(materials):
  arrays=[b'',b'',b''];rate=0
  if version<0x40000:
   assert call('rf_vfx_embedded_material_read',[owned+at,len(data)-at,version,read(x,mesh+144),count,OWNER])==0
   view=struct.unpack('<53I',x.mem_read(OWNER,212));rate=view[30]
   if view[31]:
    raw=struct.unpack('<'+'f'*view[31],x.mem_read(owned+at+view[32],view[31]*4));arrays[0]=f(*[min(1,max(0,v)) for v in raw])
   arrays[1]=w(view[52]);arrays[2]=b''.join(bytes(x.mem_read(frames+i*112+88,4)) for i in range(count));at+=view[50]
  for track in range(3):
   for time in (0,.5,2,10000):
    x.mem_write(SAMPLE,b'\xa5'*4);status=call('rf_vfx_mesh_material_evaluate',[mesh,material,track,struct.unpack('<I',f(time))[0],SAMPLE]);got=bytes(x.mem_read(SAMPLE,4));result+=w(status)+got
    if version>=0x40000 or not arrays[track]:assert status!=0 and got==b'\xa5'*4;guards+=1;continue
    assert status==0;values=arrays[track];o.mem_write(B,values);o.mem_write(OWNER,bytes(200));o.mem_write(OWNER,w(1));o.mem_write(OWNER+0x78,w(rate));o.mem_write(OWNER+[0x7c,0xb8,0xc0][track],w(len(values)//4,B));o.mem_write(TRAMP,b'\xd9\x1d'+w(SAMPLE)+b'\x68'+w(STOP)+b'\xc3')
    o.mem_write(STACK,w(TRAMP)+f(time));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,OWNER);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start([0x54a930,0x54a9e0,0x54aa80][track],STOP,count=10000);assert o.reg_read(UC_X86_REG_EIP)==STOP
    assert got==bytes(o.mem_read(SAMPLE,4)),(version,material,track,time);checks+=1
 x.mem_write(SAMPLE,b'\xa5'*4);assert call('rf_vfx_mesh_material_evaluate',[mesh,materials,1,0,SAMPLE])!=0 and bytes(x.mem_read(SAMPLE,4))==b'\xa5'*4;guards+=1
 call('rf_vfx_mesh_close',[OUT]);assert not live;responses.append(result)
pc=subprocess.check_output([probe,'--vfx-mesh-material'],input=b''.join(w(v,g,b,len(d))+f(0,0,0,0,0,0,1)+d for v,g,b,d in inputs));assert pc==b''.join(responses)
report=dict(result='PASS',authored_meshes=len(inputs),original_pc_nxdk_scalar_checks=checks,missing_global_or_range_guards=guards,scope='Owned embedded material traversal and frame opacity binding, original scalar routines supplied decoded arrays. Separate parser/frame harnesses verify decoding; no native XEMU or drawing.')
(root/'artifacts/vfx-mesh-material.json').write_text(json.dumps(report,indent=2));print(report)
