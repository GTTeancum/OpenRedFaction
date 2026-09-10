"""Original complete ordered movement-region scan versus PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;regions=base+0x1000;pointers=base+0x2000;point=base+0x3000;output=base+0x4000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(im)+4095)//4096*4096);u.mem_write(origin,im);u.mem_map(base,65536);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe)
# Static vector constructors have already run; no exit-handler registration needed.
u.mem_write(0x1754474,b'\x07')
u.mem_write(pointers,w(regions,regions+64,regions+128))
rng=random.Random(0x45cca0);commands=[];expected=[]
identity=(1,0,0,0,1,0,0,0,1)
fixtures=[]
# Exact face, edge and corner points plus adjacent binary32 values.
for axis in range(3):
 for sign in (-1,1):
  for bits in (0x3f7fffff,0x3f800000,0x3f800001):
   p=[0,0,0];p[axis]=sign*struct.unpack('<f',w(bits))[0]
   fixtures.append((3,p,[(i,[0,0,0],identity,[2,2,2]) for i in range(3)]))
for p in ((1,1,1),(-1,-1,-1),(0,0,0)):
 fixtures.append((3,p,[(i,[0,0,0],identity,[2,2,2]) for i in range(3)]))
for case in range(1500):
 boxes=[]
 for i in range(3):
  a=rng.uniform(-math.pi,math.pi);c=math.cos(a);s=math.sin(a)
  matrix=(c,0,s,0,1,0,-s,0,c)
  boxes.append((rng.randrange(4),[rng.uniform(-4,4) for _ in range(3)],matrix,[rng.uniform(0,8) for _ in range(3)]))
 p=[rng.uniform(-5,5) for _ in range(3)]
 if case%5==0:p=boxes[case%3][1]
 fixtures.append((case%4,p,boxes))
for count,p,boxes in fixtures:
 data=b''.join(w(kind)+f(*center,*matrix,*size) for kind,center,matrix,size in boxes)
 u.mem_write(regions,data);u.mem_write(point,f(*p));u.mem_write(0x6460b0,w(count,3,pointers))
 u.mem_write(stack,w(stop,point));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x45cca0,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 result=u.reg_read(UC_X86_REG_EAX);index=(result-regions)//64 if result else 0xffffffff
 assert index==0xffffffff or index<count
 assert bytes(u.mem_read(regions,192))==data
 commands.append(w(count)+f(*p)+data);expected.append(index)
probe=root/'build/pc/Release/rf_entity_probe.exe'
actual=subprocess.check_output([str(probe),'--player-regions'],input=b''.join(commands))
assert actual==w(*expected),next((i for i,n in enumerate(expected) if actual[i*4:i*4+4]!=w(n)),None)
binary=root/'build/xbox/main.exe';x=load(binary)
entry=int(re.search(r'_rf_player_movement_region_find\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for data,want in zip(commands,expected):
 x.mem_write(base,data);x.mem_write(output,w(123));x.mem_write(stack,w(stop,base+16,struct.unpack('<I',data[:4])[0],base+4,output))
 x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(output,4))==w(want) and bytes(x.mem_read(base,208))==data
 assert x.reg_read(UC_X86_REG_FPCW)==0x27f
# Invalid geometry must leave the result sentinel untouched.
guards=0
for offset,value in ((4,float('nan')),(16+52,-1),(16+16,float('inf'))):
 data=bytearray(commands[0]);data[offset:offset+4]=f(value)
 x.mem_write(base,bytes(data));x.mem_write(output,w(123));x.mem_write(stack,w(stop,base+16,3,base+4,output))
 x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(output,4))==w(123)
 guards+=1
report=dict(result='PASS',cases=len(expected),invalid_guards=guards,matches={str(i):expected.count(i) for i in (0,1,2,0xffffffff)},original_sha256=sha,
 pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
 scope='Complete original 45cca0 and all unchanged callees including 507a50; preinitialized static vector constructors. Ordered region queries with 0..3 boxes, translated/rotated boxes, inclusive boundaries and adjacent float points. PC/NXDK exact, no allocation. Does not load regions from levels or implement climbing transitions.')
(root/'artifacts/player-regions-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
