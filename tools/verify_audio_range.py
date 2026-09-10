"""Execute original sound far cutoff arithmetic."""
import hashlib,itertools,json,struct,sys,subprocess,re,random,math
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(original));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;m.mem_map(base,65536)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
u=lambda *v:struct.pack('<'+'I'*len(v),*v)
f32=lambda v:struct.unpack('<f',f(v))[0]
xp=pefile.PE(str(root/'build/xbox/main.exe'));xd=xp.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(xp.OPTIONAL_HEADER.ImageBase,(len(xd)+4095)//4096*4096);x.mem_write(xp.OPTIONAL_HEADER.ImageBase,xd);x.mem_map(base,65536)
entry=int(re.search(r'_rf_audio_far_distance\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
# Store x87 return to binary32, as original registration does at 543580.
trampoline=base+0xf100
for machine in (m,x):machine.mem_write(trampoline,b'\xd9\x1d'+u(base+64)+b'\xe9'+struct.pack('<i',stop-(trampoline+11)))
rng=random.Random(0x544960)
inputs=list(itertools.product([.01,1.,5.,10.,100.],[.1,.5,1.,2.,8.],[0.,.01,.05,.5,1.]))
inputs += [(10**rng.uniform(-2,3),10**rng.uniform(-2,1),rng.random()) for _ in range(4096)]
commands=bytearray();expected=bytearray()
for n,values in enumerate(inputs):
 wire=f(*values);commands.extend(wire)
 for machine,address in [(m,0x544960),(x,entry)]:
  machine.mem_write(stack,u(trampoline)+wire);machine.reg_write(UC_X86_REG_ESP,stack);machine.reg_write(UC_X86_REG_FPCW,0x27f)
  machine.emu_start(address,stop,count=1000);assert machine.reg_read(UC_X86_REG_EIP)==stop
 got=bytes(m.mem_read(base+64,4));actual=bytes(x.mem_read(base+64,4))
 assert got==actual,('NXDK',n,values,got.hex(),actual.hex())
 expected.extend(got)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--range'],input=commands)
assert actual==expected,'PC cutoff mismatch'
report=dict(result='PASS',cases=len(inputs),original_sha256=digest,x87_control='0x27f',
 output_sha256=hashlib.sha256(expected).hexdigest(),
 scope='Original 544960 unchanged; exact binary32 return versus shared PC/NXDK code. Positive near/rolloff, nonnegative volume including zero. Registration normalization and device gain excluded.')
(root/'artifacts/audio-range-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
