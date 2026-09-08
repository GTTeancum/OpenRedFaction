"""Verify wrapping game clocks and deadline operations against original RF.exe."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
stack,obj,stop=0x30000000,0x30100000,0x30200000
for a in (stack,obj,stop):u.mem_map(a,65536)
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
def rd(a,n):return bytes(u.mem_read(a,n))
P=0x3ff1a100;assert P==1072800000
rng=random.Random(0x4fa360);cases=[]
boundaries=[0,1,P//2-1,P//2,P//2+1,P-1,P]
for op in range(7):
    for k in range(1400):
        now=rng.choice(boundaries+[rng.randrange(P+1)]);real=rng.choice(boundaries+[rng.randrange(P+1)])
        pause=rng.randrange(1 if op==2 else 0,5)
        deadline=rng.choice([-2147483648,-1]+boundaries+[rng.randrange(P+1)])
        value=rng.choice([-P,-1200,-1]+boundaries) if op==3 else rng.choice(boundaries+[rng.randrange(P+1)])
        cases.append((now,real,pause,deadline,op,value))
wire=b''.join(struct.pack('<6i',*c) for c in cases)
out=subprocess.run([str(root/'build/pc/Release/rf_timer_probe.exe')],input=wire,capture_output=True,check=True).stdout
assert len(out)==24*len(cases)
addresses=[0x4fa2d0,0x4fa320,0x4fa330,0x4fa360,0x4fa3f0,0x4fa420,0x4fa3e0]
for k,(now,real,pause,deadline,op,value) in enumerate(cases):
    put(0x5a3ed8,'<ii',now,real);put(0x173c36c,'<i',pause);put(obj,'<i',deadline)
    put(stack+64000,'<Ii',stop,value);u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_ECX,obj)
    u.emu_start(addresses[op],stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    result=123
    if op==4:result=u.reg_read(UC_X86_REG_EAX)&255
    elif op==5:result=struct.unpack('<i',struct.pack('<I',u.reg_read(UC_X86_REG_EAX)))[0]
    expected=struct.pack('<i',0)+rd(0x5a3ed8,8)+rd(0x173c36c,4)+rd(obj,4)+struct.pack('<i',result)
    assert out[k*24:(k+1)*24]==expected,(k,cases[k],out[k*24:(k+1)*24].hex(),expected.hex())
invalid=[(0,0,0,42,0,-1),(0,0,0,42,2,0),(0,0,2147483647,42,1,0),(0,0,0,42,3,P+1),(0,0,0,P+1,4,0),(P+1,0,0,42,5,0)]
out=subprocess.run([str(root/'build/pc/Release/rf_timer_probe.exe')],input=b''.join(struct.pack('<6i',*c) for c in invalid),capture_output=True,check=True).stdout
assert len(out)==24*len(invalid)
for i,(now,real,pause,deadline,op,value) in enumerate(invalid):
    assert struct.unpack_from('<i',out,i*24)[0]!=0
    assert out[i*24+4:(i+1)*24]==struct.pack('<5i',now,real,pause,deadline,123)
report=dict(result='PASS',original_cases=len(cases),rejection_cases=len(invalid),scope='Complete original clock advance, pause/resume, deadline set/clear, expired and remaining queries; bounded valid clock domain and one-period offsets')
(root/'artifacts/timer-verification.json').write_text(json.dumps(report,indent=2));print(report)
