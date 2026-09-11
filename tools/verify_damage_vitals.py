"""Original SP41a350 numeric prefix, including unmodified41a7c0, vs PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda v:struct.pack('<f',v)
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);return m
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(original);x=machine(root/'build/xbox/main.exe');u.mem_write(0x64ecb9,bytes(2))
entry=int(re.search(r'_rf_entity_damage_vitals_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x41a350);cases=[];expected=[]
for i in range(8192):
    health=rng.uniform(-10,1000);armor=rng.choice([0,0,-1,rng.uniform(0,300)])
    amount=rng.uniform(-20,1000);kind=(-1,0,1,2,3,4,5,6,7,8,9,10)[i%12];mult=rng.choice([0,.125,.52,1,1.5,3])
    if i<64:
        health=[0,.5,struct.unpack('<f',w(0x3f000001))[0],struct.unpack('<f',w(0x3effffff))[0],100][i%5]
        armor=[0,1,52,100][i//5%4];amount=[0,1,100,200][i//7%4];mult=1
    wire=f(health)+f(armor)+w(0x12345678)+f(amount)+w(kind)+f(mult)+w(0x87654321);cases.append(wire)
    state=bytearray(0x1500);state[0x34:0x3c]=wire[:8];state[0x294:0x298]=w(b+0x4000);state[0x73c:0x740]=wire[8:12]
    u.mem_write(b,bytes(state));u.mem_write(b+0x4000+0x13e8+max(kind,0)*4,wire[20:24]);u.mem_write(0x6460f0,wire[24:28])
    u.mem_write(stack,w(stop,b)+wire[12:16]+w(-1,kind,-1));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
    u.emu_start(0x41a350,0x41a44d,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x41a44d
    want=w(0)+bytes(u.mem_read(b+0x34,8))+bytes(u.mem_read(b+0x73c,4))+bytes(u.mem_read(stack+8,4));expected.append(want)
    # No unverified later kill-attribution or effects are executed.
for at in (0,4,12,20):
    wire=bytearray(cases[1]);wire[at:at+4]=w(0x7fc00000);cases.append(bytes(wire));expected.append(w(-2)+wire[:12]+bytes([0xa5])*4)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--damage-vitals'],input=b''.join(cases))
assert len(actual)==len(cases)*20
for i,(wire,want) in enumerate(zip(cases,expected)):
    assert actual[i*20:(i+1)*20]==want,('PC',i,wire.hex(),actual[i*20:(i+1)*20].hex(),want.hex())
    x.mem_write(b,wire[:12]);x.mem_write(b+0x100,bytes([0xa5])*4)
    x.mem_write(stack,w(stop,b)+wire[12:28]+w(b+0x100));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,12))+bytes(x.mem_read(b+0x100,4))
    assert got==want,('NXDK',i,wire.hex(),got.hex(),want.hex())
report=dict(result='PASS',original_cases=8192,port_guards=4,pc_nxdk_cases=len(cases),original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='SP41a350 entry through41a44d, unmodified armor helper41a7c0, class multiplier supplied in original memory. Exact health/armor/time/scaled amount on PC/NXDK under53-bit x87. Signed finite numeric inputs and half-health boundary. Excludes wrapper eligibility, kill attribution, side effects, entity lifecycle and multiplayer.')
(root/'artifacts/damage-vitals.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
