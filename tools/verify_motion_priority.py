"""Verify selector priority branches with unmodified entity lookup and predicates."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
entity,stack,stop=0x30000000,0x30100000,0x30200000
for address in (entity,stack,stop):u.mem_map(address,65536)
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
fallthrough=0x41f5ae
u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:uc.emu_stop(),begin=fallthrough,end=fallthrough)
rng=random.Random(0x41f400);cases=[]
for k in range(5000):
    duration=rng.choice([0,.25,1]);age=duration*rng.choice([0,.25,.5,.75,1])
    c=struct.pack('<iiffiI',rng.randrange(23),rng.randrange(23) if duration else -1,duration,age,0,0)
    m=[rng.choice([-1]+list(range(32))) for _ in range(23)]
    fields=[rng.choice([-1]*8+[-2,0,9,14,20,23]),rng.choice([0,0x400,0x2000000]),rng.choice([0,0x8000]),rng.randrange(10),rng.randrange(5),rng.choice([-1,0]),rng.randrange(5),rng.choice([0,0x400000]),rng.choice([5,7]),5]
    velocity=rng.choice([(0,0,0),(.01,0,0),(.02,0,0),(.006,.008,0),(0,.01,0)])
    priority=struct.pack('<10i3fI',*fields,*velocity,rng.randrange(2))
    cases.append((c,m,priority))
# Force speed branch with increasingly small orthogonal velocity components.
for exponent in range(-15,-5):
    cases.append((struct.pack('<iiffiI',0,-1,0,0,0,0),list(range(23)),struct.pack('<10i3fI',-1,0x400,0x8000,0,0,0,0,0,7,5,.01,10.0**exponent,0,0)))
wire=b''.join(c+struct.pack('<23i',*m)+v for c,m,v in cases)
out=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--priority'],input=wire,capture_output=True,check=True).stdout
assert len(out)==32*len(cases)
for k,(c,m,v) in enumerate(cases):
    forced,flags,physics,mode,kind,action,linkkind,linkflags,occupant,handle=struct.unpack_from('<10i',v)
    linked,=struct.unpack_from('<I',v,52)
    link,info,linkinfo,modeptr,seats,seat= [entity+i*4096 for i in range(2,8)]
    u.mem_write(entity+0x138c,c[:16]);put(entity+0x834,'<i',forced);put(entity+0x810,'<I',flags);put(entity+0x1a8,'<I',physics)
    put(entity+0x24,'<I',0);put(entity+0x294,'<I',info);put(info+0x1b4,'<i',kind);put(entity+0x1380,'<i',action)
    put(entity+0x858,'<I',modeptr);put(modeptr+4,'<i',mode);put(entity+0x2c,'<i',handle);u.mem_write(entity+0x144,v[40:52])
    put(entity+0x200,'<i',6 if linked else -1);put(0x7394cc+6*4,'<I',link)
    put(link+0x2c,'<i',6);put(link+0x24,'<I',0);put(link+0x294,'<I',linkinfo);put(linkinfo+0x1b4,'<i',linkkind);put(linkinfo+0x724,'<I',linkflags)
    put(link+0x8d4,'<I',seats);put(seats,'<I',seat);put(seat+4,'<i',occupant)
    for i,mid in enumerate(m):put(entity+0x8e4+16*i,'<i',mid)
    put(stack+64000,'<II',stop,entity);u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x41f400,stop,count=100000);eip=u.reg_read(UC_X86_REG_EIP);assert eip in (stop,fallthrough)
    expected=struct.pack('<ii',0,int(eip==stop))+bytes(u.mem_read(entity+0x138c,16))+c[16:]
    actual=out[k*32:(k+1)*32]
    assert actual==expected,(k,actual.hex(),expected.hex())
report=dict(result='PASS',cases=len(cases),scope='Original priority prefix 0x41f400..0x41f5ad; unmodified handle lookup, entity predicates, velocity magnitude and state controls; observation stops at fallthrough without replacing callees')
(root/'artifacts/motion-priority-verification.json').write_text(json.dumps(report,indent=2));print(report)
