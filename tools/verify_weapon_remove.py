"""Original4031a0 removal and mutable player notifications vs PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
b=0x30000000;stack=b+0xe000;stop=b+0xf000
binary=root/'Installed_Game/RF.exe';digest=hashlib.sha256(binary.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);return m
u=machine(binary);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_weapon_remove_owned\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
get=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
trace=[];mutation=0;weapon=0

def callback(m,player,notification,native):
 trace.extend((int(notification),player))
 count=b+0x1000 if native else 0x7c7634;special=b+0x1004 if native else 0x85cce4;players=b+0x1010 if native else 0x7c75e4
 if notification:
  if mutation==2:m.mem_write(count,w(0))
  return 0
 if mutation==1:m.mem_write(special,w(weapon))
 if mutation==3 and len(trace)==2:m.mem_write(players,w(99))
 return b if player%2==0 else 0

def hook(m,a,n,unused):
 native=m is x
 if a not in ((b+0x3100,b+0x3200) if native else (0x4a5b70,0x4a70e0)):return
 sp=m.reg_read(UC_X86_REG_ESP);player=get(m,sp+(8 if native else 4));notify=a==(b+0x3200 if native else 0x4a70e0)
 value=callback(m,player,notify,native);target=get(m,sp);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_EIP,target)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x4031a0);commands=[];expected=[];notifications=0
for i in range(1024):
 inventory=rng.randbytes(448);weapon=rng.choice([-2,-1,0,1,31,63,64,65]);count=rng.randrange(-1,5);special=rng.choice([weapon,0,63]);players=[rng.randrange(10) for _ in range(4)];mutation=i%4
 commands.append(inventory+w(weapon,count,special,*players,mutation))
 u.mem_write(b,bytes(0x1000));u.mem_write(b+0x42c,inventory[:64]);u.mem_write(b+0x2ac,inventory[64:192]);u.mem_write(b+0x32c,inventory[192:])
 u.mem_write(0x7c7634,w(count));u.mem_write(0x85cce4,w(special));u.mem_write(0x7c75e4,w(*players));u.mem_write(stack,w(stop,b+0x2a0,weapon));trace=[];u.reg_write(UC_X86_REG_ESP,stack)
 before=bytes(u.mem_read(b,0x1000));u.emu_start(0x4031a0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 after=bytearray(u.mem_read(b,0x1000))
 if 0<=weapon<64:assert after[0x42c+weapon]==0;after[0x42c+weapon]=before[0x42c+weapon]
 assert after==before
 result=bytes(u.mem_read(b+0x42c,64))+bytes(u.mem_read(b+0x2ac,128))+bytes(u.mem_read(b+0x32c,256))
 state=w(get(u,0x7c7634),get(u,0x85cce4))+bytes(u.mem_read(0x7c75e4,16));events=w(len(trace)//2,*trace)+bytes((16-len(trace))*4)
 expected.append(w(0)+result+state+events);notifications+=sum(trace[::2])
 x.mem_write(b,inventory);x.mem_write(b+0x1000,w(count,special));x.mem_write(b+0x1010,w(*players));x.mem_write(b+0x2000,w(b+0x1010,b+0x1000,b+0x1004,4,b+0x3100,b+0x3200,0));x.mem_write(stack,w(stop,b,weapon,b+0x2000));trace=[];x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,448))+bytes(x.mem_read(b+0x1000,8))+bytes(x.mem_read(b+0x1010,16))+w(len(trace)//2,*trace)+bytes((16-len(trace))*4)
 assert got==expected[-1],i
assert subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--remove'],input=b''.join(commands))==b''.join(expected)
report=dict(result='PASS',cases=len(commands),notifications=notifications,original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Full4031a0 with supplied player-to-inventory resolution and notification. Exact PC/NXDK owned byte, unchanged ammo, callback sequence and mutable player count/special index/notified handle; negative/out-of-range indices and absent players included. Notification4a70e0 is supplied in this fixture; its implementation is checked separately by verify_weapon_player_slots.py.')
(root/'artifacts/weapon-remove.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
