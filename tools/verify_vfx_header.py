"""Original54b789..54bacb versioned VFX header before any allocation."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[i&0xffffffff for i in v])
B=0x30000000;S=B+0xf000;STOP=B+0xff00;OUT=B+0x2000
read=lambda u,a:struct.unpack('<I',u.mem_read(a,4))[0]
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32)
 u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(B,0x10000);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
o=machine(exe);x=machine(root/'build/xbox/main.exe');stream=dict(data=b'',at=8)
def file_service(u,a,size,data):
 sp=u.reg_read(UC_X86_REG_ESP);result=0;pop=0
 if a==0x52cf60:
  target=read(u,sp+4);amount=read(u,sp+8);assert amount==4 and read(u,sp+12)==read(u,sp+16)==0
  chunk=stream['data'][stream['at']:stream['at']+amount];assert len(chunk)==amount
  u.mem_write(target,chunk);stream['at']+=amount;pop=16
 elif a==0x524400:
  assert read(u,sp+4)==4 and read(u,sp+8)==1;stream['at']+=4;pop=8
 elif a!=0x524530:raise AssertionError(hex(a))
 u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_EIP,read(u,sp));u.reg_write(UC_X86_REG_ESP,sp+4+pop)
for a in (0x52cf60,0x524400,0x524530):o.hook_add(UC_HOOK_CODE,file_service,begin=a,end=a)
offsets=[0x44,0x84,0x4c,0x54,0x64,0x5c,0x6c,0x74,0x7c,0x88,0x90,0x98,0xa0,0xa8,0xb0,0xb8,0xc0,0xc8,0xd0,0xd8,0xe0,0xe8,0xf0,0xf8,0x100,0x108,0x110,0x118,0x120,0x128]
inv=json.loads((root/'artifacts/inventory.json').read_text());cat=json.loads((root/'artifacts/projectile-model-catalog.json').read_text());names={i['name'].lower() for i in cat['files'] if i['kind']==3};inputs=[];assets=[]
for archive in inv['files']:
 for entry in archive.get('vpp',{}).get('entries',[]):
  if entry['name'].lower() in names:
   with (root/'Installed_Game'/archive['path']).open('rb') as f:f.seek(entry['offset']);data=f.read(entry['size'])
   inputs.append(data);assets.append(dict(name=entry['name'],file_bytes=len(data)))
rng=random.Random(0x54b789);versions=[*range(0x30000,0x30011),0x40005,0x40006]
for i in range(1024):inputs.append(b'VSFX'+w(versions[i%len(versions)],*[rng.getrandbits(32) for _ in range(34)]))
expected=[]
for index,data in enumerate(inputs):
 version=struct.unpack_from('<I',data,4)[0];stream.update(data=data,at=8)
 o.mem_write(OUT,bytes([0xa5])*308);o.mem_write(S,bytes(0x800));o.mem_write(0x17e5ff0,w(version))
 o.reg_write(UC_X86_REG_ESP,S);o.reg_write(UC_X86_REG_EBP,OUT);o.reg_write(UC_X86_REG_EDI,0)
 o.emu_start(0x54b789,0x54bacb,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x54bacb
 expected.append(w(version,stream['at'],*[read(o,OUT+j) for j in offsets]))
 if index<len(assets):assets[index].update(version=hex(version),header_bytes=stream['at'],mesh_objects=read(o,OUT+0x4c),particle_objects=read(o,OUT+0x5c),frames=read(o,OUT+0x84))
mapping=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_vfx_header_read\s+([0-9a-fA-F]+)',mapping)[1],16)
def shared(data):
 x.mem_write(B,data or b'\0');x.mem_write(OUT,b'\xa5'*128);x.mem_write(S,w(STOP,B,len(data),OUT));x.reg_write(UC_X86_REG_ESP,S)
 x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(OUT,128))
responses=[]
for data,want in zip(inputs,expected):
 actual=shared(data);assert actual==w(0)+want;responses.append(actual)
guards=[b'NOPE'+inputs[0][4:]]+[b'VSFX'+w(v)+bytes(128) for v in [0,0x2ffff,0x40000,0x40001,0x40002,0x40003,0x40004,0xffffffff]]
# Every truncation before the consumed prefix of each installed projectile VFX.
for data,want in zip(inputs[:len(assets)],expected):
 for size in range(struct.unpack_from('<I',want,4)[0]):guards.append(data[:size])
for data in guards:
 actual=shared(data);assert actual[:4]!=w(0) and actual[4:]==b'\xa5'*128;responses.append(actual)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-header'],input=b''.join(w(len(d))+d for d in inputs+guards))
assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=len(inputs),guard_cases=len(guards),assets=assets,scope='Original54b789..54bacb with actual52c910 and523990; file read/seek/error primitives supplied. All30 header fields and consumed prefix match PC/NXDK, version defaults and derived uint32 wrap included. Port magic/version/truncation guards preserve output. No original full-file construction, resource allocations, chunk/track parsing, playback or native XEMU.')
(root/'artifacts/vfx-header.json').write_text(json.dumps(report,indent=2));print(report)
