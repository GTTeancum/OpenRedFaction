"""Compare compact entity views with original lookup and combat predicates."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base,stack,links=[0x30000000+i*0x100000 for i in range(3)]
for a in (base,stack,links):u.mem_map(a,65536)
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
def call(address,arg):
    put(stack+64000,'<II',stack+65000,arg&0xffffffff)
    u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(address,stack+65000,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==stack+65000
    return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x41f950);cases=[]
for k in range(2000):
    handles=[i+(rng.randrange(3)<<16) for i in range(8)];nodes=[]
    for i in range(8):
        # Increasing occupant edges keep original recursion finite. Owner may
        # be null; the views preserve its pointer independently of registry.
        occupants=[rng.choice([-1]+handles[i+1:]) for _ in range(3)]
        nodes.append([i if rng.randrange(5) else -1,handles[i],rng.choice([0,0,0,1]),rng.randrange(6),
            rng.choice([0,0,8]),rng.choice([0,1,0x10,0x800,0x810,0x811]),rng.randrange(8),
            rng.choice([0,7,16,17]),rng.choice([-1]+handles),rng.choice([-1,-1,-1,0,-2]),rng.choice([-1,-1,-1,1]),
            rng.choice([0,-0.0,1,6]),rng.choice([-1,i,i]),*occupants])
    target=rng.choice([-1]+list(range(8)))
    query=rng.choice([-1,1024,65535,65536+1024,-65536]+handles)
    attached=[rng.choice([-1]+handles) for _ in range(8)]
    cases.append((target,query,attached,nodes))
# Index limits, generation mismatch, and valid handles with the sign bit set.
for slot,handle,query in [(1023,1023,1023),(0,-65536,-65536),(0,-2147483648,-2147483648),
                          (0,65536,0),(0,0,65536),(0,0,-1),(0,0,1024)]:
    nodes=[list(n) for n in cases[0][3]]
    nodes[0][0:4]=[slot,handle,0,0]
    cases.append((0,query,[-1]*8,nodes))
wire=b''.join(struct.pack('<10i',t,q,*a)+b''.join(struct.pack('<11if4i',*n) for n in nodes) for t,q,a,nodes in cases)
out=subprocess.run([str(root/'build/pc/Release/rf_entity_probe.exe')],input=wire,capture_output=True,check=True).stdout
assert len(out)==24*len(cases)
observed=set()
for k,(target,query,attached,nodes) in enumerate(cases):
    u.mem_write(base,bytes(65536));u.mem_write(0x7394cc,bytes(4096))
    for i,n in enumerate(nodes):
        slot,handle,kind,classification,f7c,f810,f7d0,action,linked,w0,w1,speed,owner,*occupants=n
        p=base+i*8192;info=p+0x1800
        if slot>=0:put(0x7394cc+slot*4,'<I',p)
        put(p+0x2c,'<i',handle);put(p+0x24,'<i',kind);put(p+0x294,'<I',info)
        put(info+0x1b4,'<i',classification);put(info+0x50,'<f',speed)
        put(p+0x7c,'<I',f7c);put(p+0x810,'<I',f810);put(p+0x7d0,'<I',f7d0)
        put(p+0x520,'<i',action);put(p+0x200,'<i',linked)
        put(p+0x2a0,'<Iii',base+owner*8192 if owner>=0 else 0,w0,w1)
        put(p+0x8cc,'<III',3,3,p+0x1100)
        for j,h in enumerate(occupants):put(p+0x1100+j*4,'<I',p+0x1200+j*8);put(p+0x1204+j*8,'<i',h)
    put(0x7c75cc,'<I',links)
    for i,h in enumerate(attached):put(links+i*24,'<6I',links+((i+1)%8)*24,0,0,0,0,h&0xffffffff)
    a=call(0x40a0e0,query);b=call(0x426fc0,query)
    p=base+target*8192 if target>=0 else 0
    ready=call(0x41f950,p)&255
    eligible=int(bool(ready) and not(call(0x427020,p)&255) and not(call(0x428e60,p)&255))
    has=call(0x408dc0,p+0x2a0)&255 if p else 0
    expected=(int((a-base)//8192) if a else -1,int((b-base)//8192) if b else -1,0,ready,eligible,has)
    actual=struct.unpack_from('<6i',out,k*24)
    assert actual==expected,(k,actual,expected)
    observed.add((ready,eligible,has))
# Deliberate cyclic malformed view: original would recurse without bound.
t,q,a,nodes=cases[0];nodes=[list(n) for n in nodes]
nodes[0]=[0,0,0,0,0,0,0,0,-1,-1,-1,0,0,0,-1,-1]
wire=struct.pack('<10i',0,0,*([-1]*8))+b''.join(struct.pack('<11if4i',*n) for n in nodes)
rejected=struct.unpack('<6i',subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe')],input=wire))
assert rejected[2:] == (-2,-99,-99,-2),rejected
report=dict(result='PASS',cases=len(cases),cycle_rejections=1,outcomes=sorted(observed),scope='Unmodified 40a0e0/426fc0/408dc0/41f950/427020/428e60 and callees on stable acyclic entity/seat/attachment views; C-only bounded cycle rejection; compact views exclude allocation and gameplay initialization')
(root/'artifacts/entity-predicates-verification.json').write_text(json.dumps(report,indent=2));print(report)
