"""Verify remaining turn-candidate branches through original callees."""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EBP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,motions,data,stack=[0x30000000+i*0x100000 for i in range(5)]
for a in (obj,desc,motions,data,stack): u.mem_map(a,65536)
def put(a,fmt,*v): u.mem_write(a,struct.pack(fmt,*v))
def read(a,n): return bytes(u.mem_read(a,n))
rng=random.Random(0x428c90);cases=[]
for k in range(4800):
    count=k%15; ids=rng.sample(range(32),count);selected=lambda:rng.choice([-1]+list(range(count)))
    slots=b''.join(struct.pack('<iif',ids[i] if i<count else 0,rng.randrange(-160,11000),rng.choice([0,.5,1])) for i in range(16))
    state=struct.pack('<I',count)+slots+struct.pack('<3i',selected(),selected(),selected())+struct.pack('<4I6f',rng.randrange(2),1,123,456,1,2,3,4,5,6)+struct.pack('<fII',.25,65535,3)
    resources=[struct.pack('<f4iI3i',1,rng.choice([-160,0,160]),9600,0,0,rng.choice([0,1,2,255]),0,0,rng.randrange(4)) for i in range(32)]
    actions=[rng.choice([-1]+list(range(32))) for i in range(45)];sounds=[-1]*45
    effects=struct.pack('<ff9i',rng.choice([-0.0,1,3000]),10,2,*[rng.randrange(-1,10000) for _ in range(5)],0,2,4)
    assert len(effects)==44
    context=struct.pack('<I6fifIi',rng.choice([0,0x800]),rng.uniform(1,20),.7,1.3,20,7,9,rng.choice([-1,0]),rng.uniform(.5,2),rng.choice([0,1,255]),rng.choice([0,1072800000-1200,1072800000-1,1072800000]))
    x=rng.choice([-1,-0.0,0,1e-20,1])
    finish=struct.pack('<IiiiI',rng.randrange(2),rng.randrange(2),0,rng.randrange(3),rng.choice([0,1,255]))
    cases.append((state,resources,actions,sounds,effects,context,x,finish))
wire=b''.join(s+e+c+struct.pack('<f',x)+b''.join(r)+struct.pack('<90i',*a,*n)+f for s,r,a,n,e,c,x,f in cases)
run=subprocess.run([str(root/'build/pc/Release/rf_turn_probe.exe'),'--finish'],input=wire,capture_output=True,check=True)
assert len(run.stdout)==len(cases)*440
entity=obj+0x6000;wrapper=obj+0x4000;info=obj+0x9000
for k,(s,resources,actions,sounds,effects,context,x,finish) in enumerate(cases):
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
    eligible,weapon,preferred,behavior,network=struct.unpack('<IiiiI',finish)
    put(entity+0x2a4,'<i',weapon);put(0x872114,'<i',preferred);put(entity+0x554,'<i',behavior);put(0x6fc4d8,'<B',network)
    u.reg_write(UC_X86_REG_EBX,eligible);u.reg_write(UC_X86_REG_EBP,0xffffffff)
    esp=stack+62000;stop=stack+65000;out_a=stack+65200;out_b=out_a+4
    put(esp,'<20I',*([0]*16),stop,entity,out_a,out_b);put(esp+16,'<f',x)
    u.mem_write(out_a,effects[36:44])
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_ESI,entity);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x41fc84,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected=read(obj+0x12d0,196)+read(obj+0x1cfc,8)+read(obj+0x1d48,4)+struct.pack('<II',read(obj+0x1d4c,1)[0],read(obj+0x1d14,1)[0])+read(obj+0x1d18,32)+read(obj+0x1d04,4)+struct.pack('<II',struct.unpack('<H',read(obj+0x1cf8,2))[0],read(obj+0x1d44,1)[0]|read(obj+0x1d45,1)[0]<<1)+read(entity+0x8c,4)+read(entity+0x8c0,8)+b''.join(read(entity+off,4) for off in offsets)+read(entity+0x7bc,4)+read(out_a,8)+struct.pack('<i',-1)+b''.join(read(motions+i*256+0x74,4) for i in range(32))
    actual=run.stdout[k*440:(k+1)*440]
    assert struct.unpack_from('<i',actual)[0]==0
    assert actual[4:]==expected,(k,[i for i in range(0,len(expected),4) if actual[4+i:8+i]!=expected[i:i+4]])
report=dict(result='PASS',cases=len(cases),scope='Original remaining candidate branches 0x41fc84 onward with unmodified movement, action activity, restart and absent-sound resolver; complete playback/references, effects and candidate results; preceding reset/decisions and valid-sound playback excluded')
(root/'artifacts/turn-finish-verification.json').write_text(json.dumps(report,indent=2));print(report)
