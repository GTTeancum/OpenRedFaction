"""Complete slow/crouch wrapper with supplied standing effects."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
base=0x30000000;stack=base+0x8000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);b=p.OPTIONAL_HEADER.ImageBase
 u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(base,65536);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
import itertools
u=machine(exe);nx=machine(root/'build/xbox/main.exe');callback=base+0xc000
entry=int(re.search(r'\s_rf_player_slow_enter\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
blocked=calls=xcalls=0
def oh(m,address,size,data):
 global calls
 if address!=0x428a60:return
 calls+=1;sp=m.reg_read(UC_X86_REG_ESP);m.mem_write(base+0x8c,f(9))
 if not blocked:
  flags=struct.unpack('<I',m.mem_read(base+0x810,4))[0];m.mem_write(base+0x810,pack(flags&~0x400))
 m.reg_write(UC_X86_REG_EAX,int(not blocked));m.reg_write(UC_X86_REG_EIP,struct.unpack('<I',m.mem_read(sp,4))[0]);m.reg_write(UC_X86_REG_ESP,sp+4)
def xh(m,address,size,data):
 global xcalls
 if address!=callback:return
 xcalls+=1;sp=m.reg_read(UC_X86_REG_ESP);stood=struct.unpack('<I',m.mem_read(sp+8,4))[0]
 m.mem_write(base+20,f(9));m.mem_write(stood,pack(int(not blocked)));m.reg_write(UC_X86_REG_EAX,0)
 if not blocked:
  flags=struct.unpack('<I',m.mem_read(base+0x2000,4))[0];m.mem_write(base+0x2000,pack(flags&~0x400))
u.hook_add(UC_HOOK_CODE,oh);nx.hook_add(UC_HOOK_CODE,xh);nx.mem_write(callback,b'\xc3')
cases=[];expected=[]
for walk,crouch,force,blocked,enabled,forced,network in itertools.product((0,1),(0,0x400),(0,1,256,257),(0,1),(0,1,256,257),(-1,4),(0,1)):
 calls=xcalls=0;raw=pack(walk,crouch,force,blocked,enabled,forced&0xffffffff,network);cases.append(raw)
 u.mem_write(base,bytes(0x6000));u.mem_write(base+0x294,pack(base+0x3000));u.mem_write(base+0x3724,pack(walk));u.mem_write(base+0x3050,f(3.5,.5,0,1));u.mem_write(base+0x13ec,pack(0x12345678,0x23456789))
 for off,data in [(0x8c,f(2)),(0x98,f(1)),(0x75c,pack(forced&0xffffffff)),(0x810,pack(crouch)),(0x858,pack(0x62fe90,0)),(0x8c0,f(7)+pack(1)),(0x148,f(5))]:u.mem_write(base+off,data)
 u.mem_write(0x62fe50,bytes(512));u.mem_write(0x62fe70,pack(enabled,1));u.mem_write(0x62fe90,pack(1,2));u.mem_write(0x630050,pack(77));u.mem_write(0x64ecb9,bytes([network]));u.mem_write(0x594590,f(2));u.mem_write(0x59458c,f(3))
 u.mem_write(stack,pack(stop,base,force));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x428030,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 assert bytes(u.mem_read(base+0x13ec,8))==pack(0x12345678,0x23456789)
 ptr=struct.unpack('<I',u.mem_read(base+0x858,4))[0];selected=struct.unpack('<I',u.mem_read(0x630050,4))[0]
 want=pack(0,1,1,(ptr-0x62fe50)//32,int(struct.unpack('<I',u.mem_read(base+0x85c,4))[0]==0x73a858))+bytes(u.mem_read(base+0x810,4))+pack(selected,calls)+bytes(u.mem_read(base+0x8c,4))+bytes(u.mem_read(base+0x8c0,8))+bytes(u.mem_read(base+0x148,4));expected.append(want)
 nx.mem_write(base,pack(base+0x6000,base+0x6000,base+0x4040,0,123)+f(2,7)+pack(1)+f(5));nx.mem_write(base+0x2000,pack(crouch));nx.mem_write(base+0x2010,pack(77));nx.mem_write(base+0x3000,pack(walk)+f(3.5,.5,0,1,2,3));nx.mem_write(base+0x4000,bytes(512));nx.mem_write(base+0x4020,pack(enabled,1));nx.mem_write(base+0x4040,pack(1,2))
 nx.mem_write(base+0x1000,pack(base+0x3000,base+0x4000,base+0x5000,forced&0xffffffff)+f(1)+pack(force,network));nx.mem_write(stack,pack(stop,base,base+0x2000,base+0x1000,base+0x2010,callback,0));nx.reg_write(UC_X86_REG_ESP,stack);nx.emu_start(entry,stop,count=10000)
 assert nx.reg_read(UC_X86_REG_EIP)==stop
 state=struct.unpack('<9I',nx.mem_read(base,36));got=pack(nx.reg_read(UC_X86_REG_EAX),int(state[0]==base+0x6000),int(state[1]==base+0x6000),(state[2]-base-0x4000)//32,int(state[3]==base+0x5000))+bytes(nx.mem_read(base+0x2000,4))+bytes(nx.mem_read(base+0x2010,4))+pack(xcalls,*state[5:])
 assert got==want,('NXDK',raw.hex(),got.hex(),want.hex())
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--slow-enter'],input=b''.join(cases));assert pc==b''.join(expected)
report=dict(result='PASS',cases=len(cases),original_sha256=sha,scope='Complete428030 with real class/crouch predicates,427450 speed and4339d0 lookup; standing428a60 supplied at one boundary. PC/NXDK exact descriptor/flags/speed/vertical state, both regions retained. Low-byte force/enable, blocked stand continues, callback response mutation, forced action and network override. Clearance/sphere/support effects and live NPC landing remain excluded.')
(root/'artifacts/player-slow-enter.json').write_text(json.dumps(report,indent=2));print(report)
