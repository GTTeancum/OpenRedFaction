"""Verify original writer of the class word read by death clearance."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_FPCW,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);b=0x30000000;u.mem_map(b,4096)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
factor=struct.unpack_from('<f',im,0x589428-base)[0]
assert im[0x595100-base:].split(b'\0',1)[0]==b'$Min Relative Eye PHB:'
rng=random.Random(0x41bdc1);enabled=0;examples=[]
for case in range(4096):
    degrees=struct.unpack('<3f',f(*(rng.choice((0,0,90,-90,180,-180,rng.uniform(-180,180))) for _ in range(3))))
    body=bytearray(rng.randbytes(256));body[0x6c:0x78]=f(*degrees);u.mem_write(b,bytes(body))
    u.reg_write(UC_X86_REG_ESP,b+0xf00);u.reg_write(UC_X86_REG_EBP,b);u.reg_write(UC_X86_REG_EDI,b+0x6c);u.reg_write(UC_X86_REG_FPCW,0x27f)
    u.emu_start(0x41bd9b,0x41bdc4,count=100);assert u.reg_read(UC_X86_REG_EIP)==0x41bdc4
    result=f(*(v*factor for v in degrees));body[0x6c:0x78]=result
    assert bytes(u.mem_read(b,256))==body
    word=struct.unpack('<I',result[8:12])[0];enabled+=bool(word&4)
    if len(examples)<8 and (case<4 or word&4):examples.append(dict(bank_degrees=degrees[2],word_74=hex(word),clearance_bit=bool(word&4)))
assert 0<enabled<4096
report=dict(result='PASS',cases=4096,clearance_bit_set=enabled,examples=examples,original_sha256=sha,scope='Original41bd9b..41bdc1 converts parsed minimum eye PHB degrees to radians at class6c/70/74. Exact float stores and untouched class bytes. Input parse boundary supplied. Original420ec9 reads low byte of this same74 word with mask4; it is not724/728 physics flags. No whole class parser or live owner integration.')
(root/'artifacts/death-class-word.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
