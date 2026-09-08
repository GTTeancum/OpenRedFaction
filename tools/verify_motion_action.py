"""Verify original entity action predicate and loaded remaining-time query."""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_FPCW,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,motions,data,stack=[0x30000000+i*0x100000 for i in range(5)]
for a in (obj,desc,motions,data,stack): u.mem_map(a,65536)
def put(a,fmt,*v): u.mem_write(a,struct.pack(fmt,*v))
def read(a,n): return bytes(u.mem_read(a,n))
rng=random.Random(0x428d10);cases=[]
for k in range(5000):
    count=k%17;ids=rng.sample(range(32),count)
    slots=b''.join(struct.pack('<iif',ids[i] if i<count else 0,rng.choice([-160,0,1,9599,9600,9601,20000]),rng.choice([0,.5,1])) for i in range(16))
    state=struct.pack('<I',count)+slots+struct.pack('<3i',-1,-1,-1)+struct.pack('<4I6f',k%2,0,0,0,0,0,0,0,0,0)+struct.pack('<fII',.25,1,0)
    resources=[struct.pack('<f4iI3i',1,0,9600,0,0,rng.choice([0,1,2]),0,0,1) for i in range(32)]
    actions=[rng.choice([-1]+list(range(32))) for i in range(45)]
    action=rng.choice([-2147483648,-1,0,44,45,2147483647,rng.randrange(45)])
    cases.append((state,resources,actions,action))
for k in range(2000):
    s,r,a,q=cases[1]
    s=struct.pack('<I',1)+struct.pack('<iif',0,0,0)+s[16:]
    r=list(r);r[0]=struct.pack('<f4iI3i',1,0,rng.randrange(1,2147483648),0,0,1,0,0,1)
    cases.append((s,r,[0]*45,0))
wire=b''.join(s+b''.join(r)+struct.pack('<46i',*a,q) for s,r,a,q in cases)
out=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--action-active'],input=wire,capture_output=True,check=True).stdout
assert len(out)==12*len(cases)
entity=obj+0x6000;wrapper=obj+0x4000;stop=stack+65000;result=stack+65100
# Store the returned x87 value only in the test's return trampoline.
u.mem_write(stop,b'\xd9\x1d'+struct.pack('<I',result))
for k,(s,resources,actions,action) in enumerate(cases):
    u.mem_write(obj,bytes(65536));put(obj+0x1d50,'<I',desc);u.mem_write(obj+0x12d0,s[:196]);u.mem_write(obj+0x1d4c,s[208:209])
    put(wrapper,'<II',2,obj);put(entity+0x80,'<I',wrapper)
    for i,r in enumerate(resources):
        m=motions+i*256;d=data+i*256;put(desc+0xf5c+i*4,'<I',m);put(m+0x78,'<I',d);u.mem_write(d+16,r[4:12])
        u.mem_write(desc+0x120c+i,r[20:21])
    for i,mid in enumerate(actions):put(entity+0xa54+i*16,'<i',mid)
    before=read(obj,65536)
    put(stack+64000,'<IIi',stop,entity,action);u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x428d10,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    active=u.reg_read(UC_X86_REG_EAX)&255;seconds=bytes(4)
    if 0<=action<45 and actions[action]>=0:
        put(stack+64000,'<IIi',stop,wrapper,actions[action]);u.reg_write(UC_X86_REG_ESP,stack+64000)
        u.emu_start(0x5033d0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
        u.emu_start(stop,stop+6,count=1);seconds=read(result,4)
    assert read(obj,65536)==before,(k,'query mutated entity/model')
    assert out[k*12:(k+1)*12]==struct.pack('<ii',0,active)+seconds,(k,'query result')
report=dict(result='PASS',cases=len(cases),scope='Original 0x428d10 and 0x5033d0 type-two path through 0x51c270, with unmodified callees; zero weights, frozen/loop states, cursor boundaries and invalid action indices')
(root/'artifacts/motion-action-verification.json').write_text(json.dumps(report,indent=2));print(report)
