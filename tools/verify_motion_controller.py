"""Verify post-selector controller and its unmodified loaded-motion callees."""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_FPCW,UC_X86_REG_ESI
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,motions,data,stack=[0x30000000+i*0x100000 for i in range(5)]
for a in (obj,desc,motions,data,stack): u.mem_map(a,65536)
def put(a,fmt,*v): u.mem_write(a,struct.pack(fmt,*v))
def read(a,n): return bytes(u.mem_read(a,n))
rng=random.Random(0x51c190);cases=[]
for k in range(2400):
    count=k%14; ids=rng.sample(range(32),count);selected=lambda:rng.choice([-1]+list(range(count)))
    slots=b''.join(struct.pack('<iif',ids[i] if i<count else 0,rng.randrange(-160,11000),rng.choice([0,.5,1])) for i in range(16))
    state=struct.pack('<I',count)+slots+struct.pack('<3i',selected(),selected(),selected())+struct.pack('<4I6f',rng.randrange(2),1,123,456,1,2,3,4,5,6)+struct.pack('<fII',.25,65535,3)
    resources=[struct.pack('<f4iI3i',1,rng.choice([-160,0,160]),9600,0,0,rng.choice([0,1,2,255]),0,0,rng.randrange(4)) for i in range(32)]
    mapping=[rng.choice([-1]+list(range(32))) for _ in range(23)]
    current=rng.randrange(23); nxt=rng.randrange(23); duration=rng.choice([0,.1,.25,1]); age=rng.choice([0,.01,.05,.1,.2,.24999999,1])
    delta=rng.choice([0,1/30,.05,.2,1e-9])
    controller=struct.pack('<iiffiI',current,nxt if duration else -1,duration,age,rng.randrange(23),k%2)
    cases.append((state,resources,controller,mapping,delta))
# The sum rounds to the duration but remains below it in the x87 register.
state,resources,_,_,_=cases[0]
for override in (0,1):
    for delta in (2**-27,2**-26,2**-25):
        cases.append((state,resources,struct.pack('<iiffiI',0,1,.25,.25-2**-26,8,override),list(range(23)),delta))
wire=b''.join(s+b''.join(r)+c+struct.pack('<23if',*m,d) for s,r,c,m,d in cases)
run=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--controller'],input=wire,capture_output=True,check=True)
assert len(run.stdout)==len(cases)*416
entity=obj+0x6000; wrapper=obj+0x4000
for k,(s,resources,controller,mapping,delta) in enumerate(cases):
    u.mem_write(obj,bytes(65536));u.mem_write(desc,bytes(65536));put(obj+0x1d50,'<I',desc);put(desc+0xf58,'<I',32)
    u.mem_write(obj+0x12d0,s[:196]);u.mem_write(obj+0x1cfc,s[196:204]);u.mem_write(obj+0x1d48,s[204:208]);u.mem_write(obj+0x1d4c,s[208:209]);u.mem_write(obj+0x1d14,s[212:213]);u.mem_write(obj+0x1d18,s[216:248]);u.mem_write(obj+0x1d04,s[248:252]);u.mem_write(obj+0x1cf8,s[252:254]);u.mem_write(obj+0x1d44,b'\x01\x01')
    for i,r in enumerate(resources):
        m=motions+i*256;d=data+i*256;put(desc+0xf5c+i*4,'<I',m);put(m+0x78,'<I',d)
        u.mem_write(desc+0x120c+i,r[20:21]);u.mem_write(m+0x74,r[32:36]);u.mem_write(d+16,r[4:12])
    put(wrapper,'<II',2,obj);put(entity+0x80,'<I',wrapper)
    u.mem_write(entity+0x138c,controller[:16]);u.mem_write(entity+0x1384,controller[16:20]);put(entity+0x810,'<I',32 if controller[20] else 0)
    for i,mid in enumerate(mapping): put(entity+0x8e4+i*16,'<i',mid)
    put(0x5a4014,'<f',delta)
    esp=stack+64000;stop=stack+65000
    # Selector argument, saved edi/esi/ebx, return, entity argument.
    put(esp,'<6I',entity,0,0,0,stop,entity)
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_ESI,entity);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x41f2b6,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected=read(obj+0x12d0,196)+read(obj+0x1cfc,8)+read(obj+0x1d48,4)+struct.pack('<II',read(obj+0x1d4c,1)[0],read(obj+0x1d14,1)[0])+read(obj+0x1d18,32)+read(obj+0x1d04,4)+struct.pack('<II',struct.unpack('<H',read(obj+0x1cf8,2))[0],read(obj+0x1d44,1)[0]|read(obj+0x1d45,1)[0]<<1)+read(entity+0x138c,16)+controller[16:]+b''.join(read(motions+i*256+0x74,4) for i in range(32))
    actual=run.stdout[k*416:(k+1)*416]
    assert struct.unpack_from('<i',actual)[0]==0,(k,'status')
    assert actual[4:]==expected,(k,'state',[i for i in range(0,len(expected),4) if actual[4+i:8+i]!=expected[i:i+4]])
# Safe rejection: first new slot fits, second does not; no partial reference increment.
s=struct.pack('<I',15)+b''.join(struct.pack('<iif',i,0,1) for i in range(16))+struct.pack('<3i',-1,-1,-1)+bytes(40)+struct.pack('<fII',0,1,0)
r=[struct.pack('<f4iI3i',1,0,9600,0,0,1,0,0,0) for _ in range(32)]
c=struct.pack('<iiffiI',0,1,1,.25,0,0);m=[15,16]+[-1]*21
bad=[(s,r,c,m,0),(s,r,c,m,-1),(s,r,c,[-2]+m[1:],0)]
wire=b''.join(s+b''.join(r)+c+struct.pack('<23if',*m,d) for s,r,c,m,d in bad)
out=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--controller'],input=wire,capture_output=True,check=True).stdout
assert len(out)==len(bad)*416
for i,(s,r,c,m,d) in enumerate(bad):
    result=out[i*416:(i+1)*416]
    assert struct.unpack_from('<i',result)[0]!=0
    assert result[4:]==s+c+b''.join(resource[32:36] for resource in r)
report=dict(result='PASS',cases=len(cases),rejection_cases=len(bad),scope='Original post-selector 0x41f2b6..0x41f3f3 plus unhooked character and loaded-motion callees; all playback/controller state and 32 references; selector and entity gates excluded')
(root/'artifacts/motion-controller-verification.json').write_text(json.dumps(report,indent=2));print(report)
