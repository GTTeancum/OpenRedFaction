"""Original506ae0 segment/sphere helper versus PC/NXDK; all geometry callees real."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_collision_segment_sphere\s+([0-9a-fA-F]+)',mapping)[1],16)
u.mem_write(0x17543e0,w(15)) # Static vector constructors/empty atexit callbacks already initialized.
def run(m,address,args,payload):
 m.mem_write(b,payload);m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,stop,count=20000)
 assert m.reg_read(UC_X86_REG_EIP)==stop and m.reg_read(UC_X86_REG_FPCW)==0x27f
 assert bytes(m.mem_read(b,40))==payload[:40]
 return w(m.reg_read(UC_X86_REG_EAX)&255)+bytes(m.mem_read(b+40,12))
rng=random.Random(0x506ae0);commands=[];answers=[];counts=[0,0];miss_writes=0;start_hits=0
for case in range(8192):
 start=[rng.randrange(-32,33)/4 for _ in range(3)];end=[v+rng.randrange(-32,33)/4 for v in start];center=[rng.randrange(-48,49)/4 for _ in range(3)];radius=rng.choice((0,.125,.5,1,2,4,8))
 if case%13==0:end=start.copy()
 if case<1024:
  start=[0,0,0];end=[0,0,(0,.5,1,2,4,8,16,32)[case%8]];radius=(0,.5,1,2)[case//8%4]
  center=[(0,.5,1,2)[case//32%4],0,(-2,-1,0,.25,.5,2,4,8)[case//128]]
 if 1024<=case<1536:
  start=[0,0,0];end=[0,0,2];radius=1;center=[struct.unpack('<f',w(0x3f800000+(case%3)-1))[0],0,(0,1,2)[case//3%3]]
 if 1536<=case<2048:
  start=[16777216,0,16777216];end=[16777218,1,16777214];radius=2;center=[16777216,(case%9-4)/4,16777216]
 payload=f(start+end+center+[radius,91,92,93]);bits=struct.unpack('<13I',payload);commands.append(payload)
 expected=run(u,0x506ae0,[b+40,b,b+12,b+24,bits[9]],payload)
 actual=run(x,entry,[b,b+12,b+24,bits[9],b+40],payload)
 assert actual==expected,(case,payload.hex(),expected.hex(),actual.hex())
 result=struct.unpack('<I',expected[:4])[0];counts[result]+=1;miss_writes+=not result and expected[4:]!=payload[40:];start_hits+=result and expected[4:]==payload[:12];answers.append(expected)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--segment-sphere'],input=b''.join(commands))
for i,expected in enumerate(answers):assert actual[i*16:(i+1)*16]==expected,('PC',i,commands[i].hex(),expected.hex(),actual[i*16:(i+1)*16].hex())
assert all(counts) and miss_writes and start_hits
report=dict(result='PASS',cases=len(commands),misses=counts[0],hits=counts[1],misses_writing_start=miss_writes,hits_returning_start=start_hits,original_sha256=sha,scope='Full original506ae0 geometry and vector/distance callees, no hooks; static scratch constructors preinitialized. Exact PC/NXDK result/point and input preservation under027f. Zero-length, inside/outside, endpoint extension, strict tangency, adjacent floats and large-coordinate rounding. Finite disjoint inputs. Static scratch side effects and model response49afe0 excluded.')
(root/'artifacts/collision-segment-sphere.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
