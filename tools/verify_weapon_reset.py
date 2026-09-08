"""Original valid-entity reset, with observation stops at unavailable adapters."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
obj,desc,motions,stack=[0x30000000+i*0x100000 for i in range(4)]
for a in (obj,desc,motions,stack):u.mem_map(a,65536)
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
def read(a,n):return bytes(u.mem_read(a,n))
boundaries={0x505a40:'stop_sound',0x41af0a:'release_sound',0x48f130:'stop_effect',0x4a6f10:'player_reset'}
for a in boundaries:u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:uc.emu_stop(),begin=a,end=a)
local_lookups=[0]
u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:local_lookups.__setitem__(0,local_lookups[0]+1),begin=0x48aa90,end=0x48aa90)
rng=random.Random(0x41ae70);cases=[]
for k in range(2400):
    weapon=rng.choice([-1,64,rng.randrange(64),rng.randrange(64)])
    active=bytes(rng.choice([0,1,2,255]) for _ in range(64))
    state=active+struct.pack('<IIiiiII',rng.getrandbits(32),rng.choice([0,1]),rng.choice([-1,-1,99]),rng.randrange(-1,99),rng.choice([-1,-1,9]),rng.randrange(2),rng.randrange(2))
    local=rng.randrange(2)
    descriptors=b''.join(struct.pack('<IIi',rng.randrange(8),rng.choice([0,64]),rng.choice([-2,-1,-1,0])) for _ in range(64))
    context=struct.pack('<II',rng.choice([0,1,2,256]),rng.choice([0,32,64]))
    count=rng.randrange(8);ids=rng.sample(range(32),count)
    playback=struct.pack('<I',count)+b''.join(struct.pack('<iif',ids[i] if i<count else 0,rng.randrange(9600),rng.choice([0,.5,1])) for i in range(16))
    playback+=struct.pack('<3i4I6ffII',rng.randrange(count) if count else -1,rng.randrange(count) if count else -1,rng.randrange(count) if count else -1,1,1,2,3,1,2,3,4,5,6,.25,19,3)
    assert len(playback)==260
    resources=b''.join(struct.pack('<f4iI3i',1,0,9600,0,0,rng.choice([0,1,2,255]),0,0,rng.randrange(4)) for _ in range(32))
    cases.append((struct.pack('<i',weapon)+state+descriptors+context+playback+resources,local))
run=subprocess.run([str(root/'build/pc/Release/rf_weapon_probe.exe')],input=b''.join(wire for wire,local in cases),capture_output=True,check=True)
assert len(run.stdout)==len(cases)*484
entity=obj+0x6000;wrapper=obj+0x4000;info=obj+0x9000;coverage={name:0 for name in ['complete',*boundaries.values()]}
for k,(wire,local) in enumerate(cases):
    weapon,=struct.unpack_from('<i',wire);state=wire[4:96];descriptors=wire[96:864];context=wire[864:872];s=wire[872:1132];resources=wire[1132:]
    u.mem_write(obj,bytes(65536));u.mem_write(desc,bytes(65536));put(obj+0x1d50,'<I',desc);put(desc+0xf58,'<I',32)
    u.mem_write(obj+0x12d0,s[:196]);u.mem_write(obj+0x1cfc,s[196:204]);u.mem_write(obj+0x1d48,s[204:208]);u.mem_write(obj+0x1d4c,s[208:209]);u.mem_write(obj+0x1d14,s[212:213]);u.mem_write(obj+0x1d18,s[216:248]);u.mem_write(obj+0x1d04,s[248:252]);u.mem_write(obj+0x1cf8,s[252:254]);u.mem_write(obj+0x1d44,b'\x01\x01')
    for i in range(32):
        r=resources[i*36:(i+1)*36];put(desc+0xf5c+i*4,'<I',motions+i*256);u.mem_write(motions+i*256+0x74,r[32:36]);u.mem_write(desc+0x120c+i,r[20:21])
    put(wrapper,'<II',2,obj);put(entity+0x80,'<I',wrapper if struct.unpack_from('<I',state,84)[0] else 0)
    put(entity+0x294,'<I',info);put(info+0x94,'<I',2);put(entity+0x2c,'<i',7);put(0x7394cc+7*4,'<I',entity)
    u.mem_write(entity+0x46c,state[:64]);u.mem_write(entity+0x7d0,state[64:68]);u.mem_write(entity+0x810,state[68:72]);u.mem_write(entity+0x81c,state[72:80]);u.mem_write(entity+0x13d4,state[80:84])
    player,=struct.unpack_from('<I',state,88);put(entity+0x7c,'<I',8 if player else 0);put(entity+0x1430,'<I',obj+0xb000 if player else 0)
    put(0x7c7634,'<I',local);put(0x7c75e4,'<I',obj+0xc000);put(obj+0xc014,'<I',7)
    for i in range(64):
        f,g,sound=struct.unpack_from('<IIi',descriptors,i*12);put(0x85cd08+i*1360+0x264,'<II',f,g);put(0x85cd08+i*1360+0x204,'<i',sound)
    disabled,n=struct.unpack('<II',context);put(0x64ecbb,'<B',disabled&255);put(0x872448,'<I',n)
    for a in [0x872110,0x872464,0x872444,0x85cd04,0x85ccfc]:put(a,'<i',weapon)
    stop=stack+65000;put(stack+64000,'<Iii',stop,7,weapon);u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x41ae70,stop,count=100000);end=u.reg_read(UC_X86_REG_EIP);assert end==stop or end in boundaries,hex(end)
    coverage['complete' if end==stop else boundaries[end]]+=1
    expected=struct.pack('<i',0 if end==stop else -3)+read(entity+0x46c,64)+read(entity+0x7d0,4)+read(entity+0x810,4)+read(entity+0x81c,8)+read(entity+0x13d4,4)+state[84:]
    expected+=read(obj+0x12d0,196)+read(obj+0x1cfc,8)+read(obj+0x1d48,4)+struct.pack('<II',read(obj+0x1d4c,1)[0],read(obj+0x1d14,1)[0])+read(obj+0x1d18,32)+read(obj+0x1d04,4)+struct.pack('<II',struct.unpack('<H',read(obj+0x1cf8,2))[0],3)
    expected+=b''.join(read(motions+i*256+0x74,4) for i in range(32))
    assert run.stdout[k*484:(k+1)*484]==expected,(k,hex(end))
assert all(coverage.values()),coverage
assert local_lookups[0]>0
report=dict(result='PASS',cases=len(cases),coverage=coverage,completed_local_lookups=local_lookups[0],scope='Original reset with valid resolved type-zero entity, full state/playback/reference comparison, unmodified callees including complete read-only 48aa90 lookups; observation stops before unavailable sound/effect/player operations and matches explicit RF_NOT_FOUND; successful external adapters remain unverified')
(root/'artifacts/weapon-reset-verification.json').write_text(json.dumps(report,indent=2));print(report)
