"""Serialized VFX frames versus original loader instructions."""
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
  target=read(u,sp+4);amount=read(u,sp+8);assert read(u,sp+12)<=read(u,0x17e5ff0) and read(u,sp+16)==0
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

FRAMES=B+0x7000;DEF=B+0x8000;VERT=B+0x9000;UVS=B+0xa000;TRANS=B+0xb000;MAT=B+0xc000;OPACITY=B+0xd000
direction=[]
def extra_service(u,a,size,data):
 if a==0x4fab70:direction.append(bytes(u.mem_read(u.reg_read(UC_X86_REG_ECX),12)));return
 sp=u.reg_read(UC_X86_REG_ESP);assert read(u,sp+8)==1;stream['at']+=read(u,sp+4);assert stream['at']<=len(stream['data'])
 u.reg_write(UC_X86_REG_ESP,sp+12);u.reg_write(UC_X86_REG_EIP,read(u,sp))
for a in (0x524400,0x4fab70):o.hook_add(UC_HOOK_CODE,extra_service,begin=a,end=a)
def original(cfg,data):
 v,flags,packed,vertices,faces,index=struct.unpack_from('<6I',cfg);assert index<64 and vertices*6<4096 and faces*24<4096
 stream.update(data=data,at=0);direction.clear();o.mem_write(CTX,bytes(0x114));o.mem_write(0x17e5ff0,w(v));o.mem_write(OWNER,bytes(0x124))
 for at in (FRAMES,DEF,VERT,UVS,TRANS,MAT,OPACITY):o.mem_write(at,bytes(4096))
 o.mem_write(OWNER+0x114,w(flags));o.mem_write(OWNER+0x88,w(packed&65535));o.mem_write(OWNER+0xb0,w(vertices,faces));o.mem_write(OWNER+0xcc,w(FRAMES));o.mem_write(OWNER+0xd4,w(OUT));o.mem_write(OWNER+0xdc,w(TRANS))
 o.mem_write(FRAMES+index*40+4,w(VERT));o.mem_write(OUT+index*4,w(UVS));o.mem_write(OWNER+0x84,w(DEF));o.mem_write(DEF+0x8c,w(MAT));o.mem_write(MAT+0xc4,w(OPACITY));o.mem_write(OWNER+0xbc,w(1,PTR));o.mem_write(PTR,w(0))
 o.mem_write(STACK,bytes(512));o.mem_write(STACK+0x28,cfg[24:28]);o.mem_write(STACK+0x1c,cfg[28:32]);o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBX,OWNER);o.reg_write(UC_X86_REG_EBP,CTX);o.reg_write(UC_X86_REG_ESI,index);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x53ddb8,0x53e05d,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==0x53e05d
 assert read(o,FRAMES+index*40)==0x80000000
 return dict(vectors=bytes(o.mem_read(FRAMES+index*40+8,32)),transform=bytes(o.mem_read(TRANS+index*40,40)),opacity=bytes(o.mem_read(OPACITY+index*4,4)),direction=direction[0] if direction else bytes(12),vertices=bytes(o.mem_read(VERT,vertices*6)),uv=bytes(o.mem_read(UVS,faces*24)),consumed=stream['at'])
def shared(cfg,data):
 x.mem_write(B,data or b'\0');x.mem_write(PTR,cfg);x.mem_write(OUT,b'\xa5'*112)
 status=call('rf_vfx_frame_read',[B,len(data),PTR,OUT]);return w(status)+bytes(x.mem_read(OUT,112))
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
   vertices,faces=struct.unpack_from('<II',prefix,136);packed,samples=struct.unpack_from('<II',prefix,148);offset=struct.unpack_from('<I',prefix,168)[0]
   count=struct.unpack_from('<I',payload,offset)[0];offset+=4
   if v>=0x40000:offset+=count*4
   else:
    for _ in range(count):
     d=payload[offset:];m=subprocess.check_output([probe,'--vfx-embedded-material'],input=w(v,packed,samples,len(d))+d);assert m[:4]==w(0);offset+=struct.unpack_from('<I',m,204)[0]
   d=payload[offset:];edge=subprocess.check_output([probe,'--vfx-mesh-edges'],input=w(v,0,packed,faces,len(d))+d);assert edge[:4]==w(0)
   flags=struct.unpack_from('<I',edge,20)[0];legacy=edge[24:32];packed=struct.unpack_from('<I',edge,40)[0];offset+=struct.unpack_from('<I',edge,44)[0]
   for index in range(samples):
    cfg=w(v,flags,packed,vertices,faces,index)+legacy;r=original(cfg,payload[offset:]);consumed=r['consumed'];inputs.append((cfg,payload[offset:offset+consumed]));offset+=consumed;authored.append(asset['name'])
  at=end
rng=random.Random(0x53ddb8)
for i in range(1024):
 v=[0x30000,0x30008,0x30009,0x3000a,0x3000b,0x3000d,0x3000e,0x40006][i%8];flags=[0,1,4,0x100,0x104,0x800,0x805,0x901][(i//8)%8];packed=(i//64)%2*2;index=(i//128)%3;vertices=i%8;faces=i%5
 cfg=w(v,flags,packed,vertices,faces,index)+f(-1,.75);d=b''
 if flags&4 or index==0:
  d+=f(.1,.2,.3,1,2,3)+struct.pack('<'+'H'*(vertices*3),*[rng.randrange(65536) for _ in range(vertices*3)])
  if flags&0x801:
   if v>=0x3000b:d+=f(.5,.75)
   if flags&0x800 and index==0 and v>=0x40001:d+=f(.2,.3,.4)
 if v>=0x3000d and (flags&0x100 or index==0):d+=f(*[rng.uniform(-2,2) for _ in range(faces*6)])
 if not flags&4 and (not packed&2 or (v<0x3000e and index==0)):d+=f(1,2,3,0,0,0,1,1,1,1)
 if v<0x30009:d+=b'\x7f'
 if v<0x40005:d+=f(rng.choice([-1,-0.0,0,.5,1,2]))
 inputs.append((cfg,d))
responses=[]
for case,(cfg,d) in enumerate(inputs):
 got=shared(cfg,d);assert got[:4]==w(0),(case,'decode');view=got[4:];r=original(cfg,d)
 assert view[:32]==r['vectors'] and view[48:88]==r['transform'] and view[88:92]==r['opacity'] and view[92:104]==r['direction'],(case,'fields',view.hex(),r)
 assert struct.unpack_from('<I',view,108)[0]==r['consumed']==len(d)
 vo,vb,uo,ub,present=(*struct.unpack_from('<4I',view,32),struct.unpack_from('<I',view,104)[0])
 if present&1:assert d[vo:vo+vb]==r['vertices']
 if present&8:
  uv=b''
  for at in range(uo,uo+ub,24):
   vals=struct.unpack_from('<6I',d,at);uv+=w(vals[0],vals[2],vals[4],vals[1],vals[3],vals[5])
  assert uv==r['uv'],(case,'UV')
 responses.append(got)
invalid=[]
for cfg,d in inputs[:min(40,len(authored))]+inputs[-16:]:
 for n in range(len(d)):invalid.append((cfg,d[:n]))
invalid += [(w(0x40000,0,0,0,0,0)+f(0,0),bytes(64)),(w(0x40006,4,0,0xffffffff,0,0)+f(0,0),bytes(24)),(w(0x40006,0,0,0,0,0)+f(0,0),w(0x7fc00000)+bytes(60))]
for cfg,d in invalid:
 got=shared(cfg,d);assert got[:4]!=w(0) and got[4:]==b'\xa5'*112;responses.append(got)
pc=subprocess.check_output([probe,'--vfx-frame'],input=b''.join(cfg+w(len(d))+d for cfg,d in inputs+invalid));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=len(inputs),authored_frames=len(authored),guards=len(invalid),scope='Original53ddb8..53e05d with actual helpers; supplied file read/error/seek only. Direction captured before actual normalization to compare serialized view. Quantized vertices, deinterleaved UVs, transforms, opacity and consumed bytes match; no key tracks, resource ownership or native XEMU.')
(root/'artifacts/vfx-frame.json').write_text(json.dumps(report,indent=2));print(report)
