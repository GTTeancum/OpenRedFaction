"""Verify movement selector tail with unmodified predicate and request callees."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
entity,stack,stop=0x30000000,0x30100000,0x30200000
for address in (entity,stack,stop):u.mem_map(address,65536)
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
rng=random.Random(0x41f7c1);cases=[]
for k in range(6000):
    duration=rng.choice([0,.25,1]);age=duration*rng.choice([0,.25,.5,.75,1])
    c=struct.pack('<iiffiI',rng.randrange(23),rng.randrange(23) if duration else -1,duration,age,rng.randrange(23),k%2)
    mapping=[rng.choice([-1]+list(range(32))) for _ in range(23)]
    vector=rng.choice([(0,0,0),(-0.0,0,-0.0),(1,0,0),(0,-1,0),(0,0,1),(1e-30,0,0)])
    move=struct.pack('<3f5i',*vector,rng.randrange(-1,18),rng.randrange(-1,4),rng.randrange(23),rng.randrange(23),rng.randrange(23))
    cases.append((c,mapping,move))
wire=b''.join(c+struct.pack('<23i',*m)+v for c,m,v in cases)
out=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--movement'],input=wire,capture_output=True,check=True).stdout
assert len(out)==28*len(cases)
for k,(c,m,v) in enumerate(cases):
    u.mem_write(entity+0x138c,c[:16]);put(entity+0x858,'<I',entity+0x4000)
    mode,direction,idle,moving,alternate=struct.unpack_from('<5i',v,12)
    put(entity+0x4004,'<i',mode);put(entity+0x8c4,'<i',direction);u.mem_write(entity+0x714,v[:12])
    for i,mid in enumerate(m):put(entity+0x8e4+16*i,'<i',mid)
    # Three saved registers, four local words, return and reused argument.
    put(stack+64000,'<9I',0,0,0,moving,0,0,0,stop,alternate)
    u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_ESI,entity);u.reg_write(UC_X86_REG_EBX,idle);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x41f7c1,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected=struct.pack('<i',0)+bytes(u.mem_read(entity+0x138c,16))+c[16:]
    actual=out[k*28:(k+1)*28]
    assert actual==expected,(k,struct.unpack('<3f5i',v),actual.hex(),expected.hex())
c,m,v=cases[0]
invalid=[struct.pack('<3f5i',float('nan'),0,0,1,0,0,2,4),struct.pack('<3f5i',0,0,0,1,0,23,2,4)]
wire=b''.join(c+struct.pack('<23i',*m)+v for v in invalid)
out=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--movement'],input=wire,capture_output=True,check=True).stdout
assert len(out)==28*len(invalid)
for i in range(len(invalid)):
    assert struct.unpack_from('<i',out,i*28)[0]!=0
    assert out[i*28+4:(i+1)*28]==c
report=dict(result='PASS',cases=len(cases),rejection_cases=len(invalid),scope='Original 0x41f7c1..0x41f94f and unmodified movement predicates, vector comparisons, membership and request callees; upstream priorities, candidate selection and physics excluded')
(root/'artifacts/motion-movement-verification.json').write_text(json.dumps(report,indent=2));print(report)
