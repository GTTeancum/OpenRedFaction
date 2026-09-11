"""Character constructor playback projection versus PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,0x20000);m.mem_map(0,0x10000);return m
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_motion_playback_initialize\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x51ae90);payload=[];expected=[]
for index in range(128):
 raw=rng.randbytes(260);payload.append(raw)
 u.mem_write(b,rng.randbytes(0x1d5c));u.mem_write(0x181bdb8,w(0));u.mem_write(stack,w(stop,b+0x4000));u.reg_write(UC_X86_REG_ECX,b);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x51ae90,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 read=lambda off,n:bytes(u.mem_read(b+off,n))
 want=read(0x12d0,196)+read(0x1cfc,8)+read(0x1d48,4)+w(read(0x1d4c,1)[0],read(0x1d14,1)[0])+read(0x1d18,32)+read(0x1d04,4)+w(struct.unpack('<H',read(0x1cf8,2))[0],read(0x1d44,1)[0]|read(0x1d45,1)[0]<<1)
 assert len(want)==260;expected.append(want)
 x.mem_write(b,raw);x.mem_write(stack,w(stop,b));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and bytes(x.mem_read(b,260))==want
pc=subprocess.check_output([str(root/'build/pc/Release/rf_motion_probe.exe'),'--initialize'],input=b''.join(payload));assert pc==b''.join(expected)
report=dict(result='PASS',cases=len(payload),original_sha256=digest,scope='Full original51ae90 executes with real constructor/vector callees and empty instance-list root, no function stubs. Compare compact playback projection to PC/NXDK from random previous bytes. Full original list ownership and bone caches are not ported by this helper.')
(root/'artifacts/motion-initialize.json').write_text(json.dumps(report,indent=2));print(report)
