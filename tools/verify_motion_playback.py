"""Compare consecutive complete 0x51ba80 calls with reconstructed playback.

Runs the original executable's instructions without replacing any callees.
Synthetic descriptors contain no original game assets; RF.exe stays local.
"""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,motions,data,stack=[0x30000000+i*0x100000 for i in range(5)]
for a in (obj,desc,motions,data,stack): u.mem_map(a,65536)
def put(a,fmt,*v): u.mem_write(a,struct.pack(fmt,*v))
def read(a,n): return bytes(u.mem_read(a,n))
rng=random.Random(0x51ba80); cases=[]; frames=64
for trial in range(160):
    count=trial%17; ids=rng.sample(range(32),count)
    resources=[]
    for i in range(32):
        start=rng.choice([-160,0,160]); end=start+rng.choice([480,1600,9440])
        loop=0 if trial%4==0 else 1 if trial%4==1 else rng.randrange(2)
        resources.append(struct.pack('<f4iI3i',rng.choice([0,.5,1,2]),start,end,rng.randrange(1000),rng.randrange(1000),loop,start+(end-start)//3,end,rng.randrange(3)))
    slots=b''.join(struct.pack('<iif',ids[i] if i<count else 0,rng.choice([0,160,480]),rng.choice([0,.25,.5,1])) for i in range(16))
    selected=lambda:rng.choice([-1]+list(range(count)))
    state=struct.pack('<I',count)+slots+struct.pack('<3i',selected(),selected(),selected())+struct.pack('<4I6f',int(trial%23==0),1,123,456,1,2,3,4,5,6)+struct.pack('<fII',rng.choice([0,.25,.999,1]),65530,rng.randrange(4))
    times=[rng.choice([0,1/60,1/30,.2,1,2]) for _ in range(frames)]
    cases.append((state,resources,struct.pack('<64f',*times)))
payload=b''.join(s+b''.join(r)+struct.pack('<I',frames)+t for s,r,t in cases)
run=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--update'],input=payload,capture_output=True,check=True)
stride=392; assert len(run.stdout)==len(cases)*frames*stride
advanced_updates=0
for k,(state,resources,times) in enumerate(cases):
    u.mem_write(obj,bytes(65536));u.mem_write(desc,bytes(65536))
    put(obj+0x1d50,'<I',desc);put(desc+0xf58,'<I',32)
    u.mem_write(obj+0x12d0,state[:196]);u.mem_write(obj+0x1cfc,state[196:204]);u.mem_write(obj+0x1d48,state[204:208])
    u.mem_write(obj+0x1d4c,state[208:209]);u.mem_write(obj+0x1d14,state[212:213]);u.mem_write(obj+0x1d18,state[216:248])
    u.mem_write(obj+0x1d04,state[248:252]);u.mem_write(obj+0x1cf8,state[252:254])
    events=struct.unpack_from('<I',state,256)[0];u.mem_write(obj+0x1d44,bytes([events&1,(events>>1)&1]))
    for i,r in enumerate(resources):
        m=motions+i*256;d=data+i*256
        put(desc+0xf5c+i*4,'<I',m);put(m+0x78,'<I',d)
        u.mem_write(desc+0x120c+i,r[20:21]);u.mem_write(m+0x50,r[24:28]);u.mem_write(m+0x64,r[28:32]);u.mem_write(m+0x74,r[32:36])
        u.mem_write(d+16,r[4:12]);u.mem_write(d+36,r[12:20]);put(d+80,'<I',84);u.mem_write(d+84,r[:4])
    for f in range(frames):
        before_generation=read(obj+0x1cf8,2)
        esp=stack+64000;stop=stack+65000
        put(esp,'<I',stop);u.mem_write(esp+4,times[f*4:f*4+4])
        for reg,value in [(UC_X86_REG_ESP,esp),(UC_X86_REG_ECX,obj),(UC_X86_REG_FPCW,0x37f)]:u.reg_write(reg,value)
        u.emu_start(0x51ba80,stop,count=100000)
        assert u.reg_read(UC_X86_REG_EIP)==stop,(k,f,hex(u.reg_read(UC_X86_REG_EIP)))
        advanced_updates+=read(obj+0x1cf8,2)!=before_generation
        expected=struct.pack('<i',0)+read(obj+0x12d0,196)+read(obj+0x1cfc,8)+read(obj+0x1d48,4)
        expected+=struct.pack('<II',read(obj+0x1d4c,1)[0],read(obj+0x1d14,1)[0])+read(obj+0x1d18,32)+read(obj+0x1d04,4)
        expected+=struct.pack('<II',struct.unpack('<H',read(obj+0x1cf8,2))[0],read(obj+0x1d44,1)[0]|read(obj+0x1d45,1)[0]<<1)
        expected+=b''.join(read(motions+i*256+0x74,4) for i in range(32))
        actual=run.stdout[(k*frames+f)*stride:(k*frames+f+1)*stride]
        assert actual==expected,(k,f,[(i,actual[i:i+4].hex(),expected[i:i+4].hex()) for i in range(0,stride,4) if actual[i:i+4]!=expected[i:i+4]])
report=dict(result='PASS',scenarios=len(cases),consecutive_frames=frames,updates=len(cases)*frames,advanced_updates=advanced_updates,early_returns=len(cases)*frames-advanced_updates,scope='Complete unhooked 0x51ba80; all playback state including stale slots, primary auxiliaries, generation, sticky events and 32 resource reference counts')
(root/'artifacts/motion-playback-verification.json').write_text(json.dumps(report,indent=2));print(report)
