"""Original4892c0 SP eligibility/dispatch evidence; effect callees are supplied."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda v:struct.pack('<f',v)
roundf=lambda v:struct.unpack('<f',f(v))[0]
path=root/'Installed_Game/RF.exe';digest=hashlib.sha256(path.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im)
b=0x30000000;stack=b+0xe000;finish=b+0xf000;stop=finish+6;retfloat=b+0xf100
u.mem_map(b,65536);u.mem_write(finish,b'\xd9\x1d'+w(b+0x2000));u.mem_write(retfloat,b'\xd9\x05'+w(b+0x2004)+b'\xc3');u.mem_write(b+0x2004,f(7.25))
u.mem_write(0x64ecb9,bytes(2));u.mem_write(0x6fc4d8,bytes(1))
table=struct.unpack('<4f',u.mem_read(0x593dd4,16));minimum=struct.unpack('<f',u.mem_read(0x5895d0,4))[0]
trace=[];effects=[]
def hook(m,address,size,context):
    if address not in (0x40a0e0,0x426fc0,0x42cca0,0x48aaf0,0x41a350,0x410270,0x417c60):return
    sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<6I',m.mem_read(sp,24));trace.append(address)
    if address==0x40a0e0:result=b if present else 0;assert args[1]==0x12340001
    elif address==0x426fc0:result=b if entity else 0
    elif address==0x42cca0:result=immune
    elif address==0x48aaf0:result=player
    else:
        count=4 if address==0x417c60 else 5;effects.append((address,args[1:count+1]))
        # Supplied side effect lets final health cleanup be observed after delegation.
        m.mem_write(b+0x34,f(after))
        if address==0x41a350:m.reg_write(UC_X86_REG_EIP,retfloat);return
        result=0
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,args[0])
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x4892c0);accepted=0;routes={};cases=[];expected=[]
for i in range(8192):
    present=rng.choice([0,1,1,1]);entity=rng.choice([0,1]);immune=rng.choice([0,1,256,257]);player=rng.choice([0,1,256,257])
    flags=rng.choice([0,4,0x100]);force=rng.choice([0,1,256,257]);kind=rng.choice([0,4,9]);typ=i%10;difficulty=i%4
    amount=rng.choice([0,roundf(.001),roundf(.000999),1,100,roundf(100.00001),200]);health=roundf(rng.choice([-.1,0,.25,.5,.500001,100]));after=roundf(rng.choice([-.1,0,.25,.5,1]))
    cases.append(w(typ,flags)+f(health)+f(amount)+w(0x23450002,kind,0x45670004,0x56780005,force)+f(table[difficulty])+w(present,entity,immune,player)+f(after))
    u.mem_write(b,bytes(0x1500));u.mem_write(b+0x24,w(typ));u.mem_write(b+0x34,f(health));u.mem_write(b+0x7c,w(flags));u.mem_write(0x593e54,w(difficulty))
    u.mem_write(stack,w(finish,0x12340001)+f(amount)+w(0x23450002,0x34560003,kind,0x45670004,0x56780005,force))
    trace=[];effects=[];u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x4892c0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    effect_wire=w(effects[0][0],*effects[0][1]) if effects else bytes(24)
    effect_wire=effect_wire.ljust(24,b"\0")
    expected.append(w(0,typ)+bytes(u.mem_read(b+0x7c,4))+bytes(u.mem_read(b+0x34,4))+bytes(u.mem_read(b+0x2000,4))+w(len(trace))+w(*trace).ljust(32,b"\0")+effect_wire)
    want_trace=[0x40a0e0];want_effects=[];want_flags=flags;want_health=health;returned=0
    eligible=bool(present and amount>=minimum)
    if eligible:
        want_flags|=0x200000
        if not force&255:
            if flags&4:eligible=False
            else:
                want_trace.append(0x426fc0)
                if entity:
                    want_trace.append(0x42cca0)
                    if immune&255:eligible=False
            if eligible and kind!=9:
                want_trace.append(0x48aaf0)
                if player&255:amount=roundf(amount*table[difficulty])
    if eligible:
        accepted+=1
        if typ in (0,4,7):
            address={0:0x41a350,4:0x410270,7:0x417c60}[typ]
            args=(b,struct.unpack('<I',f(amount))[0],0x23450002,kind)
            if typ!=7:args+=(0x56780005 if typ==0 else 0x45670004,)
            want_trace.append(address);want_effects.append((address,args));want_health=after
            if typ==0:returned=7.25
        elif typ==2 or (typ==3 and amount>100):want_health=roundf(health-amount)
        want_trace.append(0x48aaf0)
        if player&255 and 0<want_health<=.5:want_health=0
        routes[typ]=routes.get(typ,0)+1
    assert trace==want_trace,(i,trace,want_trace)
    assert effects==want_effects,(i,effects,want_effects)
    assert bytes(u.mem_read(b+0x7c,4))==w(want_flags),i
    assert bytes(u.mem_read(b+0x34,4))==f(want_health),i
    assert bytes(u.mem_read(b+0x2000,4))==f(returned),i
report=dict(result='PASS',cases=8192,accepted=accepted,routes=routes,difficulty_multipliers=table,minimum_damage=minimum,original_sha256=digest,scope='Complete original4892c0 in SP with64ecb9/ba and6fc4d8 zero. Object/entity lookup, immunity/player predicates and delegated effects supplied. Exact eligibility call order, low-byte force, pre-rejection flag mutation, difficulty scaling except kind9, object dispatch, delegated argument forwarding, return value and post-effect health cleanup. No multiplayer or actual delegated damage lifecycle; this harness executes original code only.')
(root/'artifacts/damage-wrapper-trace.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
