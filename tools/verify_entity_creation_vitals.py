"""Original factory armor/health assignment blocks versus C on PC and NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EIP,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));image=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(image)+4095)//4096*4096);m.mem_write(base,image);m.mem_map(b,65536);return m
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_entity_creation_vitals\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x422cb4);commands=[];expected=[]
healths=[0,0x80000000,1,0x80000001,0x3f800000,0xbf800000,0x42c80000,0x7f800000,0xff800000,0x7fc12345,0x7f812345,0xffc12345]
for case in range(1536):
 values=[rng.getrandbits(32) for _ in range(8)]
 values[2]=[0,4,0xffffffff,rng.getrandbits(32)][case%4]
 values[4]=healths[case%len(healths)] if case<768 else rng.getrandbits(32)
 values[5]=healths[(case//3)%len(healths)]
 values[7]=[0,1,2,255,256,257,0xffffff00,0xffffffff][case%8]
 raw=w(*values);commands.append(raw)
 actor=bytearray(b'\xa5'*0x1500);actor[0x34:0x3c]=raw[:8];actor[0x7c:0x80]=raw[8:12];actor[0x840:0x844]=raw[12:16];actor[0x294:0x298]=w(b+0x4000)
 u.mem_write(b,bytes(actor));u.mem_write(b+0x4044,raw[16:24]);u.mem_write(b+0x4764,raw[24:28]);u.mem_write(0x64ecb9,raw[28:29])
 u.reg_write(UC_X86_REG_ESI,b);u.reg_write(UC_X86_REG_EDI,b+0x4000);u.reg_write(UC_X86_REG_FPCW,0x27f);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x422a80,0x422a9e,count=100);assert u.reg_read(UC_X86_REG_EIP)==0x422a9e
 u.emu_start(0x422cb4,0x422cea,count=100);assert u.reg_read(UC_X86_REG_EIP)==0x422cea
 after=bytes(u.mem_read(b,len(actor)));want=after[0x34:0x3c]+after[0x7c:0x80]+after[0x840:0x844]
 for start,size in [(0x34,8),(0x7c,4),(0x840,4)]:actor[start:start+size]=after[start:start+size]
 assert bytes(actor)==after;expected.append(want)
 x.mem_write(b+0x2000,raw);x.mem_write(stack,w(stop,b+0x2000,b+0x2010,values[7]));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert bytes(x.mem_read(b+0x2000,16))==want,('NXDK',case,values)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--creation-vitals'],input=b''.join(commands))
assert actual==b''.join(expected)
report=dict(result='PASS',cases=len(commands),original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),
 scope='Actual422a80..422a9e and422cb4..422cea numeric factory blocks. Exact PC/NXDK health, armor, object flags and opaque840 assignment; unchanged original actor bytes elsewhere. Signed zero, negative/positive health, infinities, quiet/signaling NaNs and low-byte network gates. Excludes intervening factory calls, class parser, allocation, later mutations and live NPC construction.')
(root/'artifacts/entity-creation-vitals.json').write_text(json.dumps(report,indent=2));print(report)
