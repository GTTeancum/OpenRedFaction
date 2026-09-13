"""Complete standalone MATL parsing vs original with deferred bitmap binding."""
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
def shared(version,data):
 x.mem_write(B,data or b'\0');x.mem_write(OUT,b'\xa5'*208);status=call('rf_vfx_material_read',[B,len(data),version,OUT]);view=bytes(x.mem_read(OUT,208));values=b''
 if not status:
  for track,countword in enumerate([31,46,48]):
   for index in range(struct.unpack_from('<I',view,countword*4)[0]):
    assert call('rf_vfx_material_sample',[B,len(data),OUT,track,index,SAMPLE])==0;values+=bytes(x.mem_read(SAMPLE,4))
 return w(status)+view+values
inputs=[];inv=json.loads((root/'artifacts/inventory.json').read_text());archive=next(a for a in inv['files'] if a['path']=='meshes.vpp');authored=[]
for asset in json.loads((root/'artifacts/vfx-header.json').read_text())['assets']:
 e=next(e for e in archive['vpp']['entries'] if e['name']==asset['name'])
 with (root/'Installed_Game/meshes.vpp').open('rb') as file:file.seek(e['offset']);data=file.read(e['size'])
 at=asset['header_bytes'];version=int(asset['version'],16)
 while at<len(data):
  tag,length=struct.unpack_from('<II',data,at);end=at+4+length
  if tag==0x4c54414d:inputs.append((version,data[at+8:end]));authored.append(asset['name'])
  at=end
rng=random.Random(0x54ab20)
for i in range(1024):
 version=[0x3000d,0x40005,0x40006][i%3];kind=[0,1,2,3,0xffffffff][i%5];data=w(kind)+(w(15) if version>=0x40003 else b'')
 if kind<=2:
  if kind==2:data+=(bytes([i%256]) if version>=0x40006 else b'')+w(*[rng.getrandbits(32) for _ in range(3)])
  else:
   name=[b'',b'texture.tga',b'$original_map',b'$original_map_rgb',b'$original_map_rgbX'][(i//5)%5]
   data+=bytes([i%256])+name+b'\0'+w(2)+f(.5)+w(3)
   if kind==1:
    second=[b'',b'second.tga',b'$original_map',b'$original_map_rgb'][i%4];count=i%7
    data+=second+b'\0'+w(4)+f(.25)+w(5,count)+(w(24) if version<0x40003 else b'')+f(*[rng.choice([-1,-0.0,0,.25,1,2]) for _ in range(count)])
   data+=f(.1,.2,.3)+(b'glare.tga' if i%2 else b'')+b'\0'
  count=i%5 if version>=0x40003 else 1;data+=(w(count) if version>=0x40003 else b'')+w(*[rng.getrandbits(32) for _ in range(count)])
  if version>=0x40005:
   count=i%6;data+=w(count)+w(*[rng.getrandbits(32) for _ in range(count)])
 inputs.append((version,data))
responses=[]
for case,(version,data) in enumerate(inputs):
 got=shared(version,data);assert got[:4]==w(0),(case,got[:4]);view=bytearray(got[4:204]);stream.update(data=data,at=0);lookup_names.clear()
 o.mem_write(OWNER,bytes(200));
 for offset in (16,68,180):o.mem_write(OWNER+offset,w(-1))
 o.mem_write(CTX,bytes(0x114));o.mem_write(COUNTS,bytes(12));o.mem_write(PARAM,w(COUNTS,BLEND,COUNTS+4,COLOR,COUNTS+8,ALPHA));o.mem_write(0x17e5ff0,w(version))
 o.mem_write(STACK,w(STOP,CTX,PARAM));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,OWNER);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x54ab20,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 raw=bytearray(o.mem_read(OWNER,200));expected_values=b''
 for countword,ptrword in [(31,32),(46,47),(48,49)]:
  count=struct.unpack_from('<I',raw,countword*4)[0];pointer=struct.unpack_from('<I',raw,ptrword*4)[0]
  expected_values+=bytes(o.mem_read(pointer,count*4)) if count else b''
  # Pointer representations differ; sample equality verifies each mapped span.
  raw[ptrword*4:ptrword*4+4]=view[ptrword*4:ptrword*4+4]
 assert raw==view,(case,'fields',[(i,raw[i:i+4].hex(),view[i:i+4].hex()) for i in range(0,200,4) if raw[i:i+4]!=view[i:i+4]])
 assert got[212:]==expected_values,(case,'samples',got[212:].hex(),expected_values.hex())
 assert struct.unpack_from('<I',got,204)[0]==stream['at']==len(data)
 mask=struct.unpack_from('<I',got,208)[0];names=[bytes(view[o:o+33]).split(b'\0')[0] for o in (20,72,144)]
 assert [n for i,n in enumerate(names) if mask&(1<<i)]==lookup_names,(case,lookup_names,names,mask)
 responses.append(got)
invalid=[]
for version,data in inputs[:len(authored)]:
 for length in range(len(data)):invalid.append((version,data[:length]))
invalid += [(0x40006,w(0,15)+b'\1'+b'x'*33+b'\0'+bytes(64)),(0x40006,w(2,15)+b'\1'+w(1,2,3,0xffffffff))]
for version,data in invalid:
 got=shared(version,data);assert got[:4]!=w(0) and got[4:]==b'\xa5'*208;responses.append(got)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-material'],input=b''.join(w(v,len(d))+d for v,d in inputs+invalid));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=len(inputs),authored_records=len(authored),guards=len(invalid),scope='Complete original54ab20 with actual file/string/version/math helpers, supplied file read/error and bitmap lookup(-1). All represented fields, lookup requests, samples and consumed bytes match PC/NXDK; pointer spans normalized. No bitmap allocation or old embedded material decoder; input-backed views require retained input. No native XEMU.')
(root/'artifacts/vfx-material.json').write_text(json.dumps(report,indent=2));print(report)
