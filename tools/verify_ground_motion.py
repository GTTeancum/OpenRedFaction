"""Compare prepared non-run linear motion with original 49f7c3..49f89f."""
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
f=lambda *v:struct.pack('<%df'%len(v),*v)
w=lambda *v:struct.pack('<%dI'%len(v),*v)
rng=random.Random(0x49f7c3);commands=[];expected=[]
for case in range(256):
    values=[(1,100,.1,10)[case%4],(0,1/60,1/30,.1)[case%4],(0,.5,2,10)[case%4]]
    values += [rng.uniform(-100,100) for _ in range(3)]
    values += [rng.uniform(-5,5) for _ in range(3)]
    values += [rng.uniform(-10,10) for _ in range(3)]
    values += [0 if case%2 else rng.uniform(-3,3) for _ in range(3)]
    values += [rng.uniform(-1,1) for _ in range(3)]
    command=f(*values);v=struct.unpack('<18f',command);commands.append(command)
    u.mem_write(base,bytes(0x1500));u.mem_write(stack,bytes(0x100))
    for offset,data in [(0x98,v[:1]),(0x1b0,v[1:2]),(0xe4,v[3:6]),(0x144,v[6:9]),(0x168,v[9:12]),(0x8a0,v[15:18])]:u.mem_write(base+offset,f(*data))
    u.mem_write(stack+0xc,f(v[2]));u.mem_write(stack+0x10,f(*v[12:15]))
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x49f7c3,0x49f89f,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x49f89f
    expected.append(bytes(u.mem_read(base+0x144,12))+bytes(u.mem_read(base+0xf0,12)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--ground-motion'],input=b''.join(commands))
assert actual==b''.join(expected),'Shared grounded proposal differs from original'
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_physics_ground_propose\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for case,command in enumerate(commands):
    state=bytearray(308);state[12:16]=command[:4];state[88:100]=command[12:24];state[184:196]=command[24:36];state[220:232]=command[36:48]
    x.mem_write(base,bytes(state));x.mem_write(base+0x2000,command[48:72])
    x.mem_write(stack,w(stop,base)+command[4:12]+w(base+0x2000,base+0x200c))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
    x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    state[184:196]=expected[case][:12];state[100:112]=expected[case][12:]
    assert bytes(x.mem_read(base,308))==state,('NXDK grounded proposal differs',case)
report=dict(status='PASS',pc_cases=256,nxdk_cases=256,scope='Prepared acceleration/drag grounded block with unchanged original callees; nonzero force, velocity, steering and support velocity, including zero steering for repeated-pass preparation. No steering transform/drag selection or collision policy.')
(root/'artifacts/ground-motion-verification.json').write_text(json.dumps(report,indent=2));print(report)
