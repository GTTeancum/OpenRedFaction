"""Original49d7e0 early routes and damped mutation; real helpers, observe boundaries only."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
pack=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
floats=lambda v:struct.pack('<'+'f'*len(v),*v)
base=0x30000000;stack=base+0xe000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(base,0x10000);return u
u=load(exe);x=load(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_physics_contact_select\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
routes={0x49ddef:0,0x49dcf6:1,0x49db7d:2,0x49d94c:3,0x49da18:4,0x49d907:5}
def observe(cpu,address,size,data):
 if address in routes:cpu.emu_stop()
u.hook_add(UC_HOOK_CODE,observe)
rng=random.Random(0x49d844);commands=[];expected=[];counts=[0]*6
for case in range(8192):
 state=bytearray(rng.getrandbits(8) for _ in range(308));mode=rng.choice((0,1,1,1,3,8));present=int(case%7!=0)
 velocity=[rng.randrange(-10000,10001)/32 for _ in range(3)]
 contact=[rng.randrange(-64,65)/32 for _ in range(3)];support=[rng.randrange(-64,65)/32 for _ in range(3)]
 if case%5==0:contact=[0,-.25,0];support=[0,0,0]
 if case%11==0:contact=[0,-.31622776,0];support=[0,0,0]
 inv=rng.choice((0.,0.,-.5,.25));radius=rng.choice((0.,1.,1.0000001,2.,5.));ny=rng.choice((-.5,-.5000001,-1.,0.,1.))
 f964=rng.choice((-1,-1,0,5));f974=rng.choice((-1,-1,0,5));flags=rng.getrandbits(32)
 word=(0 if case%8 else rng.choice((1,256,0xffffffff)))
 state[184:196]=floats(velocity);state[284:288]=floats([ny]);state[300:304]=pack(word)
 context=pack(mode,present)+floats([radius,inv]+contact+support)+pack(f964,f974,flags);assert len(context)==52
 command=bytes(state)+context
 actor=bytearray(rng.getrandbits(8) for _ in range(0xa00))
 for off,data in ((0x144,state[184:196]),(0x1a8,state[272:280]),(0x1c4,floats([ny])),(0x1ec,pack(word)),(0x1d4,floats([inv])),(0x1d8,floats(contact)),(0x8a0,floats(support)),(0x964,pack(f964)),(0x974,pack(f974)),(0x810,pack(flags)),(0x858,pack(base+0x3000)),(0x1e4,pack(0x10000 if present else 0x20000))):actor[off:off+len(data)]=data
 u.mem_write(base,bytes(actor));u.mem_write(base+0x2000,bytes(0x300));u.mem_write(base+0x202c,pack(0x10000));u.mem_write(base+0x2180,floats([radius]));u.mem_write(base+0x3004,pack(mode));u.mem_write(0x7394cc,pack(base+0x2000))
 u.mem_write(stack,pack(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x49d7e0,stop,count=100000)
 boundary=u.reg_read(UC_X86_REG_EIP);assert boundary in routes,hex(boundary);route=routes[boundary];counts[route]+=1
 out=bytes(u.mem_read(base,len(actor)));impact=bytes(u.mem_read(u.reg_read(UC_X86_REG_ESP)+0x10,4))
 want=bytearray(state)
 if route==0:
  for dst,src,size in ((184,0x144,12),(276,0x1ac,4),(300,0x1ec,4)):
   want[dst:dst+size]=out[src:src+size];actor[src:src+size]=out[src:src+size]
 assert out==actor,('unrelated original mutation',case)
 result=bytes(want)+pack(route)+impact
 x.mem_write(base,b'\xa5'*16+command+b'\x5a'*16);x.mem_write(base+0x2000,pack(0x12345678,0x12345678))
 x.mem_write(stack,pack(stop,base+16,base+16+308,base+0x2000,base+0x2004));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0,('return',case)
 got=bytes(x.mem_read(base+16,308))+bytes(x.mem_read(base+0x2000,8));assert got==result,('NXDK',case,got[308:].hex(),result[308:].hex())
 assert bytes(x.mem_read(base,16))==b'\xa5'*16 and bytes(x.mem_read(base+376,16))==b'\x5a'*16
 assert bytes(x.mem_read(base+324,52))==context
 commands.append(command);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--contact-select'],input=b''.join(commands));assert pc==b''.join(expected),'PC mismatch'
assert all(counts),counts
guards=0
valid=bytearray(commands[0]);valid[300:304]=pack(0);valid[284:288]=floats([-1]);valid[308:348]=pack(1,1)+floats([2,0,0,-1,0,0,0,0])
for offset in (184,320,316,284,328,324,332,336,340,344):
 bad=bytearray(valid)
 if offset==184:bad[300:304]=pack(1)
 bad[offset:offset+4]=pack(0x7fc00000)
 x.mem_write(base,bytes(bad));x.mem_write(base+0x2000,pack(0xa5a5a5a5,0xa5a5a5a5))
 x.mem_write(stack,pack(stop,base,base+308,base+0x2000,base+0x2004));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0xfffffffc,('guard',offset)
 assert bytes(x.mem_read(base,360))==bad and bytes(x.mem_read(base+0x2000,8))==pack(0xa5a5a5a5,0xa5a5a5a5)
 guards+=1
report=dict(result='PASS',cases=len(commands),nonfinite_guards=guards,routes=counts,original_sha256=sha,scope='Original49d7e0 entry to damped/dynamic/static/flag80/crush/stance boundaries. Actual handle lookup, vector damping and relative length,428010 and40a130. Hooks only stop at route boundaries; no helper substitution. Exact PC/NXDK complete body, route and impact plus original unrelated bytes and NXDK guards/context. Subsequent crush/stance effects, velocity response and damage dispatch excluded.')
(root/'artifacts/contact-select-verification.json').write_text(json.dumps(report,indent=2));print(report)
