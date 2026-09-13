"""VFX timing original comparison and bounded mesh-prefix asset composition."""
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
def file_service(u,a,size,data):
 sp=u.reg_read(UC_X86_REG_ESP);pop=0
 if a==0x52cf60:
  target=read(u,sp+4);amount=read(u,sp+8);assert amount in (4,12) and read(u,sp+12)==read(u,sp+16)==0
  chunk=stream['data'][stream['at']:stream['at']+amount];assert len(chunk)==amount;u.mem_write(target,chunk);stream['at']+=amount;pop=16
 else:assert a==0x524530
 u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_EIP,read(u,sp));u.reg_write(UC_X86_REG_ESP,sp+4+pop)
for a in (0x52cf60,0x524530):o.hook_add(UC_HOOK_CODE,file_service,begin=a,end=a)
mp=(root/'build/xbox/main.map').read_text();sym=lambda name:int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',mp)[1],16)
def shared(name,version,flags,data,size):
 x.mem_write(B,data or b'\0');x.mem_write(OUT,b'\xa5'*size)
 args=[B,len(data),version]+([flags] if name=='rf_vfx_mesh_timing_read' else [])+[OUT]
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(OUT,size))
def timing(version,flags,data):
 stream.update(data=data,at=0);o.mem_write(OWNER,bytes(308));o.mem_write(CTX,bytes(0x114));o.mem_write(STACK,bytes(0x400));o.mem_write(0x17e5ff0,w(version));o.mem_write(OWNER+0x88,w(flags))
 o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBX,OWNER);o.reg_write(UC_X86_REG_EBP,CTX);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x53d47a,0x53d553,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x53d553
 return w(read(o,OWNER+0x88)&65535,read(o,OWNER+0xa4))+bytes(o.mem_read(OWNER+0xa8,8))+w(stream['at'])
inputs=[];wants=[];rng=random.Random(0x53d47a)
for i in range(2048):
 version=[0x30000,0x30008,0x30009,0x3000b,0x3000c,0x3000d,0x40005,0x40006][i%8];flags=rng.getrandbits(32);rate=rng.getrandbits(32)|1
 data=(w(rate) if version>=0x30009 else b'')+(f(rng.uniform(-100,100),rng.uniform(-100,100))+w(rng.getrandbits(32)) if version>=0x40004 else w(rng.getrandbits(32),rng.getrandbits(32)))
 inputs.append((version,flags,data));want=w(0)+timing(version,flags,data);assert shared('rf_vfx_mesh_timing_read',version,flags,data,20)==want;wants.append(want)
inv=json.loads((root/'artifacts/inventory.json').read_text());archive=next(a for a in inv['files'] if a['path']=='meshes.vpp');prefixes=[];prefix_wants=[];summary=[]
for asset in json.loads((root/'artifacts/vfx-header.json').read_text())['assets']:
 e=next(e for e in archive['vpp']['entries'] if e['name']==asset['name'])
 with (root/'Installed_Game/meshes.vpp').open('rb') as file:file.seek(e['offset']);data=file.read(e['size'])
 version=int(asset['version'],16);at=asset['header_bytes']
 while at<len(data):
  tag,length=struct.unpack_from('<II',data,at);end=at+4+length
  if tag==0x4f584653:
   payload=data[at+8:end];name,parent,tail=payload.split(b'\0',2);pos=len(name)+len(parent)+2
   enabled=int(payload[pos]!=0);pos+=1;vertices,faces=struct.unpack_from('<II',payload,pos);pos+=8;face_offset=pos;pos+=faces*96
   t=timing(version,enabled,payload[pos:]);used=pos+struct.unpack_from('<I',t,16)[0]
   resolved=(parent.split(b'-',1)[-1] if parent else b'Scene Root')
   want=w(0)+name.ljust(65,b'\0')+resolved.ljust(65,b'\0')+bytes(2)+w(vertices,faces,face_offset)+t+w(used)
   assert len(want)==172;assert shared('rf_vfx_mesh_prefix_read',version,0,payload,168)==want
   prefixes.append((version,0,payload));prefix_wants.append(want);summary.append(dict(asset=asset['name'],mesh=name.decode(),vertices=vertices,faces=faces,samples=struct.unpack_from('<I',t,4)[0],remaining_bytes=len(payload)-used))
  at=end
# Synthetic parents and early-version legacy vertices with no faces.
for parent in [b'',b'Root',b'prefix-Root',b'a-b-c',b'-',b'x'*64]:
 for version in [0x30000,0x30009,0x3000c,0x40006]:
  data=b'Mesh\0'+parent+b'\0'+b'\2'+w(1)+(bytes(12) if version<0x3000a else b'')+w(0);face_offset=len(data)
  td=(w(15) if version>=0x30009 else b'')+(f(0,1)+w(16) if version>=0x40004 else w(0,15));data+=td;t=timing(version,1,td)
  want=w(0)+b'Mesh'.ljust(65,b'\0')+(parent.split(b'-',1)[-1] if parent else b'Scene Root').ljust(65,b'\0')+bytes(2)+w(1,0,face_offset)+t+w(len(data))
  assert shared('rf_vfx_mesh_prefix_read',version,0,data,168)==want;prefixes.append((version,0,data));prefix_wants.append(want)
guards=[]
for version,flags,data in inputs[:8]:
 for n in range(len(data)):guards.append((version,flags,data[:n]))
guards += [(0x3000c,0,w(0,1,2)),(0x40006,0,w(15,0x7fc00000,0,1))]
for v,flags,d in guards:
 got=shared('rf_vfx_mesh_timing_read',v,flags,d,20);assert got[:4]!=w(0) and got[4:]==b'\xa5'*20;inputs.append((v,flags,d));wants.append(got)
pg=[]
for (v,flags,data),want in zip(prefixes,prefix_wants):
 used=struct.unpack_from('<I',want,168)[0]
 for n in [0,1,4,used-1]:pg.append((v,0,data[:n]))
v,_,data=prefixes[0];bad=bytearray(data);fo=struct.unpack_from('<I',prefix_wants[0],144)[0];bad[fo:fo+4]=w(0xffffffff);pg.append((v,0,bytes(bad)))
pg += [(0x40006,0,b'x'*65+b'\0Root\0'+bytes(64)),(0x40006,0,b'Mesh\0'+b'x'*256+b'\0'+bytes(64))]
for v,flags,d in pg:
 got=shared('rf_vfx_mesh_prefix_read',v,flags,d,168);assert got[:4]!=w(0) and got[4:]==b'\xa5'*168;prefixes.append((v,flags,d));prefix_wants.append(got)
probe=str(root/'build/pc/Release/rf_entity_assets_probe.exe')
for mode,cases,expected in [('--vfx-timing',inputs,wants),('--vfx-mesh-prefix',prefixes,prefix_wants)]:
 pc=subprocess.check_output([probe,mode],input=b''.join(w(v,flags,len(d))+d for v,flags,d in cases));assert pc==b''.join(expected),mode
report=dict(result='PASS',original_timing_cases=2048,timing_guards=len(guards),authored_meshes=summary,prefix_cases=len(prefixes)-len(pg),prefix_guards=len(pg),scope='Original53d47a..53d553 timing with actual version/integer/float helpers and supplied file services; exact PC/NXDK timing output. Prefix composes verified face/timing readers with independent authored/Synthetic framing checks, not full original mesh construction. No tracks, materials or native XEMU.')
(root/'artifacts/vfx-timing-prefix.json').write_text(json.dumps(report,indent=2));print(report)
