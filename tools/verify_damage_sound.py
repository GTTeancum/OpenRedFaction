"""Complete original4196f0 routing vs PC and NXDK; external sound calls supplied."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda v:struct.pack('<f',v)
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);return m
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(original);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_entity_damage_sound\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
trace=[];current=None
def hook(m,address,size,context):
    native=context
    addresses=(b+0x3000,b+0x3010,b+0x3020) if native else (0x434da0,0x505c00,0x48a9c0,0x427020,0x42a8e0)
    if address not in addresses:return
    kind=addresses.index(address);sp=m.reg_read(UC_X86_REG_ESP)
    args=struct.unpack('<8I',m.mem_read(sp,32));result=0
    if kind in (0,1):
        trace.append(w(kind,args[2 if native else 1])+bytes(24));result=current[16+kind]
    elif kind==2:
        if native:row=w(2)+bytes(m.mem_read(args[2],12))+w(args[3],0x3f800000,0,0)
        else:
            assert args[1]==b and args[6:8]==(0x3f800000,0),args
            row=w(2,*args[2:8],0)
        trace.append(row);at=b+(4 if native else 0x810)
        flags=struct.unpack('<I',m.mem_read(at,4))[0];m.mem_write(at,w(flags^current[18]))
    else:result=current[13+kind-3]
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,args[0])
u.hook_add(UC_HOOK_CODE,hook,False);x.hook_add(UC_HOOK_CODE,hook,True)
rng=random.Random(0x4196f0);cases=[];expected=[]
for i in range(4096):
    health=rng.choice([-1,-0.0,0,1,100]);flags=rng.choice([0,4,0x12340000]);desc=rng.choice([-1,0,12])
    action=rng.choice([0,1,17,18]);now=rng.choice([0,1000,1072799500,1072800000]);deadline=rng.choice([-1,0,now,now-1 if now else 1,1072800000])
    fraction=rng.choice([-.1,0,.1,.3,struct.unpack('<f',w(0x3e999999))[0],1])
    wire=f(health)+w(flags,desc,21,22,23,action,deadline,0x87654321)+f(1)+f(-2)+f(3)+f(fraction)+w(rng.choice([0,1,2,256,257]),rng.choice([0,1,256,257]),now,rng.choice([-1,0,77]),rng.choice([0,0,1,256]),rng.choice([0,4,0x2000]))
    current=struct.unpack('<19I',wire);cases.append(wire);trace=[]
    obj=bytearray(0x1500)
    for offset,data in [(0x34,wire[:4]),(0x810,wire[4:8]),(0x294,w(b+0x4000)),(0x29c,w(b+0x6000)),(0x520,wire[24:28]),(0x1458,wire[28:32]),(0x808,wire[32:36]),(0x7d4,wire[36:48])]:obj[offset:offset+len(data)]=data
    u.mem_write(b,bytes(obj));u.mem_write(b+0x4124,wire[8:12]);u.mem_write(b+0x6124,wire[12:16]);u.mem_write(b+0x616c,wire[16:24]);u.mem_write(0x5a3ed8,w(now))
    u.mem_write(stack,w(stop,b)+wire[48:52]);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x4196f0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    state=bytearray(wire[:48]);state[4:8]=u.mem_read(b+0x810,4);state[28:32]=u.mem_read(b+0x1458,4)
    assert len(trace)<=3
    expected.append(w(0)+state+w(len(trace))+b''.join(trace)+bytes((3-len(trace))*32))
for offset in (0,36,40,44,48):
    wire=bytearray(cases[0]);wire[offset:offset+4]=w(0x7fc00000);cases.append(bytes(wire));expected.append(w(-2)+wire[:48]+bytes(100))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--damage-sound'],input=b''.join(cases));assert len(actual)==len(cases)*152
for i,(wire,want) in enumerate(zip(cases,expected)):
    assert actual[i*152:(i+1)*152]==want,('PC',i,wire.hex(),actual[i*152:(i+1)*152].hex(),want.hex())
    current=struct.unpack('<19I',wire);trace=[];x.mem_write(b,wire[:48]);x.mem_write(b+0x2000,w(b+0x3000,b+0x3010,b+0x3020,0));x.mem_write(stack,w(stop,b)+wire[48:64]+w(b+0x2000))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,48))+w(len(trace))+b''.join(trace)+bytes((3-len(trace))*32)
    assert got==want,('NXDK',i,wire.hex(),got.hex(),want.hex())
report=dict(result='PASS',original_cases=4096,port_guards=5,pc_nxdk_cases=len(cases),original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete4196f0 with original timers/vector copy. Predicate, resolver, playing and play calls supplied. Exact state and ordered sound call arguments including callback flag mutation, float .3 boundary, timer wrap, suppression, missing sample and death once. No live sound loading, playback or entity lifecycle.')
(root/'artifacts/damage-sound.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
