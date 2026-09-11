"""Original burn owner damage/fade tail and shared spread-timer scheduling."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda v:struct.pack('<f',v)
roundf=lambda v:struct.unpack('<f',f(v))[0]
bits=lambda v:struct.unpack('<I',f(v))[0]
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(original));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im)
b=0x30000000;entity=b+0x4000;stack=b+0xe000;trampoline=b+0xf000;u.mem_map(b,65536)
u.mem_write(trampoline,b'\xd9\x05'+w(b+0xf100)+b'\xc3');u.mem_write(trampoline+16,b'\xd9\xee\xc3');trace=[]
def hook(m,address,size,context):
    if address not in (0x5058c0,0x504e40,0x4892c0,0x57312d,0x42f2f0):return
    sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<9I',m.mem_read(sp,36));count={0x5058c0:4,0x504e40:2,0x4892c0:8,0x57312d:0,0x42f2f0:1}[address]
    trace.append((address,a[1:count+1]));result=0
    if address==0x504e40:m.reg_write(UC_X86_REG_EIP,trampoline);return
    if address==0x4892c0:m.reg_write(UC_X86_REG_EIP,trampoline+16);return
    if address==0x57312d:result=random_integer
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x42f1dc);damage_calls=0;fade_calls=0
wire_cases=[];wire_expected=[]
def snapshot():
    return bytes(u.mem_read(b,64))+bytes(u.mem_read(entity+0x2044,4))+bytes(u.mem_read(entity+0x810,4))+bytes(u.mem_read(entity+0x824,4))+bytes(u.mem_read(entity+0x2c,4))+bytes(u.mem_read(entity+0x3c,12))+bytes(u.mem_read(entity+0x144,12))
for i in range(4096):
    delta=roundf(rng.choice([0,1/60,1/30,.1]));maximum=roundf(rng.uniform(1,1000));divisor=roundf(rng.uniform(5,8));elapsed=roundf(rng.uniform(0,15));fading=rng.choice([0,1,256,257]);flags=rng.choice([0,1,256,257]);random_integer=rng.randrange(32768)
    u.mem_write(b,bytes(64));u.mem_write(entity+0x3c,f(1)+f(2)+f(3));u.mem_write(entity+0x144,f(-1)+f(-2)+f(-3))
    u.mem_write(b+0x24,w(0xffffffff,0x3f000000,fading));u.mem_write(b+0x30,f(elapsed));u.mem_write(entity+0x294,w(entity+0x2000));u.mem_write(entity+0x2044,f(maximum));u.mem_write(entity+0x810,w(flags));u.mem_write(entity+0x824,w(0xabcdef01));u.mem_write(entity+0x2c,w(0x12340001));u.mem_write(0x5a4014,f(delta));u.mem_write(b+0xf100,f(divisor))
    for reg,value in [(UC_X86_REG_ESP,stack),(UC_X86_REG_ESI,b),(UC_X86_REG_EBX,entity),(UC_X86_REG_FPCW,0x27f)]:u.reg_write(reg,value)
    wire_cases.append(snapshot()+f(delta)+f(divisor)+w(random_integer))
    trace=[];u.emu_start(0x42f1dc,0x42f2a2,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x42f2a2
    normalized=[]
    for address,args in trace:
        if address==0x5058c0:row=w(address,args[0])+bytes(u.mem_read(args[1],12))+bytes(u.mem_read(args[2],12))+w(args[3])
        else:row=w(address,*args).ljust(36,b'\0')
        normalized.append(row)
    wire_expected.append(w(0)+snapshot()+w(len(trace))+b''.join(normalized)+bytes((4-len(trace))*36))
    want=[(0x5058c0,(0xffffffff,entity+0x3c,entity+0x144,0x3f000000))];action=0xabcdef01
    if fading&255:
        fade_calls+=1;elapsed=roundf(elapsed+delta);want.append((0x42f2f0,(b,)))
    elif not flags&1:
        damage_calls+=1;amount=bits(delta*(maximum/divisor));want.extend([(0x504e40,(bits(5),bits(8))),(0x4892c0,(0x12340001,amount,0xffffffff,0xffffffff,4,0,0xffffffff,0)),(0x57312d,())]);action=(5,14,15)[random_integer%3]
    assert trace==want,(i,trace,want)
    assert bytes(u.mem_read(b+0x30,4))==f(elapsed),i
    assert bytes(u.mem_read(entity+0x824,4))==w(action),i
# The end-of-pass timer is shared, not the owner-damage cadence above.
timer_cases=0;period=1072800000
for now in (0,100,1000,period-100,period):
 for deadline in (-1,0,100,1000,period-100,period):
    timer_cases+=1;u.mem_write(0x5a3ed8,w(now));u.mem_write(0x62f768,w(deadline));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x42f2af,0x42f2db,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x42f2db
    expired=deadline>=0 and (deadline-now>period//2 if deadline>now else now-deadline<=period//2)
    target=now+225 if deadline==-1 or expired else deadline
    if target>period:target-=period
    assert bytes(u.mem_read(0x62f768,4))==w(target),(now,deadline,target)
report=dict(result='PASS',owner_tail_cases=4096,damage_requests=damage_calls,fade_requests=fade_calls,timer_cases=timer_cases,original_sha256=digest,scope='42f1dc..42f2a2 owner audio/damage/fade tail with actual427020; supplied random values and downstream calls. Exact arguments, action824 and elapsed30. Real timer callees in42f2af..42f2db,225ms rearm/wrap. Excludes spread geometry, attachment/emitter update, downstream fade and damage execution.')
(root/'artifacts/burn-tick-trace.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
