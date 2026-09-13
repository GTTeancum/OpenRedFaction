"""SFXO bounds and edges against original instructions and shared builds."""
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

EDGES=B+0x9000;LINKS=B+0xa000

def shared(v,flags,packed,faces,data):
 x.mem_write(B,data or b'\0');x.mem_write(OUT,b'\xa5'*44)
 status=call('rf_vfx_mesh_edges_read',[B,len(data),v,flags,packed,faces,OUT]);view=bytes(x.mem_read(OUT,44));edges=[]
 if not status:
  count,at=struct.unpack_from('<II',view,28)
  for _ in range(count):
   x.mem_write(SAMPLE,b'\xa5'*48);assert call('rf_vfx_edge_read',[B+at,len(data)-at,faces,SAMPLE])==0
   edge=bytes(x.mem_read(SAMPLE,48));edges.append(edge);at+=struct.unpack_from('<I',edge,44)[0]
 return w(status)+view+b''.join(edges)

def original(v,flags,packed,faces,data):
 stream.update(data=data,at=0);o.mem_write(CTX,bytes(0x114));o.mem_write(0x17e5ff0,w(v));o.mem_write(OWNER,bytes(0x124))
 o.mem_write(OWNER+0x114,w(flags));o.mem_write(OWNER+0x88,struct.pack('<H',packed&65535));o.mem_write(OWNER+0xb4,w(faces,FACE))
 o.mem_write(FACE,bytes(0x1800));o.mem_write(EDGES,bytes(0x1000));o.mem_write(LINKS,bytes(0x1000));o.mem_write(PARAM,bytes(0x80));o.mem_write(PARAM+0x30,w(0,EDGES,0,LINKS))
 o.mem_write(STACK,bytes(512));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBX,OWNER);o.reg_write(UC_X86_REG_EBP,CTX);o.reg_write(UC_X86_REG_EDI,PARAM);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x53d98e,0x53dc17,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==0x53dc17
 header=bytes(o.mem_read(OWNER+0x94,16))+w(read(o,OWNER+0x114))+bytes(o.mem_read(STACK+0x28,4))+bytes(o.mem_read(STACK+0x1c,4))
 count=read(o,OWNER+0xc4);edges=[]
 for i in range(count):
  edge=bytearray(o.mem_read(EDGES+i*44,44));n,pointer=struct.unpack_from('<II',edge,36)
  indices=[(read(o,pointer+j*4)-FACE)//0x90 for j in range(n)];edges.append((edge,indices))
 for i in range(faces):assert bytes(o.mem_read(FACE+i*0x90+0x84,12))==w(EDGES,EDGES,EDGES)
 assert read(o,PARAM+0x30)==count and read(o,PARAM+0x38)==sum(len(a) for _,a in edges)
 return header,count,read(o,OWNER+0x88)&65535,edges,stream['at']

inputs=[];authored=[];probe=str(root/'build/pc/Release/rf_entity_assets_probe.exe')
inv=json.loads((root/'artifacts/inventory.json').read_text());archive=next(a for a in inv['files'] if a['path']=='meshes.vpp')
for asset in json.loads((root/'artifacts/vfx-header.json').read_text())['assets']:
 v=int(asset['version'],16);e=next(e for e in archive['vpp']['entries'] if e['name']==asset['name'])
 with (root/'Installed_Game/meshes.vpp').open('rb') as file:file.seek(e['offset']);data=file.read(e['size'])
 at=asset['header_bytes']
 while at<len(data):
  tag,length=struct.unpack_from('<II',data,at);end=at+4+length
  if tag==0x4f584653:
   payload=data[at+8:end];prefix=subprocess.check_output([probe,'--vfx-mesh-prefix'],input=w(v,0,len(payload))+payload);assert prefix[:4]==w(0)
   faces=struct.unpack_from('<I',prefix,140)[0];packed,samples=struct.unpack_from('<II',prefix,148);offset=struct.unpack_from('<I',prefix,168)[0]
   count=struct.unpack_from('<I',payload,offset)[0];offset+=4
   if v>=0x40000:offset+=count*4
   else:
    for _ in range(count):
     d=payload[offset:];m=subprocess.check_output([probe,'--vfx-embedded-material'],input=w(v,packed,samples,len(d))+d);assert m[:4]==w(0);offset+=struct.unpack_from('<I',m,204)[0]
   # Original edges fit these explicit scratch bounds for all shipped meshes.
   assert faces*0x90<=4096 or faces==40
   inputs.append((v,0,packed,faces,payload[offset:]));authored.append(asset['name'])
  at=end
rng=random.Random(0x53d98e)
for i in range(1024):
 v=[0x30000,0x30001,0x30002,0x30008,0x30009,0x3000a,0x30012,0x40006][i%8];flags=rng.getrandbits(32);packed=rng.getrandbits(32);faces=i%9+1;count=i%7
 d=f(.25,-2,3,4)+(w(i%4) if v<0x30002 else b'')+w(i%2)
 if v==0x3000a and ((flags|i%2)&1):d+=f(.5,.75)
 d+=w(count)
 for j in range(count):
  n=(i+j)%5;d+=w(j,j+1)+f(.1,.2)+w(n)+w(*[rng.randrange(faces) for _ in range(n)])
 if v>=0x30009:d+=bytes([i%256])
 inputs.append((v,flags,packed,faces,d))
responses=[];edge_total=0
for case,(v,flags,packed,faces,d) in enumerate(inputs):
 got=shared(v,flags,packed,faces,d);assert got[:4]==w(0),(case,'decode')
 h,n,m,edges,consumed=original(v,flags,packed,faces,d);view=got[4:48]
 assert view[:28]==h and struct.unpack_from('<I',view,28)[0]==n and struct.unpack_from('<II',view,36)==(m,consumed),(case,'header')
 at=struct.unpack_from('<I',view,32)[0]
 for j,(raw,indices) in enumerate(edges):
  edge=got[48+j*48:96+j*48];raw[40:44]=edge[40:44];assert raw==edge[:44],(case,j,'edge')
  assert indices==list(struct.unpack_from('<'+'I'*len(indices),d,at+20));at+=struct.unpack_from('<I',edge,44)[0]
 edge_total+=n;responses.append(got)
invalid=[]
for v,flags,packed,faces,d in inputs[:len(authored)+8]:
 consumed=struct.unpack_from('<I',shared(v,flags,packed,faces,d),44)[0]
 for size in range(consumed):invalid.append((v,flags,packed,faces,d[:size]))
invalid += [(0x40000,0,0,0,bytes(24)),(0x30012,0,0,1,f(0,0,0,1)+w(0,0xffffffff)),(0x30012,0,0,1,f(0,0,0,1)+w(0,1,0,1)+f(0,0)+w(1,1)+b'\0')]
for v,flags,packed,faces,d in invalid:
 got=shared(v,flags,packed,faces,d);assert got[:4]!=w(0) and got[4:]==b'\xa5'*44;responses.append(got)
pc=subprocess.check_output([probe,'--vfx-mesh-edges'],input=b''.join(w(v,flags,packed,faces,len(d))+d for v,flags,packed,faces,d in inputs+invalid));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=len(inputs),authored_meshes=len(authored),edges=edge_total,guards=len(invalid),scope='Original53d98e..53dc17 with real helper instructions and supplied file services. All decoded edge fields, adjacency and consumed bytes compared with normalized pointers; zero-index reverse face links exercised. No track allocation or native XEMU.')
(root/'artifacts/vfx-mesh-edges.json').write_text(json.dumps(report,indent=2));print(report)
