"""Verify contact translation and time clipping against 49ffd2..4a007c."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
f=lambda v:struct.pack('<%df'%len(v),*v)
w=lambda *v:struct.pack('<%dI'%len(v),*v)
rng=random.Random(0x4a007c);commands=[];expected=[]
for case in range(256):
    position=[rng.uniform(-100,100) for _ in range(3)]
    next_pos=[v+rng.uniform(-1,1)*(.01 if case%3==0 else 1) for v in position]
    values=[(0,1/60,1/30,.1)[case%4],(0,.12988822,.5,.999)[case%4],*position,*next_pos]
    flags=0x400000 if case%2 else 0
    command=f(values)+w(flags);values=struct.unpack('<8f',command[:32]);commands.append(command)
    u.mem_write(base,bytes(0x1500));u.mem_write(stack,bytes(0x100))
    for offset,items in [(0x1b0,values[:1]),(0x1cc,values[1:2]),(0xe4,values[2:5]),(0xf0,values[5:8])]:u.mem_write(base+offset,f(items))
    u.mem_write(base+0x1a8,w(flags));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x49ffd2,0x4a007c,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x4a007c
    expected.append(bytes(u.mem_read(base+0xe4,12))+bytes(u.mem_read(base+0x1cc,4))+bytes(u.mem_read(stack+0x58,4)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--advance'],input=b''.join(commands))
for i,want in enumerate(expected):assert actual[i*20:(i+1)*20]==want,('PC advance mismatch',i,actual[i*20:(i+1)*20].hex(),want.hex())
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_physics_contact_advance\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for case,command in enumerate(commands):
    state=bytearray([0xa5]*308);state[88:112]=command[8:32];state[272:276]=command[32:36]
    x.mem_write(base,bytes(state));x.mem_write(base+0x2000,bytes(4))
    x.mem_write(stack,w(stop,base)+command[:8]+w(base+0x2000))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    state[88:100]=expected[case][:12];state[292:296]=expected[case][12:16]
    assert bytes(x.mem_read(base,308))==state and bytes(x.mem_read(base+0x2000,4))==expected[case][16:],('NXDK advance mismatch',case)
report=dict(status='PASS',pc_cases=len(expected),nxdk_cases=len(expected),scope='Original contact translation block and unchanged callees; fraction below 1, positive displacement, with/without support flag. No response, orientation, room commit or continued substeps.')
(root/'artifacts/physics-advance-verification.json').write_text(json.dumps(report,indent=2));print(report)
