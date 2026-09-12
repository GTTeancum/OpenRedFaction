"""Exact4ace90 byte result and4ad8a0 selective player-state clear, PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*v);b=0x30000000;stack=b+0xe000;stop=b+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);return m
u=machine(original);x=machine(root/'build/xbox/main.exe');symbols=(root/'build/xbox/main.map').read_text()
entries=[int(re.search(r'\s_rf_player_mode_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16) for name in ('active','stop')]
def call(m,entry,arg):
 m.mem_write(stack,w(stop,arg));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(entry,stop,count=1000);assert m.reg_read(UC_X86_REG_EIP)==stop
 return m.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4ad8a0);commands=[];expected=[]
for i in range(1024):
 body=rng.randbytes(0x1200);payload=bytes([i%256])+body[0xf95:0xf9c];body=body[:0xf94]+payload+body[0xf9c:];commands.append(payload)
 u.mem_write(b,body);active=call(u,0x4ace90,b)&255;assert bytes(u.mem_read(b,len(body)))==body
 call(u,0x4ad8a0,b);after=bytes(u.mem_read(b,len(body)))
 assert after==body[:0xf94]+bytes(2)+body[0xf96:0xf98]+bytes(4)+body[0xf9c:]
 want=w(active)+after[0xf94:0xf9c];expected.append(want)
 x.mem_write(b,payload);result=call(x,entries[0],b);assert bytes(x.mem_read(b,8))==payload
 call(x,entries[1],b);assert w(result)+bytes(x.mem_read(b,8))==want
exe=root/'build/pc/Release/rf_entity_probe.exe';assert subprocess.check_output([str(exe),'--player-mode'],input=b''.join(commands))==b''.join(expected)
report=dict(result='PASS',cases=1024,original_sha256=digest,pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Full original4ace90 low-byte result and4ad8a0 execute without hooks; exact shared PC/NXDK output, all256 active-byte values, reserved and surrounding bytes preserved. No live player/camera owner binding or XEMU gameplay.')
(root/'artifacts/player-mode.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
