"""Original device-volume initialization/lookup, without sound-device calls."""
import hashlib,json,struct,sys,math,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
path=root/'Installed_Game/RF.exe';digest=hashlib.sha256(path.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(path));b=p.OPTIONAL_HEADER.ImageBase;data=p.get_memory_mapped_image()
m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(b,(len(data)+4095)//4096*4096);m.mem_write(b,data)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;m.mem_map(base,65536)
u=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda v:struct.pack('<f',v)
f32=lambda v:struct.unpack('<f',f(v))[0]
def call(address,args=b''):
 m.mem_write(stack,u(stop)+args);m.reg_write(UC_X86_REG_ESP,stack);m.reg_write(UC_X86_REG_FPCW,0x27f)
 m.emu_start(address,stop,count=100000);assert m.reg_read(UC_X86_REG_EIP)==stop
 return m.reg_read(UC_X86_REG_EAX)
call(0x521680)
tables=[list(struct.unpack('<101i',m.mem_read(a,404))) for a in [0x1ad7388,0x18871f0]]
constants={hex(a):struct.unpack(fmt,p.get_data(a-b,struct.calcsize(fmt)))[0] for a,fmt in [(0x5897b0,'<f'),(0x589e30,'<f'),(0x589e28,'<d'),(0x589e20,'<d'),(0x5894b8,'<d')]}
linear=[math.trunc(.5-(1-i*constants['0x5897b0'])*constants['0x589e30']) for i in range(101)]
logarithmic=[-10000]+[math.trunc(math.log(i*constants['0x5897b0'])/math.log(constants['0x589e28'])*constants['0x589e20']+constants['0x5894b8']) for i in range(1,101)]
assert tables==[logarithmic,linear]
rng=random.Random(0x522420);values=[f32((i+.5)/100+d) for i in range(100) for d in [-1e-7,0,1e-7]]+[f32(rng.uniform(-1,2)) for _ in range(4096)]
for first,second in [(0,0),(0,1),(1,0),(1,1)]:
 m.mem_write(0x1aed340,bytes([first]));m.mem_write(0x1aed360,bytes([second]))
 for v in values:
  index=max(0,min(100,math.trunc(v*100+.5)));want=tables[first and second][index]&0xffffffff
  assert call(0x522420,f(v))==want,(first,second,v,index)
report=dict(result='PASS',original_sha256=digest,table_entries=202,lookup_cases=len(values)*4,constants=constants,
 scope='Original 521680 and 522420 plus original ftol execute unchanged under x87 0x027f; exact independent table formulas and both selection flags. No device/host output.',tables=tables)
(root/'artifacts/audio-device-volume.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='tables'})
