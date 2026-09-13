"""VFX transform key tracks versus original loader instructions."""
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


TRANSLATE=B+0x9000;ROTATE=B+0xa000;SCALE=B+0xb000

def original(v,legacy,data):
 stream.update(data=data,at=0);o.mem_write(CTX,bytes(0x114));o.mem_write(0x17e5ff0,w(v));o.mem_write(OWNER,bytes(0x124));o.mem_write(OWNER+0xe0,w(PTR));o.mem_write(PTR,bytes(20));o.mem_write(PARAM,bytes(0x80));o.mem_write(PARAM+0x60,w(0,TRANSLATE,0,ROTATE,0,SCALE))
 for at in (TRANSLATE,ROTATE,SCALE):o.mem_write(at,bytes(4096))
 o.mem_write(0x173c378,legacy[:12]);o.mem_write(0x1818ba0,legacy[12:28]);o.mem_write(STACK,bytes(512));o.mem_write(STACK+0x150,w(PARAM));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBX,OWNER);o.reg_write(UC_X86_REG_EBP,CTX);o.reg_write(UC_X86_REG_EDI,PARAM);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x53e080,0x53e424,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==0x53e424
 counts=struct.unpack('<3H',o.mem_read(PTR,6));keys=b''
 for t,(count,at,stride) in enumerate(zip(counts,[TRANSLATE,ROTATE,SCALE],[40,20,40])):
  assert read(o,PARAM+0x60+t*8)==count
  for i in range(count):keys+=bytes(o.mem_read(at+i*stride,stride))+bytes(40-stride)
 return bytes(o.mem_read(OWNER+0xe4,40)),counts,keys,stream['at']
def shared(v,legacy,data):
 x.mem_write(B,data or b'\0');x.mem_write(PTR,legacy);x.mem_write(OUT,b'\xa5'*68)
 status=call('rf_vfx_key_tracks_read',[B,len(data),v,PTR,OUT]);view=bytes(x.mem_read(OUT,68));keys=b''
 if not status:
  counts=struct.unpack_from('<3I',view,40);offsets=struct.unpack_from('<3I',view,52)
  for t in range(3):
   for i in range(counts[t]):
    at=offsets[t]+i*40;assert call('rf_vfx_key_read',[B+at,len(data)-at,t,SAMPLE])==0;keys+=bytes(x.mem_read(SAMPLE,40))
 return w(status)+view+keys
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
    cfg=w(v,flags,packed,vertices,faces,index)+legacy;d=payload[offset:]
    frame=subprocess.check_output([probe,'--vfx-frame'],input=cfg+w(len(d))+d);assert frame[:4]==w(0);offset+=struct.unpack_from('<I',frame,112)[0]
   if packed&2:
    inputs.append((v,f(0,0,0,0,0,0,1),payload[offset:]));authored.append(asset['name'])
   else:assert offset==len(payload),(asset['name'],'trailing non-key bytes')
  at=end

rng=random.Random(0x53e080)
for i in range(1024):
 v=[0x30000,0x30009,0x3000a,0x3000d,0x40006][i%5];legacy=f(.25,-2,3,0,0,0,1);d=f(1,2,3,0,0,0,1,1,2,3) if v>=0x3000a else b''
 for t in range(3):
  n=(i+t)%5;d+=w(n|((i%4)<<16))
  for j in range(n):
   vals=[rng.uniform(-2,2) for _ in range(9)] if t!=1 else [rng.choice([-1,-.5,0,.5,1,2]) for _ in range(4)]+[rng.uniform(-300,300) for _ in range(5)]
   d+=w(j*7)+f(*vals)
 inputs.append((v,legacy,d))
responses=[];key_total=0
for case,(v,legacy,d) in enumerate(inputs):
 got=shared(v,legacy,d);assert got[:4]==w(0),(case,'decode')
 base,counts,keys,consumed=original(v,legacy,d);view=got[4:72]
 assert view[:40]==base and struct.unpack_from('<3I',view,40)==counts and got[72:]==keys,(case,'fields',got.hex(),base.hex(),counts,keys.hex())
 assert struct.unpack_from('<I',view,64)[0]==consumed==len(d),(case,consumed,len(d))
 key_total+=sum(counts);responses.append(got)
invalid=[]
for v,legacy,d in inputs[:len(authored)+12]:
 for n in range(len(d)):invalid.append((v,legacy,d[:n]))
invalid += [(0x40000,f(0,0,0,0,0,0,1),bytes(52)),(0x40006,f(0,0,0,0,0,0,1),bytes(40)+w(65535)),(0x40006,f(0,0,0,0,0,0,1),bytes(40)+w(0,1,0)+f(1e20)+bytes(32)+w(0))]
for v,legacy,d in invalid:
 got=shared(v,legacy,d);assert got[:4]!=w(0) and got[4:]==b'\xa5'*68;responses.append(got)
pc=subprocess.check_output([probe,'--vfx-keys'],input=b''.join(w(v,len(d))+legacy+d for v,legacy,d in inputs+invalid));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=len(inputs),authored_meshes=len(authored),keys=key_total,guards=len(invalid),scope='Original53e080..53e424 with actual helpers and supplied file read/error. Legacy position/quaternion globals explicitly seeded from caller defaults. All base fields, low16 counts, converted records and consumed bytes match; pointer representation normalized. No interpolation/owner binding/native XEMU.')
(root/'artifacts/vfx-keys.json').write_text(json.dumps(report,indent=2));print(report)
