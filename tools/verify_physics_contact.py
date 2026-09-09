"""Compare stationary non-liquid actor response with the complete original routine."""
import hashlib,json,random,re,struct,subprocess,sys,math
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
pack=lambda v:struct.pack('<%df'%len(v),*v)
words=lambda *v:struct.pack('<%dI'%len(v),*v)
impact=[];stop_before_damage=False
def observe(cpu,address,size,data):
    if address==0x49cd80:
        impact.append(bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_ESP)+8,4)))
        if stop_before_damage:cpu.emu_stop()
u.hook_add(UC_HOOK_CODE,observe)
def original(command,mode,before_damage=False):
    global stop_before_damage
    stop_before_damage=before_damage
    values=struct.unpack('<15f',command)
    u.mem_write(base,bytes(0x4000));u.mem_write(base+0x858,words(base+0x2000));u.mem_write(base+0x2004,words(mode))
    u.mem_write(base+0x294,words(base+0x3000));u.mem_write(base+0x1e4,words(0xffffffff))
    for offset,items in [(0x144,values[:3]),(0x150,values[3:6]),(0x1c0,values[6:9]),(0x8a0,values[9:12]),(0x1d8,values[12:15])]:u.mem_write(base+offset,pack(items))
    u.mem_write(stack,words(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f);impact.clear()
    u.emu_start(0x49d7e0,stop,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==(0x49cd80 if before_damage else stop) and len(impact)==1
    return bytes(u.mem_read(base+0x144,24))+impact[0]

rng=random.Random(0x49dc1d);commands=[];expected=[];guest_cases=0
for mode in (3,1):
    for case in range(256):
        normal=[rng.uniform(-1,1) for _ in range(3)];length=math.sqrt(sum(v*v for v in normal));normal=[v/length for v in normal]
        command=pack([rng.uniform(-2,2) for _ in range(6)]+normal+[rng.uniform(-.5,.5) for _ in range(6)])
        commands.append(command);expected.append(original(command,mode))
if len(sys.argv)>1:
    symbols=json.loads(Path(sys.argv[1]).read_text())['symbols']
    guest_cases=symbols['rf_scene_actor_contact_count']['words'][0]
    assert 0<guest_cases<=64
    records=symbols['rf_scene_actor_contacts']['words']
    for index in range(guest_cases):
        record=records[index*25:(index+1)*25]
        assert record[0]<63 and record[1]<10 and record[2] in (1,3)
        command=words(*record[3:18]);want=original(command,record[2],True)
        assert want==words(*record[18:25]),('Original/guest contact differs',index,record[:3],want.hex(),words(*record[18:25]).hex())
        commands.append(command);expected.append(want)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--contact'],input=b''.join(commands))
for i,want in enumerate(expected):assert actual[i*28:(i+1)*28]==want,('PC contact mismatch',i,actual[i*28:(i+1)*28].hex(),want.hex())
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_physics_static_contact\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for case,command in enumerate(commands):
    state=bytearray([0xa5]*308);state[184:208]=command[:24];state[272:276]=words(0)
    x.mem_write(base,bytes(state));x.mem_write(base+0x2000,command[24:]);x.mem_write(base+0x3000,bytes(4))
    x.mem_write(stack,words(stop,base,base+0x2000,base+0x200c,base+0x2018,base+0x3000))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    state[184:208]=expected[case][:24]
    assert bytes(x.mem_read(base,308))==state and bytes(x.mem_read(base+0x3000,4))==expected[case][24:],('NXDK contact mismatch',case)
report=dict(status='PASS',pc_cases=len(expected),nxdk_cases=len(expected),guest_cases=guest_cases,scope='Original 49d7e0, no function substitution; prepared non-liquid zero-inverse-mass contact, no object, flags 0x80 clear, non-rotating actor, modes 3 and 1. Synthetic cases execute full routine; live captured inputs stop at damage call after verifying its impact argument. Damage/gameplay effects are excluded.')
(root/'artifacts/physics-contact-verification.json').write_text(json.dumps(report,indent=2));print(report)
