"""Original force turbulence with actual shared CRT draws vs PC/NXDK."""
import hashlib,json,re,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
    p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(origin,(len(b)+4095)//4096*4096);m.mem_write(origin,b);m.mem_map(base,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u,x=machine(exe),machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_physics_force_turbulence\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
thread=base+0x3000;draws=0
def thread_data(cpu,address,size,data):
    global draws
    draws+=1;sp=cpu.reg_read(UC_X86_REG_ESP)
    cpu.reg_write(UC_X86_REG_EAX,thread);cpu.reg_write(UC_X86_REG_EIP,struct.unpack('<I',cpu.mem_read(sp,4))[0]);cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,thread_data,begin=0x577eef,end=0x577eef)
assert bytes(u.mem_read(0x589808,4))==f(150)
rng=random.Random(0x4869f6);commands=bytearray();expected=bytearray()
for case in range(2048):
    amount=case%16;flags=amount<<16;dt=(0.,1/60,1/30,.25,2.)[(case//16)%5]
    strength=(0.,.01,-20.,20.,10000.)[(case//80)%5]
    axis=((0,1,0),(0,-1,0),(1,0,0),(0,0,1))[case%4] if case<640 else tuple(rng.uniform(-1,1) for _ in range(3))
    influence=f(*axis,strength);seed=rng.getrandbits(32);args=influence+w(flags)+f(dt)+w(seed)
    u.mem_write(stack+0x1c,influence[:12]);u.mem_write(stack+0x50,influence[12:]);u.mem_write(thread+20,w(seed));u.mem_write(0x5a4014,f(dt))
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,flags);u.reg_write(UC_X86_REG_FPCW,0x27f);draws=0
    u.emu_start(0x4869f6,0x486a72 if amount else 0x486a9e,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==(0x486a72 if amount else 0x486a9e) and draws==(2 if amount else 0)
    result=bytes(u.mem_read(stack+0x1c,12))+influence[12:]+bytes(u.mem_read(thread+20,4))+(bytes(u.mem_read(stack+0x10,4)) if amount else f(0))
    commands.extend(args);expected.extend(result)
    x.mem_write(base,args);x.mem_write(base+0x1000,f(-100))
    x.mem_write(stack,w(stop,base,flags)+f(dt)+w(base+24,base+0x1000))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    actual=bytes(x.mem_read(base,16))+bytes(x.mem_read(base+24,4))+bytes(x.mem_read(base+0x1000,4))
    assert actual==result,(case,actual.hex(),result.hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--force-turbulence'],input=commands)
assert actual==expected,('PC differs',next((i//24,i%24) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b))
report=dict(result='PASS',cases=2048,original_sha256=digest,
 scope='Original4869f6..486a72 scale/clamp/cone/basis/rotation and actual CRT RNG. Only CRT thread pointer supplied, 53-bit x87 environment. All16 turbulence nibbles, zero/nonzero dt and strength, signed strength, clamped cones, vertical/nonunit axes. Exact PC/NXDK direction, strength, RNG and unclamped shake amplitude. Two draws whenever enabled, none disabled. Player-view shake and campaign integration excluded.')
(root/'artifacts/force-turbulence.json').write_text(json.dumps(report,indent=2));print(report)
