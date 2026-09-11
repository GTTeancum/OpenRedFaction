"""Original42f2f0 fading: exact emitter writes, timer gates and release order."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda v:struct.pack('<f',v)
roundf=lambda v:struct.unpack('<f',f(v))[0]
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(original));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im)
b=0x30000000;emitters=[b+0x1000+i*0x200 for i in range(4)];owner=b+0x4000;stack=b+0xe000;stop=b+0xf000;u.mem_map(b,65536);trace=[]
actual_stop=globals().get('actual_stop',False)
def hook(m,address,size,context):
    if address not in (0x4973d0,0x4174c0,0x426fc0,0x407ee0,0x42ed20):return
    sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<3I',m.mem_read(sp,12));result=0
    if address==0x4973d0:args=(m.reg_read(UC_X86_REG_ECX),)
    else:args=a[1:3] if address==0x42ed20 else a[1:2]
    trace.append((address,args))
    if address==0x4973d0 and actual_stop:return
    if address==0x4174c0:result=owner
    elif address==0x426fc0:result=owner if entity_present else 0
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,hook)
scales={a:struct.unpack('<f',u.mem_read(a,4))[0] for a in (0x589444,0x5894d0,0x58952c)}
offsets=(0x24,0x28,0x30,0x34,0x44,0x48);rng=random.Random(0x42f2f0);released=scaled=reset=0
elapsed_values=[0,5,12,17,18]+[struct.unpack('<f',w(bits))[0] for bits in (0x40a00001,0x41400001,0x41880001)]
wire_cases=[];wire_expected=[]
def snapshot():
    data=bytes(u.mem_read(b,64))
    for ptr in emitters:
        data+=b''.join(bytes(u.mem_read(ptr+off,4)) for off in offsets)+bytes(u.mem_read(ptr+0x87,1))+bytes(u.mem_read(ptr+0x140,1))+bytes(2)
    return data+bytes(u.mem_read(owner+0x29c,4))
for i in range(8192):
    elapsed=elapsed_values[i%len(elapsed_values)];now=rng.choice([0,1000,1072799900]);deadline=rng.choice([-1,now,now+1,0]);entity_present=rng.choice([0,1]);countdown=rng.choice([0,1,2,128,255]);active=[rng.choice([0,0,1,2,255]) for _ in range(3)];flags=rng.getrandbits(32);volume=roundf(rng.uniform(0,1))
    expected_emitters=[]
    for index,ptr in enumerate(emitters):
        data=bytearray([0xa5]*0x180)
        for offset in offsets:data[offset:offset+4]=f(rng.uniform(.001,1000))
        data[0x140]=active[index] if index<3 else 0;data[0x87]=countdown if index==3 else 17
        u.mem_write(ptr,bytes(data));expected_emitters.append(data)
    u.mem_write(b,bytes(64))
    u.mem_write(b,w(*emitters,0x12340001));u.mem_write(b+0x28,f(volume));u.mem_write(b+0x30,f(elapsed));u.mem_write(owner+0x29c,w(flags));u.mem_write(0x5a3ed8,w(now));u.mem_write(0x62f768,w(deadline))
    wire_cases.append(snapshot()+w(deadline,now,entity_present))
    trace=[];u.mem_write(stack,w(stop,b));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x42f2f0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    wire_expected.append(w(0)+snapshot()+w(len(trace))+b"".join(w(address,*args).ljust(12,b"\0") for address,args in trace)+bytes((8-len(trace))*12))
    want=[]
    if elapsed>12 and any(active):
        reset+=1;want.extend([(0x4973d0,(ptr,)) for ptr in emitters[:3]]);want.append((0x4174c0,(0x12340001,)));flags=(flags&0xfffffdff)|0x100
        if actual_stop:
            for data in expected_emitters[:3]:data[0x140]=0
    expired=deadline>=0 and (deadline-now>1072800000//2 if deadline>now else now-deadline<=1072800000//2)
    release=elapsed>17
    if not release and expired and elapsed>5:
        countdown=(countdown-1)&255;expected_emitters[3][0x87]=countdown;release=countdown==0
    if release:
        released+=1;want.append((0x426fc0,(0x12340001,)))
        if entity_present:want.append((0x407ee0,(owner+0x2a0,)))
        want.append((0x42ed20,(b,0)))
    elif expired:
        scaled+=1
        for index,data in enumerate(expected_emitters):
            for offset in offsets:
                if index==3 and offset in (0x30,0x34):continue
                factor=scales[0x5894d0 if index==3 else 0x58952c if offset in (0x24,0x28) else 0x589444]
                value=struct.unpack('<f',data[offset:offset+4])[0];data[offset:offset+4]=f(value*factor)
        volume=roundf(volume*scales[0x589444])
    assert trace==want,(i,trace,want)
    assert bytes(u.mem_read(owner+0x29c,4))==w(flags),i
    assert bytes(u.mem_read(b+0x28,4))==f(volume),i
    for ptr,data in zip(emitters,expected_emitters):assert bytes(u.mem_read(ptr,0x180))==data,(i,hex(ptr))
report=dict(result='PASS',cases=8192,release_requests=released,scaling_passes=scaled,emitter_stop_groups=reset,original_sha256=digest,scope='Complete42f2f0 with actual shared timer. Exact six float fields per emitter, fourth byte87, volume28, owner flags29c and callback order. Supplied emitter stop, owner/entity lookup, reaction and release; four distinct emitter objects and a valid4174c0 result. Threshold next-floats, byte wrap, timer clear/future/expired and missing entity covered. No actual downstream stop/release or owner type conversion.')
if actual_stop:report['scope']='Complete42f2f0 with actual timer and4973d0 stop. Exact raw emitter writes including preserved enable high bytes and color RGB, owner flags, volume and callback order. Owner lookup/reaction/release supplied; no resource release or live gameplay.'
(root/('artifacts/burn-fade-stop-trace.json' if actual_stop else 'artifacts/burn-fade-trace.json')).write_text(json.dumps(report,indent=2)+'\n');print(report)
