"""Verify complete candidate helper with absent-entry reset and absent sounds."""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_FPCW,UC_X86_REG_ESI
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,motions,data,stack=[0x30000000+i*0x100000 for i in range(5)]
for a in (obj,desc,motions,data,stack): u.mem_map(a,65536)
def put(a,fmt,*v): u.mem_write(a,struct.pack(fmt,*v))
def read(a,n): return bytes(u.mem_read(a,n))
reset_calls=[0]
u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:reset_calls.__setitem__(0,reset_calls[0]+1),begin=0x41ae70,end=0x41ae70)
rng=random.Random(0x41f9f0);cases=[]
coverage={address:0 for address in (0x41fa00,0x41fa3a,0x41fd95,0x41fbdc,0x41fca5,0x41fd2a)}
for address in coverage:u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:coverage.__setitem__(a,coverage[a]+1),begin=address,end=address)
for k in range(3000):
    count=k%15; ids=rng.sample(range(32),count);selected=lambda:rng.choice([-1]+list(range(count)))
    slots=b''.join(struct.pack('<iif',ids[i] if i<count else 0,rng.randrange(-160,11000),rng.choice([0,.5,1])) for i in range(16))
    state=struct.pack('<I',count)+slots+struct.pack('<3i',selected(),selected(),selected())+struct.pack('<4I6f',rng.randrange(2),1,123,456,1,2,3,4,5,6)+struct.pack('<fII',.25,65535,3)
    resources=[struct.pack('<f4iI3i',1,rng.choice([-160,0,160]),9600,0,0,rng.choice([0,1,2,255]),0,0,rng.randrange(4)) for i in range(32)]
    actions=[rng.choice([-1]+list(range(32))) for i in range(45)];sounds=[-1]*45
    effects=struct.pack('<ff9i',rng.choice([-0.0,1,3000]),10,2,*[rng.randrange(-1,10000) for _ in range(5)],0,2,4)
    assert len(effects)==44
    context=struct.pack('<I6fifIi',rng.choice([0,0x800]),rng.uniform(1,20),.7,1.3,20,7,9,rng.choice([-1,0]),rng.uniform(.5,2),rng.choice([0,1,255]),rng.choice([0,1072800000-1200,1072800000-1,1072800000]))
    x=rng.choice([-1,-0.0,0,1e-20,1])
    flags=rng.choice([0,0x800,0x20000,0x20800]);context=struct.pack('<I',flags)+context[4:]
    direction=struct.pack('<IIi12f',rng.choice([0,4]),8,rng.choice([0,1]),*rng.choice([(1,0,0),(-1,0,0),(0,0,0),(0,0,1)]),1,0,0,0,1,0,0,0,1)
    actor=direction+struct.pack('<iI3iI6f3I',rng.choice([0,9,12]),flags,-1,rng.choice([-1,0]),rng.choice([0,1]),rng.choice([0,0,0,1]),rng.choice([0,8.2,10]),rng.choice([0,1e-8]),0,0,0,0,rng.choice([0,1]),rng.choice([0,1]),rng.choice([0,1]))
    assert len(actor)==120
    cases.append((state,resources,actions,sounds,effects,context,x,actor))
# Empty slots force the reset/target gate without early activity suppression.
s,r,a,n,e,c,x,actor=cases[0];r=list(r)
for mid in (19,20):r[mid]=r[mid][:20]+struct.pack('<I',0)+r[mid][24:]
a=[-1]*45;a[19]=19;a[20]=20
direction=struct.pack('<IIi12f',0,8,1,1,0,0,1,0,0,0,1,0,0,0,1)
for dx,dy in [(8.2,0),(8.2,1e-8),(-8.2,1e-8),(10,0)]:
    actor=direction+struct.pack('<iI3iI6f3I',0,struct.unpack_from('<I',c)[0],-1,0,0,0,dx,dy,0,0,0,0,1,1,0)
    cases.append((s,r,a,n,e,c,x,actor))
wire=b''.join(s+e+c+struct.pack('<f',x)+b''.join(r)+struct.pack('<90i',*a,*n)+actor for s,r,a,n,e,c,x,actor in cases)
run=subprocess.run([str(root/'build/pc/Release/rf_turn_probe.exe'),'--update'],input=wire,capture_output=True,check=True)
assert len(run.stdout)==len(cases)*444
entity=obj+0x6000;wrapper=obj+0x4000;info=obj+0x9000
for k,(s,resources,actions,sounds,effects,context,x,actor) in enumerate(cases):
    u.mem_write(obj,bytes(65536));u.mem_write(desc,bytes(65536));put(obj+0x1d50,'<I',desc);put(desc+0xf58,'<I',32)
    u.mem_write(obj+0x12d0,s[:196]);u.mem_write(obj+0x1cfc,s[196:204]);u.mem_write(obj+0x1d48,s[204:208]);u.mem_write(obj+0x1d4c,s[208:209]);u.mem_write(obj+0x1d14,s[212:213]);u.mem_write(obj+0x1d18,s[216:248]);u.mem_write(obj+0x1d04,s[248:252]);u.mem_write(obj+0x1cf8,s[252:254]);u.mem_write(obj+0x1d44,b'\x01\x01')
    for i,r in enumerate(resources):
        m=motions+i*256;d=data+i*256;put(desc+0xf5c+i*4,'<I',m);put(m+0x78,'<I',d)
        u.mem_write(desc+0x120c+i,r[20:21]);u.mem_write(m+0x74,r[32:36]);u.mem_write(d+16,r[4:12])
    put(wrapper,'<II',2,obj);put(entity+0x80,'<I',wrapper)
    for i,mid in enumerate(actions):put(entity+0xa54+i*16,'<iIi',mid,0,sounds[i])
    u.mem_write(entity+0x8c,effects[:4]);u.mem_write(entity+0x8c0,effects[4:12])
    offsets=[0x79c,0x4d0,0x4d4,0x744,0x798]
    for i,offset in enumerate(offsets):u.mem_write(entity+offset,effects[12+4*i:16+4*i])
    u.mem_write(entity+0x7bc,effects[32:36]);put(entity+0x294,'<I',info)
    u.mem_write(info+0x724,context[:4]);u.mem_write(info+0x50,context[4:20]);u.mem_write(0x594590,context[20:24]);u.mem_write(0x59458c,context[24:28])
    u.mem_write(entity+0x75c,context[28:32]);u.mem_write(entity+0x98,context[32:36]);u.mem_write(0x64ecb9,context[36:37]);u.mem_write(0x5a3ed8,context[40:44])
    u.mem_write(info+0x728,actor[:4]);u.mem_write(entity+0x7d0,actor[4:8]);u.mem_write(entity+0x588,actor[8:12]);u.mem_write(entity+0x7a0,actor[12:24]);u.mem_write(entity+0x48,actor[24:60])
    mode,flags,weapon,preferred,behavior,network=struct.unpack_from('<iI3iI',actor,60)
    put(entity+0x858,'<I',obj+0xa000);put(obj+0xa004,'<i',mode);put(info+0x724,'<I',flags)
    put(entity+0x2a4,'<i',weapon);put(0x872114,'<i',preferred);put(entity+0x554,'<i',behavior);put(0x6fc4d8,'<B',network)
    u.mem_write(entity+0x7d4,actor[84:96]);u.mem_write(entity+0x6fc,actor[96:108]);u.mem_write(entity+0x6f8,actor[108:109]);u.mem_write(entity+0x53c,actor[112:113]);u.mem_write(entity+0x53d,actor[116:117])
    esp=stack+62000;stop=stack+65000;out_a=stack+65200;out_b=out_a+4
    put(esp,'<4I',stop,entity,out_a,out_b);u.mem_write(out_a,effects[36:44]);reset_calls[0]=0
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x41f9f0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected=read(obj+0x12d0,196)+read(obj+0x1cfc,8)+read(obj+0x1d48,4)+struct.pack('<II',read(obj+0x1d4c,1)[0],read(obj+0x1d14,1)[0])+read(obj+0x1d18,32)+read(obj+0x1d04,4)+struct.pack('<II',struct.unpack('<H',read(obj+0x1cf8,2))[0],read(obj+0x1d44,1)[0]|read(obj+0x1d45,1)[0]<<1)+read(entity+0x8c,4)+read(entity+0x8c0,8)+b''.join(read(entity+off,4) for off in offsets)+read(entity+0x7bc,4)+read(out_a,8)+struct.pack('<i',-1)+b''.join(read(motions+i*256+0x74,4) for i in range(32))
    expected+=struct.pack('<I',reset_calls[0])
    actual=run.stdout[k*444:(k+1)*444]
    assert struct.unpack_from('<i',actual)[0]==0
    assert actual[4:]==expected,(k,[i for i in range(0,len(expected),4) if actual[4+i:8+i]!=expected[i:i+4]])
assert all(coverage.values()),coverage
# Required adapters must not silently become no-ops.
s,r,a,n,e,c,x,actor=cases[-1]
wire=s+e+c+struct.pack('<f',x)+b''.join(r)+struct.pack('<90i',*a,*n)+actor
out=subprocess.run([str(root/'build/pc/Release/rf_turn_probe.exe'),'--update-no-reset'],input=wire,capture_output=True,check=True).stdout
assert len(out)==444 and struct.unpack_from('<i',out)[0]==-3
assert out[4:]==s+e+struct.pack('<i',-99)+b''.join(v[32:36] for v in r)+struct.pack('<I',0)
report=dict(result='PASS',cases=len(cases),required_reset_rejections=1,branch_coverage={hex(a):n for a,n in coverage.items()},scope='Complete original 0x41f9f0 with unmodified callees, absent weapon entry (-1) causing reset to return unchanged, and absent sound classes; complete state/effects/reference/candidate/reset-call comparison; populated reset and audio adapters excluded')
(root/'artifacts/turn-update-verification.json').write_text(json.dumps(report,indent=2));print(report)
