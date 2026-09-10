"""Compare run proposals through original 49f646 dispatch and 49e400."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;stack=base+0xe000;u.mem_map(base,0x10000)
f=lambda *v:struct.pack('<%df'%len(v),*v)
w=lambda *v:struct.pack('<%dI'%len(v),*v)
seen=[]
def observe(cpu,address,size,data):
    if address==0x49e400:seen.append(address)
u.hook_add(UC_HOOK_CODE,observe)
climb='--climb' in sys.argv
rng=random.Random(0x49e400);commands=[];expected=[]
for case in range(384):
    state=bytearray(308);state[12:16]=f(100);state[88:100]=f(*[rng.uniform(-20,20) for _ in range(3)])
    state[184:196]=f(*[rng.uniform(-6,6) for _ in range(3)]);state[220:232]=f(*[rng.uniform(-2,2) for _ in range(3)])
    state[272:276]=w(0x1000078 if case%4==0 else 0x78)
    n=[rng.uniform(-1,1),rng.uniform(.5,1),rng.uniform(-1,1)];length=math.sqrt(sum(v*v for v in n));n=[v/length for v in n]
    if case%11==0:n=[1,0,0]
    if climb:n=[0,0,0]
    values=f(rng.choice([1/60,1/30,.001]),rng.choice([3,6]),20,1 if climb else rng.choice([.2,1,2]),rng.uniform(-1,1),rng.uniform(-1,1) if climb else 0,rng.uniform(-1,1),*n,*[rng.uniform(-.5,.5) for _ in range(3)])
    u.mem_write(base,bytes(0x6000));u.mem_write(stack,bytes(256));u.mem_write(base+0x858,w(base+0x2000));u.mem_write(base+0x2000,w(1,2 if climb else 1,2,2 if climb else 0,2,1,3,0));u.mem_write(base+0xfc,f(1,0,0,0,1,0,0,0,1));u.mem_write(base+0x294,w(base+0x3000))
    for dst,src,size in [(0x98,12,4),(0xe4,88,12),(0x144,184,12),(0x168,220,12),(0x1a8,272,4)]:u.mem_write(base+dst,bytes(state[src:src+size]))
    for dst,src,size in [(0x1b0,0,4),(0x8c0,4,4),(0x305c,8,4),(0x714,16,12),(0x1c0,28,12),(0x8a0,40,12)]:u.mem_write(base+dst,values[src:src+size])
    u.mem_write(0x649f60,values[12:16]);u.mem_write(0x64ecb9,b'\0');u.mem_write(0x7c6ec8,f(0,1,0));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_FPCW,0x37f);seen.clear()
    u.emu_start(0x49f646,0x49f8aa,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x49f8aa and seen==[0x49e400]
    commands.append(bytes(state)+values);state[184:196]=u.mem_read(base+0x144,12);state[100:112]=u.mem_read(base+0xf0,12);expected.append(bytes(state))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--climb' if climb else '--run'],input=b''.join(commands))
assert len(actual)==len(expected)*308
for i,want in enumerate(expected):
    got=actual[i*308:(i+1)*308]
    assert got==want,('run mismatch',i,[(j,struct.unpack('<f',got[j:j+4])[0],struct.unpack('<f',want[j:j+4])[0]) for j in range(0,308,4) if got[j:j+4]!=want[j:j+4]])
pe=pefile.PE(str(root/'build/xbox/main.exe'));xb=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_physics_'+('climb' if climb else 'run')+r'_propose\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16);stop=base+0xf000
for i,command in enumerate(commands):
    x.mem_write(base,command[:308]);x.mem_write(base+0x3000,command[308:]);args=struct.unpack('<4I',command[308:324])
    x.mem_write(stack,w(stop,base,*args[:3],base+0x3010,base+0x3028) if climb else w(stop,base,*args,base+0x3010,base+0x301c,base+0x3028));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    assert bytes(x.mem_read(base,308))==expected[i],('NXDK run mismatch',i,[(j,struct.unpack('<f',bytes(x.mem_read(base+j,4)))[0],struct.unpack('<f',expected[i][j:j+4])[0]) for j in range(0,308,4) if bytes(x.mem_read(base+j,4))!=expected[i][j:j+4]])
report=dict(nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),pc_sha256=hashlib.sha256((root/'build/pc/Release/rf_physics_probe.exe').read_bytes()).hexdigest(),mode='climb' if climb else 'run',status='PASS',pc_cases=len(expected),nxdk_cases=len(expected),scope=('Original 49f646 selects descriptor 2/49e400 with full 3-axis prepared input; no surface projection or traction scaling. Original callees unchanged. Compares shared climb proposal with extended speed/acceleration ratio; not full climb movement/input transform integration.' if climb else 'Original 49f646 selects 49e400 for descriptor 1 on every case; complete callees with prepared identity body/input and surface traction. Slopes, force, support and repeated passes included.'))
(root/('artifacts/climb-motion-verification.json' if climb else 'artifacts/run-motion-verification.json')).write_text(json.dumps(report,indent=2));print(report)
