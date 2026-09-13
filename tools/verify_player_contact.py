"""Original427550 surface-route gates, with real numeric and class callees."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe')
maps=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_player_contact_direction\s+([0-9a-fA-F]+)',maps)[1],16)
mark_entry=int(re.search(r'\s_rf_player_contact_mark\s+([0-9a-fA-F]+)',maps)[1],16)
word=lambda cpu,a:struct.unpack('<I',bytes(cpu.mem_read(a,4)))[0]
def lookup(cpu,at,size,data):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EAX,B+0x2000);cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,lookup,begin=0x426fc0,end=0x426fc0)
def call(cpu,address,args):
 cpu.mem_write(STACK,w(STOP,*args));cpu.reg_write(UC_X86_REG_ESP,STACK);cpu.reg_write(UC_X86_REG_FPCW,0x37f);cpu.emu_start(address,STOP,count=10000);assert cpu.reg_read(UC_X86_REG_EIP)==STOP;return cpu.reg_read(UC_X86_REG_EAX)
def compiled(blob):
 present,flags,mark=struct.unpack('<III',blob[48:]);x.mem_write(B,blob[:48]);x.mem_write(B+0x100,w(0xa5a5a5a5,flags));status=call(x,entry,[B,B+12 if present else 0,B+0x100]);call(x,mark_entry,[B+0x104 if present else 0,mark]);return w(status)+bytes(x.mem_read(B+0x100,8))
rng=random.Random(0x4a5a20);commands=[];expected=[];directions={}
for n in range(2048):
 normal=[rng.uniform(-10,10) for _ in range(3)];basis=[rng.uniform(-1,1) for _ in range(9)]
 if n%8==0:normal=[0,1,0]
 if n%8==1:normal=[1,0,1];basis=[1,0,0,0,1,0,0,0,1]
 present=int(n%7!=0);flags=rng.getrandbits(32);mark=rng.getrandbits(32);blob=f(*normal,*basis)+w(present,flags,mark)
 u.mem_write(B,blob[:12]);u.mem_write(B+0x1000,bytes(32));u.mem_write(B+0x1010,w(flags));u.mem_write(B+0x2048,blob[12:48]);direction=call(u,0x4a5a20,[B+0x1000 if present else 0,B]);call(u,0x4a5af0,[B+0x1000 if present else 0,mark]);result=w(0,direction,word(u,B+0x1010));assert compiled(blob)==result,(n,blob.hex(),result.hex(),compiled(blob).hex());commands.append(blob);expected.append(result);directions[direction]=directions.get(direction,0)+1
for offset in (0,4,12):
 blob=bytearray(commands[2]);blob[offset:offset+4]=w(0x7fc00000);blob=bytes(blob);result=compiled(blob);assert result[:8]==w(0xfffffffc,0xa5a5a5a5);commands.append(blob);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--player-contact'],input=b''.join(commands));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=2048,finite_guards=3,directions=directions,scope='Full original4a5a20 and4a5af0 with real vector copy/4fac60 transform; actor lookup supplied. Exact direction and arbitrary low-nibble flag accumulation on PC/NXDK, missing player, vertical normal and equal-axis tie cases. Scene player-loop binding excluded.')
(root/'artifacts/player-contact.json').write_text(json.dumps(report,indent=2));print(report)
