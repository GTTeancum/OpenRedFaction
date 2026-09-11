"""Original rotation ramp state and stored scale vs PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
    p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(origin,(len(b)+4095)//4096*4096);m.mem_write(origin,b);m.mem_map(base,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u,x=machine(exe),machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_group_rotation_ramp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
offsets=[0x318,0x2e8,0x2f8,0x2fc,0x300,0x30c,0x308]
rng=random.Random(0x46a4d3);commands=bytearray();expected=bytearray();key=base+0x1000
u.mem_write(base+0x6000,b'\xd9\x1d'+w(base+0x6100));u.mem_write(base+0x6200,b'\xdb\xe3')
for case in range(1025):
 flags=(rng.getrandbits(32)&~0x30)|((case%4)<<4)
 elapsed=(case%9)*.125;dt=(1/60,.125,.5)[case%3]
 acceleration=(0.,.125,.25,1.)[(case//4)%4];deceleration=(0.,.125,.5,2.)[(case//16)%4]
 if case==1024:elapsed=dt=acceleration=deceleration=0.;flags=(flags&~0x30)|0x20
 data=w(flags,case%6,0,1)+f(.25)+w(0xffffffff)+f(elapsed,acceleration,deceleration,dt)
 raw=bytearray(rng.randbytes(1024))
 for i,o in enumerate(offsets):raw[o:o+4]=data[4*i:4*i+4]
 u.mem_write(base,bytes(raw));u.mem_write(key+0x40,f(acceleration,deceleration));u.mem_write(0x5a4014,f(dt))
 u.emu_start(base+0x6200,base+0x6202,count=10);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBP,key);u.reg_write(UC_X86_REG_EBX,0);u.reg_write(UC_X86_REG_ECX,flags);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x46a4d3,0x46a55f,count=1000);assert u.reg_read(UC_X86_REG_EIP)==0x46a55f
 u.emu_start(base+0x6000,base+0x6006,count=10)
 after=bytes(u.mem_read(base,1024));result=b''.join(after[o:o+4] for o in offsets)+bytes(u.mem_read(base+0x6100,4))
 for o in (0x318,0x2fc,0x308):raw[o:o+4]=after[o:o+4]
 assert raw==after
 commands.extend(data);expected.extend(w(0)+result)
 x.mem_write(base,data);x.mem_write(base+0x1000,f(123));x.mem_write(stack,w(stop,base,base+24)+f(acceleration,deceleration,dt)+w(base+0x1000));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(base,28))+bytes(x.mem_read(base+0x1000,4))==result,case
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-ramp'],input=commands);assert actual==expected
report=dict(result='PASS',cases=1025,original_sha256=digest,scope='Prepared original46a4d3..46a55f with explicit53-bit x87 precision. Float view of ramp factor and all motion/elapsed fields exact PC/NXDK. Actual acceleration/deceleration arithmetic, flag precedence, zero durations, including zero-time unordered deceleration clamp, clamp/stop/reset. No timer gate, phase advance, angle integration or full rotating controller.')
(root/'artifacts/group-rotation-ramp.json').write_text(json.dumps(report,indent=2));print(report)
