"""Original41a505..41a7ab effect ordering, with external effects/predicates supplied."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda v:struct.pack('<f',v)
bits=lambda v:struct.unpack('<I',f(v))[0]
path=root/'Installed_Game/RF.exe';digest=hashlib.sha256(path.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im)
b=0x30000000;other=b+0x6000;burn=b+0x7000;stack=b+0xe000;trampoline=b+0xf000
u.mem_map(b,65536);u.mem_write(trampoline,b'\xd9\x05'+w(b+0xf100)+b'\xc3')
counts={0x428740:1,0x4196f0:2,0x429990:1,0x4290d0:1,0x42a8e0:1,0x429a80:1,0x425210:1,0x426fc0:1,0x42e910:2,0x504e40:2,0x4089f0:3,0x4085f0:3,0x505c00:1,0x5056a0:5,0x4a7520:0,0x4895d0:1,0x407fb0:4}
trace=[];hits={a:0 for a in counts}
def hook(m,address,size,context):
    if address not in counts:return
    sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<6I',m.mem_read(sp,24));a=args[1:1+counts[address]]
    trace.append((address,a));hits[address]+=1;result=0
    if address in predicates:result=predicates[address]
    if address==0x42a8e0:result=selected if a[0]==b else source_selected
    elif address==0x425210:result=other if uid_found else 0
    elif address==0x426fc0:result=other if source_exists else 0
    elif address==0x42e910:result=burn if create_success else 0
    elif address==0x505c00:result=playing
    elif address==0x5056a0:result=0x76540001
    elif address==0x504e40:
        m.reg_write(UC_X86_REG_EIP,trampoline);return
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,args[0])
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x41a505)
for i in range(8192):
    kind=[4,4,4,6,0,10,-1][i%7];source=rng.choice([-1,0x23450002]);uid=rng.choice([-1,26]);uid_found=rng.choice([0,1]);source_exists=rng.choice([0,1]);create_success=rng.choice([0,1])
    incoming=rng.choice([0,1,5,10,100]);maximum=rng.choice([100,1000,10000]);scaled=rng.choice([0,1,100]);oldhealth=rng.choice([-1,0,100]);health=rng.choice([-1,0,.25,100]);armor=rng.choice([0,25,50,100]);maxarmor=rng.choice([0,100])
    flags=rng.choice([0,0x80000000]);flags2=rng.choice([0,0x2000,0x8000,0xa000,0x1234a000]);hasburn=rng.choice([0,0,0,1]);classflags=rng.choice([0,16]);team=rng.choice([0,1]);otherteam=rng.choice([0,1]);selected=rng.choice([0,0,0,1,256,257]);source_selected=rng.choice([0,1,256,257]);playing=rng.choice([0,1,256])
    predicates={a:rng.choice([0,0,0,1,256,257]) for a in (0x429990,0x4290d0,0x429a80,0x4895d0)}
    u.mem_write(b,bytes(0x1500))
    for off,data in [(0x34,f(health)+f(armor)),(0x294,w(b+0x4000)),(0x2c,w(0x12340001)),(0x1f8,w(team)),(0x810,w(flags,flags2)),(0x854,w(0x65430001)),(0x13d8,w(burn if hasburn else 0))]:u.mem_write(b+off,data)
    u.mem_write(b+0x4044,f(maximum)+f(maxarmor));u.mem_write(b+0x4728,w(classflags));u.mem_write(other+0x1f8,w(otherteam));u.mem_write(other+0x2c,w(0x34560003));u.mem_write(b+0xf100,f(4))
    u.mem_write(stack+0x10,f(incoming)+f(oldhealth));u.mem_write(stack+0x20,f(scaled));u.mem_write(stack+0x28,w(kind))
    for reg,value in [(UC_X86_REG_ESP,stack),(UC_X86_REG_ESI,b),(UC_X86_REG_EBX,source&0xffffffff),(UC_X86_REG_EDI,uid&0xffffffff),(UC_X86_REG_EBP,kind&0xffffffff),(UC_X86_REG_FPCW,0x27f)]:u.reg_write(reg,value)
    trace=[];u.emu_start(0x41a505,0x41a7ab,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x41a7ab
    want=[];wantburn=burn if hasburn else 0;wantvoice=0x65430001
    def call(address,*args):want.append((address,tuple(a&0xffffffff for a in args)))
    if incoming>5:call(0x428740,b)
    # Original compares the unrounded quotient in x87 before passing a float.
    if incoming/maximum>.001 and kind!=10:call(0x4196f0,b,bits(incoming/maximum))
    if kind==4:
        if not hasburn:
            if maxarmor==0 or armor/maxarmor<.5 or flags2&0x8000:
                eligible=True
                for address in (0x429990,0x4290d0,0x42a8e0,0x429a80):
                    call(address,b)
                    if (selected if address==0x42a8e0 else predicates[address])&255:eligible=False;break
                if eligible:
                    responsible=source
                    if source==-1 and uid!=-1:
                        call(0x425210,uid)
                        if uid_found:responsible=0x34560003
                    call(0x426fc0,responsible)
                    should_create=not source_exists
                    if source_exists:
                        call(0x42a8e0,other)
                        should_create=bool((source_selected&255 and not flags2&0x2000) or team!=otherteam)
                    if should_create:
                        call(0x42e910,0x12340001,responsible if source_exists else -1)
                        wantburn=burn if create_success else 0
                    if wantburn and not classflags&16:
                        call(0x504e40,bits(5),bits(10));call(0x4089f0,b+0x2a0,bits(4),0)
            elif flags2&0x2000:
                call(0x504e40,bits(3),bits(5));call(0x4085f0,b+0x2a0,source,bits(4))
        flags2&=0xffff5fff
    if not flags&0x80000000 and kind==6:
        call(0x505c00,wantvoice)
        if not playing:
            call(0x5056a0,0x23,b+0x3c,bits(1),0x173c378,0);wantvoice=0x76540001
    call(0x42a8e0,b)
    if selected&255 and oldhealth>0 and scaled>0 and kind!=10:call(0x4a7520)
    call(0x4895d0,b)
    if not predicates[0x4895d0]&255 and health>0:call(0x407fb0,b+0x2a0,source,bits(incoming),0)
    assert trace==want,(i,trace,want)
    assert bytes(u.mem_read(b+0x814,4))==w(flags2),i
    assert bytes(u.mem_read(b+0x13d8,4))==w(wantburn),i
    assert bytes(u.mem_read(b+0x854,4))==w(wantvoice),i
assert all(hits.values()),hits
report=dict(result='PASS',cases=8192,calls={hex(k):v for k,v in hits.items()},original_sha256=digest,scope='Original41a505..41a7ab after vitals/credit; exact effect/predicate call order and arguments plus burn pointer, voice and flag changes. External effects, UID/entity lookup, predicate results and random return supplied. No effect implementations, lifecycle or multiplayer; x87 quotient checked before float argument rounding.')
(root/'artifacts/damage-effects-trace.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
