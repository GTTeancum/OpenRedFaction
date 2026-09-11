"""Original42cca0 low-byte result vs shared PC/NXDK armor immunity."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);return m
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(original);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_entity_armor_immunity\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x42cca0);cases=[];expected=[]
for i in range(4096):
    armor=[0,0x80000000,0x3f800000,0xbf800000,0x7f800000,0xff800000,0x7fc00000,1][i%8]
    classflags=rng.getrandbits(32);flags=rng.getrandbits(32);wire=w(armor,classflags,flags);cases.append(wire)
    u.mem_write(b+0x38,w(armor));u.mem_write(b+0x294,w(b+0x4000));u.mem_write(b+0x4724,w(classflags));u.mem_write(b+0x814,w(flags))
    u.mem_write(stack,w(stop,b));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x42cca0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected.append(w(u.reg_read(UC_X86_REG_EAX)&255))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--armor-immunity'],input=b''.join(cases));assert actual==b''.join(expected)
for i,(wire,want) in enumerate(zip(cases,expected)):
    x.mem_write(stack,w(stop)+wire);x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert w(x.reg_read(UC_X86_REG_EAX))==want,i
report=dict(result='PASS',cases=len(cases),original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete unmodified42cca0 for present entity, normalized low byte compared on PC/NXDK. Random flag words, positive/negative armor, signed zero, minimum subnormal, infinities and quiet NaN. No entity creation/ownership or live immunity dispatch.')
(root/'artifacts/armor-immunity.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
