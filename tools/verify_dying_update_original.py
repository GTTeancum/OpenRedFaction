"""Execute complete original41ee40; supply owner/effect boundaries, retain vectors."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
b=0x30000000;cls=b+0x2000;player=b+0x4000;owner=b+0x6000;event=b+0x7000;stack=b+0xe000;stop=b+0xf000
u.mem_map(b,65536);u.mem_write(stop+16,b'\xd9\xee\xc3') # damage returns float zero
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
read=lambda a,n:list(struct.unpack('<'+'I'*n,u.mem_read(a,4*n)))
counts={0x42e3c0:1,0x42ed20:2,0x428d10:2,0x41a830:2,0x41ae70:2,
        0x4fa3f0:0,0x506ae0:5,0x4892c0:8,0x40e0b0:3,0x418f80:1,
        0x5001d0:2,0x4c0e00:1,0x4c0200:1,0x4be410:1,0x4b6760:3}
trace=[];answers={};segment=[]
def hook(m,a,size,data):
    if a not in counts:return
    sp=m.reg_read(UC_X86_REG_ESP);ret=read(sp,1)[0];args=read(sp+4,counts[a])
    if a==0x4fa3f0:args=[m.reg_read(UC_X86_REG_ECX)]
    if a==0x506ae0:
        segment[:]=[read(args[1],3),read(args[2],3),read(args[3],3),args[4]]
        args=[args[1],args[3],args[4]] # stack-local output/end addresses excluded
    trace.append([a,args])
    if a==0x4892c0:
        m.reg_write(UC_X86_REG_EIP,stop+16);return
    m.reg_write(UC_X86_REG_EAX,answers.get(a,0));m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x41ee40);finalized=damaged=burns=0;digest=hashlib.sha256()
for case in range(4096):
    flags=rng.getrandbits(32);action=rng.choice((-1,5,12,16));burn=rng.choice((0,12345))
    special=rng.choice((0,0x20));present=rng.choice((0,1));radius=rng.choice((2.,6.,6.25,9.))
    playing,weapon,timer,hit,named=[rng.choice((0,1,2,256,257,0xffffffff)) for _ in range(5)]
    found_a,found_b=[rng.choice((0,event)) for _ in range(2)]
    body=bytearray(rng.randbytes(0x1500))
    for off,value in ((0x2c,77),(0x294,cls),(0x2a4,9),(0x810,flags),(0x824,action),(0x13d8,burn)):body[off:off+4]=w(value)
    body[0x3c:0x48]=struct.pack('<3f',1,2,3);body[0x60:0x6c]=struct.pack('<3f',0,0,1);body[0x78:0x7c]=struct.pack('<f',radius)
    u.mem_write(b,bytes(body));u.mem_write(cls+0x728,w(special));u.mem_write(player+0x2c,w(88));u.mem_write(player+0x3c,struct.pack('<3f',4,5,6))
    u.mem_write(owner+0xc4,w(99));u.mem_write(event+0x30,w(111));u.mem_write(0x5cb054,w(player if present else 0));u.mem_write(0x7c75d4,w(owner))
    answers={0x428d10:playing,0x41a830:weapon,0x4fa3f0:timer,0x506ae0:hit,0x5001d0:named,0x4c0e00:found_a,0x4be410:found_b}
    trace=[];segment=[];u.mem_write(stack,w(stop,b));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x41ee40,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected=[]
    def call(a,*args):expected.append([a,list(args)])
    finish=bool(flags&0x80) or action==-1 or not playing&255
    if flags&0x80:
        call(0x42e3c0,77)
        if burn:call(0x42ed20,burn,0);body[0x13d8:0x13dc]=w(0);burns+=1
    elif action!=-1:call(0x428d10,b,action&0xffffffff)
    call(0x41a830,77,9)
    if weapon&255==1:call(0x41ae70,77,9)
    if special and present:
        call(0x4fa3f0,b+0x4b8)
        if timer&255==1:
            wide=radius>6;size=0x40200000 if wide else 0x3fc00000
            call(0x506ae0,b+0x3c,player+0x3c,size)
            assert segment==[read(b+0x3c,3),list(struct.unpack('<3I',struct.pack('<3f',1,2,3+radius))),read(player+0x3c,3),size]
            if hit&255==1:call(0x4892c0,88,0x44c80000,77,0xffffffff,0xffffffff,0,0xffffffff,0);damaged+=1
            call(0x40e0b0,99,0x3b449ba6,0x3fa00000 if wide else 0x3f800000)
    if finish:
        finalized+=1;call(0x418f80,b);call(0x5001d0,b+0x18,0x595888)
        if named&255:
            call(0x4c0e00,0x118a)
            if found_a:call(0x4c0200,found_a)
            call(0x4be410,0x47c3)
            if found_b:call(0x4b6760,111,0xffffffff,0xffffffff)
    assert trace==expected,(case,trace,expected)
    assert bytes(u.mem_read(b,len(body)))==body,case
    digest.update(json.dumps(trace).encode())
    if 'observe_case' in globals():observe_case(globals())
report=dict(result='PASS',cases=4096,finalized=finalized,damage_calls=damaged,burn_clears=burns,trace_sha256=digest.hexdigest(),original_sha256=sha,
    scope='Full original41ee40 with real vector initialization/scale/add; supplied playback, weapon, timer, collision, damage, camera, finalization and event boundaries. Exact call order/arguments and actor mutations, including low-byte gates and radius6 branch. Stable callback owners; no reconstructed runtime, actual effect implementations or reentrant mutations verified.')
(root/'artifacts/dying-update-original.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
