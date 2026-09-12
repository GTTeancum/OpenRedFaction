"""Verify both original swept-bounds and contact preparation tails, PC and NXDK; no hooks."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_EAX
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
base=0x30000000;stack=base+0xe000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
    pe=pefile.PE(str(path));image=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
    cpu=Uc(UC_ARCH_X86,UC_MODE_32)
    cpu.mem_map(origin,(len(image)+4095)//4096*4096);cpu.mem_write(origin,image)
    cpu.mem_map(base,0x10000)
    return cpu
u=load(exe);x=load(root/'build/xbox/main.exe')
mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_physics_body_prepare_sweep\s+([0-9a-fA-F]+)',mapping)[1],16)
def compact(actor):
    b=actor[0x88:0x1f8]
    return b[:12]+b[0x10:0xfc]+b[0x108:0x128]+b[0x138:0x148]+b[0x15c:0x160]+b[0x164:0x16c]
rng=random.Random(0x49f8aa);commands=[];expected=[]
for case in range(2048):
    # Arbitrary bits deliberately cover stale fields, signed zeros and NaNs.
    seed=bytearray(rng.getrandbits(8) for _ in range(0x240))
    flags=(0,0xffffffff,0x40000000,0x20000000,0x01000000,rng.getrandbits(32))[case%6]
    seed[0x1a8:0x1ac]=pack(flags)
    values=[rng.choice((0.0,-0.0,rng.randrange(-65536,65537)/16)) for _ in range(6)]
    radius=rng.choice((0.0,-0.0,.25,1,4,-.5))
    seed[0xe4:0xfc]=struct.pack('<6f',*values);seed[0x180:0x184]=struct.pack('<f',radius)
    want=bytearray(seed)
    for i in range(3):
        a,b=values[i],values[i+3];low,high=(a,b) if a<b else (b,a)
        want[0x190+4*i:0x194+4*i]=struct.pack('<f',low-radius)
        want[0x19c+4*i:0x1a0+4*i]=struct.pack('<f',radius+high)
    want[0x168:0x180]=bytes(24)
    want[0x1cc:0x1d0]=pack(0x3f800000);want[0x1e4:0x1e8]=pack(0xffffffff)
    want[0x1a8:0x1ac]=pack(flags|0x01000000)
    for start,end in ((0x49f8aa,0x49f926),(0x49fd84,0x49fdfa)):
        u.mem_write(base,bytes(seed));u.mem_write(stack,pack(0x12345678))
        u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBP,base+0xe4)
        u.emu_start(start,end,count=10000)
        assert u.reg_read(UC_X86_REG_EIP)==end
        assert bytes(u.mem_read(base,len(seed)))==want,('original',case,hex(start))
    command=compact(seed);result=compact(want)
    assert len(command)==308
    commands.append(command);expected.append(result)
    # Guard bytes and repeat call also check owner preservation/idempotence.
    x.mem_write(base,b'\xa5'*16+command+b'\x5a'*16)
    for repeat in range(2):
        x.mem_write(stack,pack(stop,base+16));x.reg_write(UC_X86_REG_ESP,stack)
        x.emu_start(entry,stop,count=10000)
        assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0,(case,hex(x.reg_read(UC_X86_REG_EAX)),hex(x.reg_read(UC_X86_REG_EIP)))
        assert bytes(x.mem_read(base,340))==b'\xa5'*16+result+b'\x5a'*16,('NXDK',case,repeat)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--prepare-sweep'],input=b''.join(commands))
assert actual==b''.join(expected),'PC mismatch'
x.mem_write(stack,pack(stop,0));x.reg_write(UC_X86_REG_ESP,stack)
x.emu_start(entry,stop,count=10000)
assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0xfffffffc
for offset,bits in ((88,0x7fc00000),(244,0x7f800000)):
    invalid=bytearray(commands[0]);invalid[offset:offset+4]=pack(bits);x.mem_write(base,bytes(invalid));x.mem_write(stack,pack(stop,base));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(base,308))==invalid
report=dict(result='PASS' ,cases=len(commands),original_tail_executions=2*len(commands),nxdk_executions=2*len(commands),
    scope='Both original49f8aa..49f925 and49fd84..49fdf9 tails including actual539460/465ee0/465ec0 and4fad00, no hooks. Exact PC/NXDK retained body bytes, surrounding original bytes and guards; signed zeros, both motion directions, equal coordinates, finite signed radius, repeated calls and NULL/nonfinite error preservation. Movement prediction itself excluded.')
(root/'artifacts/physics-prepare-sweep-verification.json').write_text(json.dumps(report,indent=2))
print(report)
