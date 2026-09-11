"""Original view-shake activation including registry and timer vs PC/NXDK."""
import hashlib,json,re,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
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
entry=int(re.search(r'_rf_camera_effect_start\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x40e0b0);commands=bytearray();expected=bytearray();period=1072800000
u.mem_write(0x7394cc,w(base));u.mem_write(base+0x2004,w(base+0x3000));u.mem_write(base+0x3014,w(0x70000))
for case in range(1024):
    duration=(.05,0.,-.05,.0019,-.0019,1.,10.)[case%7]
    now=(0,1,period-50,period-1,period)[(case//7)%5];strength=rng.uniform(-20,20)
    actor=bytearray(rng.randbytes(0x1500));actor[0x24:0x28]=w(0);actor[0x2c:0x30]=w(0x70000)
    u.mem_write(base,bytes(actor));u.mem_write(0x5a3ed8,w(now))
    u.mem_write(stack,w(stop,base+0x2000)+f(strength,duration));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
    u.emu_start(0x40e0b0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    result=bytes(u.mem_read(base+0x8b4,12));actor[0x8b4:0x8c0]=result
    assert bytes(u.mem_read(base,len(actor)))==actor
    commands.extend(f(strength,duration)+w(now));expected.extend(result)
    x.mem_write(base,bytes([0xa5])*12);x.mem_write(stack,w(stop,base)+f(strength,duration)+w(now))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    assert bytes(x.mem_read(base,12))==result,case
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--camera-start'],input=commands)
assert actual==expected,'PC differs'
report=dict(result='PASS',cases=1024,original_sha256=digest,
 scope='Complete original40e0b0 with unchanged player/actor registry lookup, ftol conversion and timer setter. Prepared view/player/actor links; only actor+8b4..8bf changes. Exact PC/NXDK signed strength/duration, truncation, zero duration and deadline wrapping. No missing-owner path, camera application or campaign integration.')
(root/'artifacts/camera-start.json').write_text(json.dumps(report,indent=2));print(report)
