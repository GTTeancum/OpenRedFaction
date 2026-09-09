"""Verify prepared falling translation, original 49e8b7..49e9e6, no callee hooks."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;stack=base+0xe000;u.mem_map(base,0x10000)
pack=lambda v:struct.pack('<%df'%len(v),*v)
rng=random.Random(0x49e8b7);commands=[];expected=[]
for case in range(256):
    values=[(1,100,800)[case%3],(0,1/60,1/30,.1)[case%4],9.8]
    values += [rng.uniform(-100,100) for _ in range(3)]
    values += [rng.uniform(-5,5) for _ in range(3)]
    values += [rng.uniform(-200,200) for _ in range(3)]
    values += [rng.uniform(-2,2) for _ in range(3)]
    command=pack(values);values=struct.unpack('<15f',command)
    u.mem_write(base,bytes(0x1500));u.mem_write(stack-0x100,bytes(0x200))
    for offset,items in [(0x98,values[:1]),(0x1b0,values[1:2]),(0xe4,values[3:6]),
                         (0x144,values[6:9]),(0x168,values[9:12]),(0x8a0,values[12:15])]:u.mem_write(base+offset,pack(items))
    u.mem_write(0x5a00dc,pack(values[2:3]))
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EDI,base+0x144)
    u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(0x49e8b7,0x49e9e6,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==0x49e9e6
    commands.append(command);expected.append(bytes(u.mem_read(base+0x144,12))+bytes(u.mem_read(base+0xf0,12)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--fall'],input=b''.join(commands))
for i,want in enumerate(expected):assert actual[i*24:(i+1)*24]==want,('PC fall mismatch',i,actual[i*24:(i+1)*24].hex(),want.hex())
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_physics_fall_propose\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for case,command in enumerate(commands):
    values=struct.unpack('<15f',command);state=bytearray([0xa5]*308)
    for offset,items in [(12,values[:1]),(88,values[3:6]),(184,values[6:9]),(220,values[9:12])]:state[offset:offset+len(items)*4]=pack(items)
    x.mem_write(base,bytes(state));x.mem_write(base+0x2000,pack(values[12:15]));stop=base+0xf000
    x.mem_write(stack,struct.pack('<IIffI',stop,base,values[1],values[2],base+0x2000))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    state[184:196]=expected[case][:12];state[100:112]=expected[case][12:]
    assert bytes(x.mem_read(base,308))==state,('NXDK fall mismatch',case)
report=dict(cases=len(expected),nxdk_cases=len(expected),status='PASS',nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Prepared falling translation only; steering, movement dispatch, contact response and commit excluded.')
(root/'artifacts/physics-fall-verification.json').write_text(json.dumps(report,indent=2));print(report)
