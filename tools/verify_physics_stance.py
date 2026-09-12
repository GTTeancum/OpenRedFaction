"""Original cached stance-center switches and stand clearance preparation."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EDI,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<%dI'%len(v),*v)
f=lambda *v:struct.pack('<%df'%len(v),*v)
r=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
base=0x30000000;cls=base+0x2000;records=base+0x4000;centers=base+0x5000;owner=base+0x6000;output=base+0x7000;stack=base+0xe000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
    pe=pefile.PE(str(path));b=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
    cpu=Uc(UC_ARCH_X86,UC_MODE_32);cpu.mem_map(origin,(len(b)+4095)//4096*4096);cpu.mem_write(origin,b);cpu.mem_map(base,0x10000);return cpu
u=load(exe);x=load(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
symbol=lambda n:int(re.search(r'_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entry=symbol('rf_physics_stance_centers');endpoint=symbol('rf_physics_stand_endpoint')
def call(address,args):
    x.mem_write(stack,w(stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
    x.emu_start(address,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    return x.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4289d0);commands=[];expected=[]
for case in range(288):
    count=case%9;crouching=case//9%2;flags=rng.getrandbits(32)
    initial=rng.randbytes(192);target=f(*[rng.uniform(-2,2) for _ in range(24)])
    position=f(*[rng.uniform(-100,100) for _ in range(3)]);height=f(rng.uniform(0,2))
    seed=bytearray(rng.randbytes(0x1500));seed[0x2c:0x30]=w(0xffffffff);seed[0x3c:0x48]=position
    seed[0x184:0x190]=w(count,8,records);seed[0x294:0x298]=w(cls);seed[0x29c:0x2a0]=w(cls);seed[0x810:0x814]=w(flags)
    u.mem_write(base,bytes(seed));u.mem_write(cls,bytes(0x2000));u.mem_write(cls+0xf74,height);u.mem_write(records,initial)
    class_records=b''.join(bytes(24)+target[i*12:(i+1)*12]+w(i) for i in range(8))
    u.mem_write(cls+(0xe30 if crouching else 0xcec),w(count)+class_records)
    u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EDI,base);u.reg_write(UC_X86_REG_FPCW,0x37f)
    # Standing begins after successful clearance; stop both paths before ground query.
    finish=0x428a37 if crouching else 0x428b3e
    u.emu_start(0x4289d0 if crouching else 0x428acd,finish,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==finish
    after=bytes(u.mem_read(records,192));actual_flags=r(u,base+0x810)
    want_seed=bytearray(seed);want_seed[0x810:0x814]=w(actual_flags)
    assert bytes(u.mem_read(base,len(seed)))==want_seed,'Unexpected original actor writes'
    for i in range(8):assert after[i*24+12:(i+1)*24]==initial[i*24+12:(i+1)*24]
    # Full standing entry to the query call: inspect its actual endpoint argument.
    u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x428a60,0x428ab9,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x428ab9
    end=bytes(u.mem_read(r(u,u.reg_read(UC_X86_REG_ESP)+4),12))
    commands.append(w(count,flags,crouching)+initial+target+position+height)
    expected.append(w(0,actual_flags)+after+end)
    x.mem_write(records,initial);x.mem_write(centers,target);x.mem_write(owner,w(records,count,192));x.mem_write(output,w(flags))
    assert call(entry,[owner,centers,count,output,crouching])==0
    assert bytes(x.mem_read(records,192))==after and r(x,output)==actual_flags
    assert bytes(x.mem_read(owner,12))==w(records,count,192)
    x.mem_write(centers,position);assert call(endpoint,[centers,struct.unpack('<I',height)[0],output])==0
    assert bytes(x.mem_read(output,12))==end
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--stance'],input=b''.join(commands))
assert len(actual)==len(expected)*212
for i,want in enumerate(expected):assert actual[i*212:(i+1)*212]==want,('PC mismatch',i)
# Malformed later center must preserve earlier centers and actor flags.
x.mem_write(records,bytes([0xa5])*192);x.mem_write(owner,w(records,2,192));x.mem_write(centers,f(1,2,3,4,float('nan'),6));x.mem_write(output,w(0x12345678))
for args in ([owner,centers,2,output,1],[owner,centers,1,output,1],[owner,centers,2,output,2]):
    assert call(entry,args)!=0 and bytes(x.mem_read(records,192))==bytes([0xa5])*192 and r(x,output)==0x12345678
# Already-cached class guard must not resample model bones on later actors.
for case in range(16):
    cached=case%2;flags=(rng.getrandbits(32)&~0x40000000)|(cached*0x40000000)
    u.mem_write(base,bytes(0x1500));u.mem_write(base+0x29c,w(cls));u.mem_write(cls+0x724,w(flags))
    u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack)
    finish=stop if cached else 0x423bd0
    u.emu_start(0x423b90,finish,count=1000)
    assert u.reg_read(UC_X86_REG_EIP)==finish and r(u,cls+0x724)==flags
# Execute full original crouch through its clock store with a reentrant ground boundary.
crouch_ground_calls=0;tail_clock=tail_mode=tail_count=0
def crouch_ground(cpu,address,size,data):
    global crouch_ground_calls
    if address!=0x4a0840:return
    sp=cpu.reg_read(UC_X86_REG_ESP);assert r(cpu,sp+4)==base
    assert r(cpu,base+0x810)&0x400
    crouch_ground_calls+=1
    cpu.mem_write(base+0x7b4,w(0xdeadbeef));cpu.mem_write(0x6460f0,w(tail_clock))
    if tail_mode:
        cpu.mem_write(base+0x810,w(r(cpu,base+0x810)&~0x400))
        if tail_count:
            value=struct.unpack('<f',cpu.mem_read(records+4,4))[0];cpu.mem_write(records+4,f(value+.125))
    cpu.reg_write(UC_X86_REG_EIP,r(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
hook=u.hook_add(UC_HOOK_CODE,crouch_ground)
for case in range(128):
    tail_count=case%9;tail_mode=case%2;tail_clock=rng.getrandbits(32);flags=rng.getrandbits(32)
    initial=rng.randbytes(192);target=f(*[rng.randrange(-16,17)/8 for _ in range(24)])
    seed=bytearray(rng.randbytes(0x1500));seed[0x184:0x190]=w(tail_count,8,records);seed[0x294:0x298]=w(cls);seed[0x29c:0x2a0]=w(cls);seed[0x810:0x814]=w(flags)
    u.mem_write(base,bytes(seed));u.mem_write(cls,bytes(0x2000));u.mem_write(records,initial)
    u.mem_write(cls+0xe30,w(tail_count)+b''.join(bytes(24)+target[i*12:(i+1)*12]+w(i) for i in range(8)))
    u.mem_write(0x6460f0,w(tail_clock^0xffffffff));u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
    u.emu_start(0x4289d0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    want=bytearray(seed);want[0x810:0x814]=w((flags|0x400)&(~0x400 if tail_mode else 0xffffffff));want[0x7b4:0x7b8]=w(tail_clock)
    assert bytes(u.mem_read(base,0x1500))==want
    want_records=bytearray(initial)
    for i in range(tail_count):want_records[i*24:i*24+12]=target[i*12:(i+1)*12]
    if tail_mode and tail_count:want_records[4:8]=f(struct.unpack_from('<f',want_records,4)[0]+.125)
    assert bytes(u.mem_read(records,192))==want_records
assert crouch_ground_calls==128;u.hook_del(hook)
report=dict(status='PASS',pc_cases=288,nxdk_cases=288,guard_cases=3,class_cache_guard_cases=16,original_crouch_tail_cases=128,scope='Original crouch entry and standing successful-clearance suffix before ground refresh, all callees unchanged; complete actor storage and sphere records checked. Original standing entry through clearance call verifies endpoint. Additional128 full original crouch calls supply a reentrant ground boundary and verify the later raw6460f0-to7b4 store plus preserved callback mutations. Cache construction, actual ground effects and gameplay scheduling excluded.')
(root/'artifacts/physics-stance-verification.json').write_text(json.dumps(report,indent=2));print(report)
