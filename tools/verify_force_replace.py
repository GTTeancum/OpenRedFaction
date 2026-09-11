"""Original replacement force, fall and sound ordering vs PC/NXDK."""
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
entry=int(re.search(r'_rf_player_force_replace\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
events=[];xevents=[];callback=base+0xc000;x.mem_write(callback,b'\xc3')
def original_sound(cpu,address,size,data):
    sp=cpu.reg_read(UC_X86_REG_ESP);args=bytes(cpu.mem_read(sp+4,32))
    assert struct.unpack_from('<I',args)[0]==base
    assert args[4:16]==position
    slotptr,count,volume,pan=struct.unpack_from('<4I',args,16)
    assert bytes(cpu.mem_read(slotptr,4))==w(0x53) and count==0 and volume==0x3f800000 and pan==0
    events.append(bytes(cpu.mem_read(base+0x144,12))+bytes(cpu.mem_read(base+0x1a8,4))+
                  bytes(cpu.mem_read(base+0x1488,4))+bytes(cpu.mem_read(base+0x858,8)))
    cpu.reg_write(UC_X86_REG_EIP,struct.unpack('<I',cpu.mem_read(sp,4))[0]);cpu.reg_write(UC_X86_REG_ESP,sp+4)
def shared_sound(cpu,address,size,data):
    sp=cpu.reg_read(UC_X86_REG_ESP);context,state,pos,slot=struct.unpack('<4I',cpu.mem_read(sp+4,16))
    assert context==123 and state==base and slot==0x53 and bytes(cpu.mem_read(pos,12))==position
    xevents.append(bytes(cpu.mem_read(state,28)))
u.hook_add(UC_HOOK_CODE,original_sound,begin=0x48a930,end=0x48a930)
x.hook_add(UC_HOOK_CODE,shared_sound,begin=callback,end=callback)
rng=random.Random(0x486ab0);commands=bytearray();expected=bytearray()
for case in range(512):
    influence=f(*(rng.uniform(-1,1) for _ in range(3)),rng.uniform(-30,30))
    position=f(*(rng.uniform(-100,100) for _ in range(3)));speed=(0.,5.,30.)[case%3]
    classflags=rng.getrandbits(32);enabled=(0,1,255,256)[case%4];flags=rng.getrandbits(32);cap=f(rng.uniform(0,100))
    actor=bytearray(rng.randbytes(0x1500));actor[0x294:0x298]=w(base+0x2000);actor[0xe4:0xf0]=position
    actor[0x1a8:0x1ac]=w(flags);actor[0x1488:0x148c]=cap
    u.mem_write(base,bytes(actor));u.mem_write(base+0x2050,f(speed));u.mem_write(base+0x2724,w(classflags))
    for i in range(16):u.mem_write(0x62fe50+i*32,w(enabled,i,0,0,0,0,0,0))
    u.mem_write(stack+0x1c,influence[:12]);u.mem_write(stack+0x50,influence[12:]);u.mem_write(0x630050,w(77))
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBX,base)
    u.reg_write(UC_X86_REG_FPCW,0x37f);events.clear();u.emu_start(0x486ab0,0x486c1c,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==0x486c1c
    selected=struct.unpack('<I',u.mem_read(0x630050,4))[0]
    result=bytes(u.mem_read(base+0x144,12))+bytes(u.mem_read(base+0x1a8,4))+bytes(u.mem_read(base+0x1488,4))
    pointers=bytes(u.mem_read(base+0x858,8));assert pointers==w(0x62fe50+selected*32,0x73a858)
    for off,size in [(0x144,12),(0x1a8,4),(0x1488,4),(0x858,8)]:actor[off:off+size]=u.mem_read(base+off,size)
    assert bytes(u.mem_read(base,len(actor)))==actor
    assert len(events)==(0 if flags&0x200000 else 1)
    observation=w(1)+events[0][:20]+w(selected) if events else bytes(28)
    if events:assert events[0][12:20]==w(flags|1)+cap and events[0][20:]==pointers
    commands.extend(influence+position+f(speed)+w(classflags,enabled,flags)+cap)
    expected.extend(result+w(selected,1)+observation)
    table=base+0x3000;identity=base+0x4000;inp=base+0x1000;selection=base+0x5000
    for i in range(16):x.mem_write(table+i*32,w(enabled,i,0,0,0,0,0,0))
    x.mem_write(base,bytes(12)+w(flags)+cap+w(0,0));x.mem_write(selection,w(77))
    x.mem_write(inp,influence+position+f(speed)+w(classflags,table,identity))
    x.mem_write(stack,w(stop,base,inp,selection,callback,123));x.reg_write(UC_X86_REG_ESP,stack)
    x.reg_write(UC_X86_REG_FPCW,0x27f);xevents.clear();x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    assert bytes(x.mem_read(base,28))==result+w(table+selected*32,identity),case
    assert bytes(x.mem_read(selection,4))==w(selected)
    assert len(xevents)==len(events)
    if events:assert xevents[0]==events[0][:20]+w(table+selected*32,identity)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--force-replace'],input=commands)
assert actual==expected,('PC differs',len(actual),len(expected))
report=dict(result='PASS',cases=512,original_sha256=digest,
 scope='Original486ab0..486c1c retains velocity math, fall transition, class predicate, descriptor lookup and cap; only48a930 audio boundary supplied. Whole actor preservation and first-entry callback state/arguments/order verified. Exact PC/NXDK final and callback state, enabled-byte fallback, alternate class flag and preexisting force flag. No audio playback, selection/eligibility/rotation or campaign integration.')
(root/'artifacts/force-replace.json').write_text(json.dumps(report,indent=2));print(report)
