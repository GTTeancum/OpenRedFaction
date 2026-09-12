"""Check retained contact ownership against the original actor byte layout."""
import json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
base=0x30000000;stack=base+0xe000;stop=base+0xf000
p=pefile.PE(str(root/'build/xbox/main.exe'));data=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(data)+4095)//4096*4096);x.mem_write(origin,data);x.mem_map(base,65536)
mapping=(root/'build/xbox/main.map').read_text()
symbol=lambda name:int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
read=symbol('rf_collision_contact_read');write=symbol('rf_collision_contact_write')
def call(entry,args):
    x.mem_write(stack,struct.pack('<4I',stop,*args));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
    return x.reg_read(UC_X86_REG_EAX)
def retained(actor):
    b=actor[0x88:0x1f8]
    body=b[:12]+b[0x10:0xfc]+b[0x108:0x128]+b[0x138:0x148]+b[0x15c:0x160]+b[0x164:0x16c]
    extra=actor[0x1b4:0x1c0]+actor[0x1d0:0x1e4]+actor[0x1e8:0x1ec]+actor[0x1f4:0x1f8]
    assert len(body)==308 and len(extra)==40
    return body+extra
rng=random.Random(0x1b41f7);commands=[];expected=[]
for case in range(4096):
    actor=bytearray(rng.getrandbits(8) for _ in range(0x240))
    source=bytes(rng.getrandbits(8) for _ in range(68))
    seed=retained(actor);gathered=bytes(actor[0x1b4:0x1f8])
    actor[0x1b4:0x1f8]=source;want=retained(actor)
    commands.append(seed+source);expected.append(gathered+want)
    # Disjoint guarded body, extra, source and destination storage.
    x.mem_write(base,b'\xa5'*16+seed[:308]+b'\x5a'*16)
    x.mem_write(base+0x1000,b'\x35'*16+seed[308:]+b'\x53'*16)
    x.mem_write(base+0x2000,source);x.mem_write(base+0x3000,b'\x67'*100)
    assert call(read,[base+16,base+0x1010,base+0x3010])==0
    assert bytes(x.mem_read(base+0x3000,100))==b'\x67'*16+gathered+b'\x67'*16
    assert bytes(x.mem_read(base,340))==b'\xa5'*16+seed[:308]+b'\x5a'*16
    assert bytes(x.mem_read(base+0x1000,72))==b'\x35'*16+seed[308:]+b'\x53'*16
    assert call(write,[base+16,base+0x1010,base+0x2000])==0
    assert bytes(x.mem_read(base,340))==b'\xa5'*16+want[:308]+b'\x5a'*16
    assert bytes(x.mem_read(base+0x1000,72))==b'\x35'*16+want[308:]+b'\x53'*16
    assert bytes(x.mem_read(base+0x2000,68))==source
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--contact-storage'],input=b''.join(commands))
assert actual==b''.join(expected),'PC mapping mismatch'
for entry in (read,write):
    for missing in range(3):
        args=[base+16,base+0x1010,base+0x2000];args[missing]=0
        before=bytes(x.mem_read(base,0x4000))
        assert call(entry,args)==0xfffffffc
        assert bytes(x.mem_read(base,0x4000))==before
report=dict(result='PASS',pc_nxdk_cases=len(commands),null_preservation_cases=6,extra_bytes=40,
    scope='Independent original actor-offset gather/scatter mapping, all 308 body and 40 extension bytes; arbitrary bits including NaNs preserved, guard bytes and source lifetime unchanged. Original offsets established by response verifiers. No original routine represents this new storage adapter, and no live actor allocation, scheduling or XEMU execution is claimed.')
(root/'artifacts/collision-contact-storage.json').write_text(json.dumps(report,indent=2));print(report)
