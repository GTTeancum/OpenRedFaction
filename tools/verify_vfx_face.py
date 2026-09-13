"""Complete original SFXO face decoding vs PC/NXDK, including authored faces."""
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
inputs=[];asset_faces=[];inv=json.loads((root/'artifacts/inventory.json').read_text());archive=next(a for a in inv['files'] if a['path']=='meshes.vpp')
for asset in json.loads((root/'artifacts/vfx-header.json').read_text())['assets']:
 e=next(e for e in archive['vpp']['entries'] if e['name']==asset['name'])
 with (root/'Installed_Game/meshes.vpp').open('rb') as stream_file:stream_file.seek(e['offset']);data=stream_file.read(e['size'])
 version=int(asset['version'],16);at=asset['header_bytes'];total=0
 while at<len(data):
  tag,length=struct.unpack_from('<II',data,at);end=at+4+length
  if tag==0x4f584653:
   pos=at+8
   for _ in range(2):pos=data.index(b'\0',pos)+1
   pos+=1;vertices=struct.unpack_from('<I',data,pos)[0];pos+=4
   if version<0x3000a:pos+=vertices*12
   count=struct.unpack_from('<I',data,pos)[0];pos+=4;size=120 if version<0x3000d else 96
   assert pos+count*size<=end
   for i in range(count):
    face=data[pos+i*size:pos+(i+1)*size];assert max(struct.unpack_from('<3I',face))<vertices
    inputs.append((version,face));total+=1
  at=end
 asset_faces.append(dict(name=asset['name'],faces=total))
authored=len(inputs);rng=random.Random(0x53d277)
for i in range(2048):
 version=[0x30000,0x3000c,0x3000d,0x3000e,0x40005,0x40006][i%6]
 data=w(*[rng.getrandbits(32) for _ in range(3)])
 if version<0x3000d:data+=f(*[rng.uniform(-10,10) for _ in range(6)])
 data+=f(*[rng.uniform(-2,3) if i%3 else rng.choice([0,.5,1,1/255,128/255]) for _ in range(9)])
 data+=f(*[rng.uniform(-10,10) for _ in range(7)])+w(*[rng.getrandbits(32) for _ in range(5)])
 inputs.append((version,data))
expected=[]
for version,data in inputs:
 stream.update(data=data,at=0);o.mem_write(OWNER,bytes(308));o.mem_write(FACE,b'\xa5'*144);o.mem_write(UV,bytes(24));o.mem_write(PTR,w(UV));o.mem_write(CTX,bytes(0x114));o.mem_write(STACK,bytes(0x400));o.mem_write(0x17e5ff0,w(version))
 o.mem_write(OWNER+0xb4,w(1,FACE));o.mem_write(OWNER+0xd4,w(PTR));o.mem_write(STACK+0x20,w(12));o.mem_write(STACK+0x27,b'\1')
 o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBX,OWNER);o.reg_write(UC_X86_REG_EBP,CTX);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x53d277,0x53d47a,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x53d47a and stream['at']==len(data)
 raw=bytes(o.mem_read(FACE,144));expected.append(w(0)+raw[0x14:0x20]+bytes(o.mem_read(UV,24))+raw[0x54:0x5d]+bytes(3)+raw[0x60:0x7c]+raw[0x20:0x24]+raw[0x80:0x90]+w(len(data)))
mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_vfx_face_read\s+([0-9a-fA-F]+)',mp)[1],16)
def shared(version,data):
 x.mem_write(B,data or b'\0');x.mem_write(OUT,b'\xa5'*100);x.mem_write(STACK,w(STOP,B,len(data),version,OUT));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(OUT,100))
for (version,data),want in zip(inputs,expected):assert shared(version,data)==want
base=next(d for v,d in inputs if v==0x40006);guards=[(0x40006,base[:n]) for n in range(96)]
legacy=next(d for v,d in inputs if v==0x30000);guards += [(0x30000,legacy[:n]) for n in range(120)]
for version in [0,0x2ffff,0x40000,0x40004,0xffffffff]:guards.append((version,base))
for offset in range(12,76,4):
 bad=bytearray(base);bad[offset:offset+4]=w(0x7fc00000);guards.append((0x40006,bytes(bad)))
bad=bytearray(base);bad[12:16]=f(1e20);guards.append((0x40006,bytes(bad)))
for version,data in guards:
 got=shared(version,data);assert got[:4]!=w(0) and got[4:]==b'\xa5'*100;expected.append(got)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-face'],input=b''.join(w(v,len(d))+d for v,d in inputs+guards));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=len(inputs),authored_faces=authored,assets=asset_faces,guards=len(guards),scope='Original53d277..53d474 with actual integer/float/vector/version readers and255 multiplier/ftol; only file read/error primitives supplied. Missing-material diagnostic suppressed; all represented face fields and consumed bytes match PC/NXDK. Synthetic version/material/color cases and every projectile SFXO face. No full mesh parser, vertex tracks, material resolution or native XEMU.')
(root/'artifacts/vfx-face.json').write_text(json.dumps(report,indent=2));print(report)
