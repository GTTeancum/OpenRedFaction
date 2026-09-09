"""Complete original segment/AABB function, including failed-attempt writes."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);stack=base+60000;stop=base+64000

u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:uc.emu_stop(),begin=0x4deced,end=0x4deced)
rng=random.Random(0x4dec10);cases=[];expected=[];passes=0;guards=0
qbits=[0x20,0x40,0x400,2,0x1000,8,0x2000,0x800];fbits=[0x40,0x80,4,0x2000,1]
inputs=[]
for q in range(256):
 for f in range(32):
  for prop in [-1,0,1]:
   inputs.append([sum(bit for j,bit in enumerate(qbits) if q>>j&1),sum(bit for j,bit in enumerate(fbits) if f>>j&1),prop,rng.randrange(2),rng.choice([0,1,2,255]),rng.choice([0,1,255])])
for n in range(4096):
 values=[rng.getrandbits(32),rng.getrandbits(32),rng.choice([-32768,-1,0,1,32767]),rng.randrange(2),rng.choice([0,1,2,255]),rng.choice([0,1,255])]
 if n%13==0:values[rng.choice([2,3,4,5])]=65536
 inputs.append(values)
for values in inputs:
 q,f,prop,present,kind,state=values;wire=struct.pack('<IIiIII',*values);cases.append(wire)
 if not -32768<=prop<=32767 or present>1 or kind>255 or state>255:
  expected.append(struct.pack('<iI',-4,0xa5a5a5a5));guards+=1;continue
 face=base;query=base+4096;owner=base+8192
 u.mem_write(face+0x28,struct.pack('<I',f));u.mem_write(face+0x34,struct.pack('<h',prop));u.mem_write(face+0x44,struct.pack('<I',owner if present else 0))
 u.mem_write(query+0x50,struct.pack('<I',q));u.mem_write(owner,bytes([kind]));u.mem_write(owner+0x98,bytes([state]))
 u.mem_write(stack,struct.pack('<4I',stop,face,query,base+12288));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x4dec10,stop,count=10000);endpoint=u.reg_read(UC_X86_REG_EIP);assert endpoint in (stop,0x4deced)
 accepted=int(endpoint==0x4deced)
 if not accepted:assert u.reg_read(UC_X86_REG_EAX)&255==0
 passes+=accepted;expected.append(struct.pack('<iI',0,accepted))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--filter'],input=b''.join(cases))
assert len(actual)==len(expected)*8
for n,want in enumerate(expected):assert actual[n*8:(n+1)*8]==want,('PC',n,inputs[n],actual[n*8:(n+1)*8].hex(),want.hex())
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_face_accept\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match
entry=int(match.group(1),16)
for n,(wire,want) in enumerate(zip(cases,expected)):
 x.mem_write(base,wire);x.mem_write(base+64,struct.pack('<I',0xa5a5a5a5));x.mem_write(stack,struct.pack('<3I',stop,base,base+64));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+64,4))
 assert got==want,('NXDK',n,got.hex(),want.hex())
report=dict(result='PASS',original_cases=len(cases)-guards,accepted=passes,port_guards=guards,nxdk_cases=len(cases),nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),scope='Original 4dec10 filter prefix through 4deced, all seven predicates unchanged; all 256 relevant query masks x 32 face masks x signed property -1/0/1 plus random full words. Geometry and later texture tests excluded.')
(root/'artifacts/collision-filter-verification.json').write_text(json.dumps(report,indent=2));print(report)
