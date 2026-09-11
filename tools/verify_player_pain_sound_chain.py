"""Compose living-player pain selection/routing against the original executable.

Only TLS and audio device boundaries are supplied on the original side.
The NXDK side composes existing damage-sound, group and player-route helpers.
No actual PCM loading, device output, class construction or death is claimed.
"""
import hashlib,json,random,re,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xe000;stop=b+0xf000;player=b+0x2000;camera=b+0x3000
cls=b+0x4000;tls=b+0x6000;samples=b+0x7000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096)
    m.mem_write(base,im);m.mem_map(b,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
binary=root/'build/xbox/main.exe';u=machine(exe);x=machine(binary);helper=machine(binary)
symbols=(root/'build/xbox/main.map').read_text()
def entry(name):return int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16)
def run(m,address,args):
    m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.reg_write(UC_X86_REG_FPCW,0x27f)
    m.emu_start(address,stop,count=100000);assert m.reg_read(UC_X86_REG_EIP)==stop
    return m.reg_read(UC_X86_REG_EAX)
def ret(m,args,value):
    sp=m.reg_read(UC_X86_REG_ESP);m.reg_write(UC_X86_REG_EAX,value&0xffffffff)
    m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,args[0])
trace=[];playing=0;draws=0
def original_hook(m,address,size,context):
    global draws
    if address not in (0x577eef,0x505c00,0x505560,0x5056a0):return
    a=struct.unpack('<8I',m.mem_read(m.reg_read(UC_X86_REG_ESP),32));value=0
    if address==0x577eef:draws+=1;value=tls
    elif address==0x505c00:trace.append(('playing',a[1]));value=playing
    elif address==0x505560:trace.append(('play',w(0,a[1])+bytes(12)+w(a[4],a[3],a[2])));value=123
    else:
        assert a[4]==0x173c378
        trace.append(('play',w(1,a[1])+bytes(m.mem_read(a[2],12))+w(a[3],0,a[5])));value=456
    ret(m,a,value)
u.hook_add(UC_HOOK_CODE,original_hook)
def port_hook(m,address,size,context):
    if address not in (b+0x8000,b+0x8010,b+0x8020):return
    a=struct.unpack('<5I',m.mem_read(m.reg_read(UC_X86_REG_ESP),20));value=0
    if address==b+0x8000:
        group=a[2]
        if group>1:value=0xffffffff
        else:
            helper.mem_write(b,w(1,0,0)+bytes(20))
            assert run(helper,entry('rf_audio_group_choose'),[b,samples+group*64,16,counts[group],tls,b+0x100])==0
            value=struct.unpack('<I',helper.mem_read(b+0x100,4))[0]
    elif address==b+0x8010:trace.append(('playing',a[2]));value=playing
    else:
        position=bytes(m.mem_read(a[2],12))
        helper.mem_write(b,w(kind,present,mode)+position+w(a[3])+f(1)+w(0))
        assert run(helper,entry('rf_player_sound_route'),[b,b+0x100])==0
        trace.append(('play',bytes(helper.mem_read(b+0x100,32))))
    ret(m,a,value)
x.hook_add(UC_HOOK_CODE,port_hook)
rng=random.Random(0x4196f048);flat=spatial=suppressed=0
for case in range(2048):
    kind=rng.choice((0,0,1));present=rng.randrange(2);mode=rng.choice((0,0,1,256,-1))
    object_flags=rng.getrandbits(32);flags=rng.getrandbits(32);action=rng.choice((0,1,17,22))
    now=rng.choice((0,1000,1072799500));deadline=rng.choice((-1,now,now+1))
    fraction=rng.choice((0,.1,.3,1));playing=rng.choice((0,0,1,256));seed=rng.getrandbits(32)
    counts=[rng.choice((0,1,2,16)),rng.choice((0,1,2,16))]
    values=[[rng.choice((-1,0,18,564)) for _ in range(16)] for _ in range(2)]
    groups=[rng.choice((-1,0,1)),rng.choice((-1,0,1))];position=f(1,-2,3)
    # Keep a substantial always-eligible player cohort alongside suppression cases.
    if case<512:
        kind=0;present=1;mode=(0,1,256,-1)[case%4];object_flags=8;flags=0;action=0
        now=1000;deadline=0;playing=0;groups=[0,1];counts=[1,16]
        values=[[18]*16,list(range(560,576))]
    actor=bytearray(0x1500)
    for offset,data in ((0x24,w(kind)),(0x34,f(100)),(0x7c,w(object_flags)),(0x294,w(cls)),(0x29c,w(cls)),
                        (0x520,w(action)),(0x7d4,position),(0x808,w(0x87654321)),(0x810,w(flags)),
                        (0x1430,w(player if present else 0)),(0x1458,w(deadline))):actor[offset:offset+len(data)]=data
    u.mem_write(b,bytes(actor));u.mem_write(player+0xc4,w(camera));u.mem_write(camera+8,w(mode))
    u.mem_write(cls+0x124,w(-1));u.mem_write(cls+0x16c,w(*groups));u.mem_write(0x5a3ed8,w(now))
    u.mem_write(0x636ef8,w(2));u.mem_write(tls+20,w(seed));helper.mem_write(tls,w(seed))
    for group in range(2):
        u.mem_write(0x63011c+group*0x2c,w(counts[group],samples+group*64))
        u.mem_write(samples+group*64,w(*values[group]));helper.mem_write(samples+group*64,w(*values[group]))
    trace=[];draws=0;run(u,0x4196f0,[b,struct.unpack('<I',f(fraction))[0]])
    want=list(trace);want_deadline=bytes(u.mem_read(b+0x1458,4));want_rng=bytes(u.mem_read(tls+20,4))
    actor[0x1458:0x145c]=want_deadline;assert bytes(u.mem_read(b,len(actor)))==actor
    wire=f(100)+w(flags,-1,-1,*groups,action,deadline,0x87654321)+position
    x.mem_write(b,wire);x.mem_write(b+0x1000,w(b+0x8000,b+0x8010,b+0x8020,0))
    trace=[];predicate_b=bool(object_flags&8) and bool(present)
    assert run(x,entry('rf_entity_damage_sound'),[b,struct.unpack('<I',f(fraction))[0],flags&1,int(predicate_b),now,b+0x1000])==0
    assert trace==want,(case,trace,want)
    assert bytes(x.mem_read(b+28,4))==want_deadline and bytes(helper.mem_read(tls,4))==want_rng,case
    output=[item for item in trace if item[0]=='play']
    if not output:suppressed+=1
    elif struct.unpack('<I',output[0][1][:4])[0]:spatial+=1
    else:flat+=1
assert flat and spatial and suppressed
report=dict(result='PASS',cases=2048,flat=flat,spatial=spatial,no_play=suppressed,original_sha256=digest,
    nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
    scope='Living actors: complete original4196f0 with real predicates, timers,434da0/CRT RNG and48a9c0 camera routing; only TLS and device boundaries supplied. Exact composed NXDK helper playback requests, cooldown and RNG. Full actor unchanged except cooldown. Prepared class groups/owners; no class factory, PCM, death or live audio integration.')
(root/'artifacts/player-pain-sound-chain.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
