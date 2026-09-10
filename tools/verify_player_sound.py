"""Original 48a9c0 routing versus PC and compiled NXDK requests; no audio output."""
import hashlib,itertools,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
base=0x30000000;entity=base;owner=base+0x2000;camera=base+0x4000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]);f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def load(path):
 pe=pefile.PE(str(path));im=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(im)+4095)//4096*4096);u.mem_write(origin,im);u.mem_map(base,65536);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);observed=[]
def boundary(cpu,address,size,data):
 if address not in (0x505560,0x5056a0):return
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if address==0x505560:
  sound,group,pan,volume=struct.unpack('<4I',cpu.mem_read(sp+4,16))
  observed.append(w(0,sound)+bytes(12)+w(volume,pan,group))
 else:
  sound,position,volume,unused,group=struct.unpack('<5I',cpu.mem_read(sp+4,20))
  assert unused==0x173c378
  observed.append(w(1,sound)+bytes(cpu.mem_read(position,12))+w(volume,0,group))
 cpu.emu_stop()
u.hook_add(UC_HOOK_CODE,boundary);u.mem_write(owner+0xc4,w(camera))
commands=[];expected=[]
for kind,present,mode,sound,volume,pan in itertools.product((0,1,4),(0,1),(0,1,2,-1),(18,0,75),(.25,1,2),(-100,0,100)):
 position=f(1.25,-2.5,17)
 u.mem_write(entity+0x24,w(kind));u.mem_write(entity+0x1430,w(owner if present else 0));u.mem_write(camera+8,w(mode))
 before=bytes(u.mem_read(base,0x5000));u.mem_write(stack,w(stop,entity)+position+w(sound)+f(volume)+w(pan));u.reg_write(UC_X86_REG_ESP,stack)
 observed.clear();u.emu_start(0x48a9c0,stop,count=1000)
 assert len(observed)==1 and bytes(u.mem_read(base,0x5000))==before
 commands.append(w(kind,present,mode)+position+w(sound)+f(volume)+w(pan));expected.append(observed[0])
probe=root/'build/pc/Release/rf_entity_probe.exe'
assert subprocess.check_output([str(probe),'--player-sound'],input=b''.join(commands))==b''.join(expected)
binary=root/'build/xbox/main.exe';x=load(binary)
entry=int(re.search(r'_rf_player_sound_route\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for data,want in zip(commands,expected):
 x.mem_write(base,data);x.mem_write(stack,w(stop,base,owner));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=1000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(owner,32))==want and bytes(x.mem_read(base,36))==data
report=dict(result='PASS',cases=len(commands),original_sha256=sha,
 pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
 scope='Original 48a9c0 through playback dispatch, unchanged ownership and camera getter. Dispatch arguments normalized into shared requests, exact PC/NXDK. Does not execute audio backend or identify sound assets.')
(root/'artifacts/player-sound-verification.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(commands),'PC/NXDK sound routing cases')
