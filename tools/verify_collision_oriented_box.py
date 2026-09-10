"""Original oriented segment/box including transforms and miss writes."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data)
 m.mem_map(base,65536);m.reg_write(UC_X86_REG_FPCW,0x37f);return m
u=machine(original);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_collision_segment_oriented_box\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
rng=random.Random(0x508660);commands=bytearray();expected=bytearray();hits=guards=changed_misses=0
# Constructors only register empty exit handlers; use the already initialized
# original scratch state so every geometric instruction and callee runs unchanged.
u.mem_write(0x1754524,w(15))
for n in range(4096):
 scale=2.0**rng.randrange(-15,16);center=[rng.uniform(-10,10)*scale for _ in range(3)]
 angle=rng.uniform(-math.pi,math.pi);c=math.cos(angle);s=math.sin(angle)
 matrix=[c,0,s,0,1,0,-s,0,c]
 if n%5==0:matrix=[1,0,0,0,1,0,0,0,1]
 if n%7==0:matrix=[rng.uniform(-2,2) for _ in range(9)]
 size=[rng.uniform(0,10)*scale for _ in range(3)]
 start=[a+rng.uniform(-15,15)*scale for a in center];end=[a+rng.uniform(-15,15)*scale for a in center]
 if n%3==0:start=center[:]
 if n%11==0:end=start[:]
 if n%13==0:end=center[:]
 if n%17==0:size[n%3]=0
 if n%19==0:
  center=[0,0,0];matrix=[1,0,0,0,1,0,0,0,1];size=[2*scale]*3
  start=[scale,0,0];end=[scale*2,0,0]
 if n%101==0:size[1]=-1
 if n%103==0:matrix[3]=math.nan
 wire=struct.pack('<24f',*center,*matrix,*size,*start,*end,123,456,789);commands.extend(wire)
 if n%101==0 or n%103==0:want=w(-2,0xa5a5a5a5)+wire[84:];guards+=1
 else:
  u.mem_write(base,wire);u.mem_write(stack,w(stop,base,base+12,base+48,base+60,base+72,base+84));u.reg_write(UC_X86_REG_ESP,stack)
  u.emu_start(0x508660,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
  hit=u.reg_read(UC_X86_REG_EAX)&255;assert hit in (0,1);hits+=hit
  point=bytes(u.mem_read(base+84,12));changed_misses+=not hit and point!=wire[84:];want=w(0,hit)+point
  assert bytes(u.mem_read(base,84))==wire[:84]
 expected.extend(want);x.mem_write(base,wire);x.mem_write(base+0x100,w(0xa5a5a5a5));x.mem_write(stack,w(stop,base,base+12,base+48,base+60,base+72,base+84,base+0x100));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x100,4))+bytes(x.mem_read(base+84,12))
 assert got==want,('NXDK',n,got.hex(),want.hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--oriented-box'],input=commands)
assert len(actual)==len(expected)
for n in range(4096):assert actual[n*20:(n+1)*20]==expected[n*20:(n+1)*20],('PC',n,actual[n*20:(n+1)*20].hex(),expected[n*20:(n+1)*20].hex())
report=dict(result='PASS',cases=4096,original_cases=4096-guards,port_guards=guards,hits=hits,misses_writing_point=changed_misses,original_sha256=digest,scope='Full original 508660 with initialized static scratch and unchanged geometric callees. Explicit 64-bit nearest x87, exact PC/NXDK hit and output bytes including misses. Rotation, general matrices, degenerate dimensions, zero-length segments, boundaries and scales 2^-15..2^15. Nonfinite/negative-dimension guards preserve output. Directional trigger branch and live actor integration excluded.')
(root/'artifacts/collision-oriented-box-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
