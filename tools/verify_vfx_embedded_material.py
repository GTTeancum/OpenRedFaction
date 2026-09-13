"""Older SFXO embedded materials against original instructions and both builds."""
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

def shared(version,flags,samples,data):
 x.mem_write(B,data or b'\0');x.mem_write(OUT,b'\xa5'*212)
 status=call('rf_vfx_embedded_material_read',[B,len(data),version,flags,samples,OUT]);view=bytes(x.mem_read(OUT,212));values=b''
 if not status:
  for index in range(struct.unpack_from('<I',view,124)[0]):
   assert call('rf_vfx_material_sample',[B,len(data),OUT,0,index,SAMPLE])==0
   values+=bytes(x.mem_read(SAMPLE,4))
 return w(status)+view+values

def original(version,flags,samples,data):
 stream.update(data=data,at=0);lookup_names.clear()
 o.mem_write(OWNER,bytes(0x124));o.mem_write(OWNER+0x88,struct.pack('<H',flags&65535));o.mem_write(OWNER+0xa4,w(samples));o.mem_write(OWNER+0xbc,w(1,PTR))
 o.mem_write(FACE,bytes(200))
 for offset in (16,68,180):o.mem_write(FACE+offset,w(-1))
 o.mem_write(CTX,bytes(0x114));o.mem_write(COUNTS,bytes(16));o.mem_write(PARAM,w(0,0,COUNTS,FACE,COUNTS+4,BLEND,COUNTS+8,COLOR,COUNTS+12,ALPHA));o.mem_write(0x17e5ff0,w(version))
 o.mem_write(STACK,bytes(512));o.mem_write(STACK+0x150,w(PARAM));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBX,OWNER);o.reg_write(UC_X86_REG_EBP,CTX);o.reg_write(UC_X86_REG_EDI,PARAM);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x53d5d1,0x53d98e,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==0x53d98e
 raw=bytearray(o.mem_read(FACE,200));count=struct.unpack_from('<I',raw,124)[0]
 assert bytes(o.mem_read(COUNTS,16))==w(1,count,1,samples)
 return raw,bytes(o.mem_read(BLEND,count*4)),bytes(o.mem_read(COLOR,4)),stream['at'],list(lookup_names)

inputs=[];authored=[]
inv=json.loads((root/'artifacts/inventory.json').read_text());archive=next(a for a in inv['files'] if a['path']=='meshes.vpp')
for asset in json.loads((root/'artifacts/vfx-header.json').read_text())['assets']:
 version=int(asset['version'],16)
 if version>=0x40000:continue
 e=next(e for e in archive['vpp']['entries'] if e['name']==asset['name'])
 with (root/'Installed_Game/meshes.vpp').open('rb') as file:file.seek(e['offset']);data=file.read(e['size'])
 at=asset['header_bytes']
 while at<len(data):
  tag,length=struct.unpack_from('<II',data,at);end=at+4+length
  if tag==0x4f584653:
   payload=data[at+8:end]
   prefix=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-mesh-prefix'],input=w(version,0,len(payload))+payload)
   assert prefix[:4]==w(0) and len(prefix)==172
   flags,samples=struct.unpack_from('<II',prefix,148);offset=struct.unpack_from('<I',prefix,168)[0]
   count=struct.unpack_from('<I',payload,offset)[0];offset+=4
   for material in range(count):
    raw,blend,color,consumed,names=original(version,flags,samples,payload[offset:])
    inputs.append((version,flags,samples,payload[offset:offset+consumed]));authored.append(asset['name']);offset+=consumed
  at=end
rng=random.Random(0x53d5d1)
for i in range(1024):
 version=[0x30000,0x30002,0x30003,0x30006,0x30007,0x3000d,0x3000e,0x30010,0x30011,0x30012][i%10]
 kind=[0,1,2,3,0xffffffff][(i//10)%5];samples=i%9;flags=rng.getrandbits(32);data=w(kind)
 if kind==2:data+=w(*[rng.getrandbits(32) for _ in range(3)])
 else:
  data+=(bytes([i%256]) if version>=0x30003 else b'')+[b'',b'texture.tga',b'$original_map_rgb'][i%3]+b'\0'
  if version>=0x30012:data+=w(2)+f(.5)+w(3)
  if kind==1:
   data+=(b'second.tga' if i%2 else b'')+b'\0'
   if version>=0x30012:data+=w(4)+f(.25)+w(5)
  if version<0x30012:data+=w(2,3)
  if version>=0x30007:data+=f(.1,.2,.3)+(b'glare.tga' if i%2 else b'')+b'\0'
  if kind==1:data+=f(*[rng.choice([-1,-0.0,0,.25,1,2]) for _ in range(samples)])
 if version>=0x30011:data+=f(.75)
 inputs.append((version,flags,samples,data))
responses=[]
for case,(version,flags,samples,data) in enumerate(inputs):
 got=shared(version,flags,samples,data);assert got[:4]==w(0),(case,got[:4])
 raw,blend,color,consumed,names=original(version,flags,samples,data);view=got[4:204]
 for ptrword in (32,47,49):raw[ptrword*4:ptrword*4+4]=view[ptrword*4:ptrword*4+4]
 assert raw==view,(case,[(i,raw[i:i+4].hex(),view[i:i+4].hex()) for i in range(0,200,4) if raw[i:i+4]!=view[i:i+4]])
 assert got[212:216]==color and got[216:]==blend,(case,'tracks')
 assert struct.unpack_from('<I',got,204)[0]==consumed==len(data)
 mask=struct.unpack_from('<I',got,208)[0];parsed=[view[p:p+33].split(b'\0')[0] for p in (20,72,144)]
 assert [n for i,n in enumerate(parsed) if mask&(1<<i)]==names
 responses.append(got)
invalid=[]
for version,flags,samples,data in inputs[:50]:
 for size in range(len(data)):invalid.append((version,flags,samples,data[:size]))
invalid += [(0x40000,0,0,w(2)),(0x2ffff,0,0,w(2)),(0x30012,0,0x80000000,w(2)),(0x30012,0,0,w(0)+b'\1'+b'x'*33+b'\0'+bytes(64))]
for v,flags,n,d in invalid:
 got=shared(v,flags,n,d);assert got[:4]!=w(0) and got[4:]==b'\xa5'*212;responses.append(got)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-embedded-material'],input=b''.join(w(v,flags,n,len(d))+d for v,flags,n,d in inputs+invalid));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=len(inputs),authored_records=len(authored),guards=len(invalid),scope='Original53d5d1..53d98e with actual version/string/math helpers, supplied file and bitmap services. Pointer fields normalized; pending opacity reservation checked, no bitmap ownership or native XEMU.')
(root/'artifacts/vfx-embedded-material.json').write_text(json.dumps(report,indent=2));print(report)

