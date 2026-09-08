"""Verify logical animation state requests against unmodified RF.exe x86."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
entity,stack,stop=0x30000000,0x30100000,0x30200000
for address in (entity,stack,stop):u.mem_map(address,65536)
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
def call(address,requested,duration):
    # cdecl entity, requested, duration
    put(stack+64000,'<IIif',stop,entity,requested,duration)
    u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(address,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
f32=lambda v:struct.unpack('<f',struct.pack('<f',v))[0]
rng=random.Random(0x42a580);cases=[]
for k in range(10000):
    duration=rng.choice([0,.25,1,f32(2**rng.randrange(-20,20)*rng.uniform(.5,1))])
    elapsed=f32(duration*rng.choice([0,.25,.49999997,.5,.50000006,.75,1,rng.random()]))
    current=rng.randrange(23);nxt=rng.randrange(23) if duration else -1
    controller=struct.pack('<iiffiI',current,nxt,duration,elapsed,rng.randrange(23),k%2)
    mapping=[rng.choice([-1]+list(range(32))) for _ in range(23)]
    requested=rng.choice([-2147483648,-1,0,current,nxt,22,23,2147483647,rng.randrange(23)])
    new_duration=rng.choice([0,.25,1,f32(2**rng.randrange(-20,20)*rng.uniform(.5,1))])
    cases.append((controller,mapping,requested,new_duration))
wire=b''.join(c+struct.pack('<24if',*m,r,d) for c,m,r,d in cases)
out=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--request-state'],input=wire,capture_output=True,check=True).stdout
assert len(out)==32*len(cases)
for k,(c,m,r,d) in enumerate(cases):
    u.mem_write(entity+0x138c,c[:16]);u.mem_write(entity+0x1384,c[16:20])
    for i,mid in enumerate(m):put(entity+0x8e4+16*i,'<i',mid)
    call(0x42a650,r,d);matched=u.reg_read(UC_X86_REG_EAX)&255
    call(0x42a580,r,d)
    expected=struct.pack('<ii',0,matched)+bytes(u.mem_read(entity+0x138c,16))+c[16:]
    actual=out[k*32:(k+1)*32]
    assert actual==expected,(k,c.hex(),r,d,actual.hex(),expected.hex())
invalid=[]
for c,d in [(struct.pack('<iiffiI',0,1,1,2,0,0),.25),
            (struct.pack('<iiffiI',0,-1,1,0,0,0),.25),
            (struct.pack('<iiffiI',0,-1,0,0,0,0),-1),
            (struct.pack('<iiffiI',0,-1,0,0,0,0),float('nan'))]:
    invalid.append((c,d))
wire=b''.join(c+struct.pack('<24if',*range(23),2,d) for c,d in invalid)
out=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--request-state'],input=wire,capture_output=True,check=True).stdout
assert len(out)==32*len(invalid)
for i,(c,d) in enumerate(invalid):
    assert struct.unpack_from('<i',out,i*32)[0]!=0
    assert out[i*32+8:(i+1)*32]==c
report=dict(result='PASS',cases=len(cases),rejection_cases=len(invalid),scope='Complete original 0x42a580 state request and 0x42a650 membership predicate; finite forward transitions, all controller fields')
(root/'artifacts/motion-request-verification.json').write_text(json.dumps(report,indent=2));print(report)
