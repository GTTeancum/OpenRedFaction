"""Verify entity action start through loaded controls and sound-class dispatch."""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,motions,data,stack=[0x30000000+i*0x100000 for i in range(5)]
for a in (obj,desc,motions,data,stack): u.mem_map(a,65536)
def put(a,fmt,*v): u.mem_write(a,struct.pack(fmt,*v))
def read(a,n): return bytes(u.mem_read(a,n))
u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:uc.emu_stop(),begin=0x434da0,end=0x434da0)
rng=random.Random(0x428c90);cases=[]
for k in range(6000):
    count=k%15; ids=rng.sample(range(32),count);selected=lambda:rng.choice([-1]+list(range(count)))
    slots=b''.join(struct.pack('<iif',ids[i] if i<count else 0,rng.randrange(-160,11000),rng.choice([0,.5,1])) for i in range(16))
    state=struct.pack('<I',count)+slots+struct.pack('<3i',selected(),selected(),selected())+struct.pack('<4I6f',rng.randrange(2),1,123,456,1,2,3,4,5,6)+struct.pack('<fII',.25,65535,3)
    resources=[struct.pack('<f4iI3i',1,rng.choice([-160,0,160]),9600,0,0,rng.choice([0,1,2,255]),0,0,rng.randrange(4)) for i in range(32)]
    actions=[rng.choice([-1]+list(range(32))) for i in range(45)];sounds=[rng.randrange(-1,20) for i in range(45)]
    action=rng.choice([-1,45,rng.randrange(45)]);weight=rng.choice([0,.25,.5,1]);freeze=rng.choice([0,1,2,257,-255]);play=rng.choice([0,1,2,257,-255])
    cases.append((state,resources,actions,sounds,action,weight,freeze,play))
wire=b''.join(s+b''.join(r)+struct.pack('<91ifii',*a,*n,q,w,f,p) for s,r,a,n,q,w,f,p in cases)
run=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--start-action'],input=wire,capture_output=True,check=True)
assert len(run.stdout)==len(cases)*396
entity=obj+0x6000;wrapper=obj+0x4000
for k,(s,resources,actions,sounds,action,w,freeze,play) in enumerate(cases):
    u.mem_write(obj,bytes(65536));u.mem_write(desc,bytes(65536));put(obj+0x1d50,'<I',desc);put(desc+0xf58,'<I',32)
    u.mem_write(obj+0x12d0,s[:196]);u.mem_write(obj+0x1cfc,s[196:204]);u.mem_write(obj+0x1d48,s[204:208]);u.mem_write(obj+0x1d4c,s[208:209]);u.mem_write(obj+0x1d14,s[212:213]);u.mem_write(obj+0x1d18,s[216:248]);u.mem_write(obj+0x1d04,s[248:252]);u.mem_write(obj+0x1cf8,s[252:254]);u.mem_write(obj+0x1d44,b'\x01\x01')
    for i,r in enumerate(resources):
        m=motions+i*256;d=data+i*256;put(desc+0xf5c+i*4,'<I',m);put(m+0x78,'<I',d)
        u.mem_write(desc+0x120c+i,r[20:21]);u.mem_write(m+0x74,r[32:36]);u.mem_write(d+16,r[4:12])
    put(wrapper,'<II',2,obj);put(entity+0x80,'<I',wrapper)
    for i,mid in enumerate(actions):put(entity+0xa54+i*16,'<iIi',mid,0,sounds[i])
    esp=stack+64000;stop=stack+65000;put(esp,'<IIifii',stop,entity,action,w,freeze,play)
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x428c90,stop,count=100000);eip=u.reg_read(UC_X86_REG_EIP);assert eip in (stop,0x434da0)
    sound=-1 if eip==stop else struct.unpack('<i',read(u.reg_read(UC_X86_REG_ESP)+4,4))[0]
    expected=read(obj+0x12d0,196)+read(obj+0x1cfc,8)+read(obj+0x1d48,4)+struct.pack('<II',read(obj+0x1d4c,1)[0],read(obj+0x1d14,1)[0])+read(obj+0x1d18,32)+read(obj+0x1d04,4)+struct.pack('<II',struct.unpack('<H',read(obj+0x1cf8,2))[0],read(obj+0x1d44,1)[0]|read(obj+0x1d45,1)[0]<<1)+b''.join(read(motions+i*256+0x74,4) for i in range(32))
    actual=run.stdout[k*396:(k+1)*396]
    assert struct.unpack_from('<i',actual)[0]==0,(k,'status')
    assert struct.unpack_from('<i',actual,4)[0]==sound,(k,'sound class')
    assert actual[8:]==expected,(k,'state')
report=dict(result='PASS',cases=len(cases),scope='Original 0x428c90 through loaded type-two restart and sound-class resolver entry; all playback/reference fields and sound-class dispatch; sound selection/playback excluded')
(root/'artifacts/motion-action-start-verification.json').write_text(json.dumps(report,indent=2));print(report)
