"""Verify48aa30 ordered player control lookup with original426fc0 intact."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe')

w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
entry=int(re.search(r'\s_rf_entity_player_controls\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x48aa30);commands=[];expected=[];hits=0
for n in range(2048):
 handles=[i+65536*rng.randrange(3) for i in range(4)];query=rng.randrange(-1,4);count=rng.randrange(-1,5)
 players=[rng.choice([-1]+handles+[h^65536 for h in handles]) for _ in range(4)]
 nodes=[(handles[i],rng.choice([0,0,1]),rng.choice([-1]+handles),rng.randrange(2)) for i in range(4)]
 command=w(query,count,*players,*(a for row in nodes for a in row));commands.append(command)
 u.mem_write(B,bytes(0xc000));u.mem_write(0x7394cc,bytes(4096));u.mem_write(0x7c7634,w(count));x.mem_write(B,bytes(0xc000))
 for i,(handle,kind,linked,present) in enumerate(nodes):
  p=B+i*0x1000;v=B+0x2000+i*56
  u.mem_write(p+0x2c,w(handle));u.mem_write(p+0x24,w(kind));u.mem_write(p+0x200,w(linked))
  if present:u.mem_write(0x7394cc+i*4,w(p));x.mem_write(B+i*4,w(v))
  x.mem_write(v,w(handle,kind));x.mem_write(v+28,w(linked))
  u.mem_write(0x7c75e4+i*4,w(B+0x8000+i*32));u.mem_write(B+0x8014+i*32,w(players[i]))
 u.mem_write(STACK,w(STOP,B+query*0x1000 if query>=0 else 0));u.reg_write(UC_X86_REG_ESP,STACK);u.emu_start(0x48aa30,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 value=u.reg_read(UC_X86_REG_EAX)&255;assert value in (0,1);expected.append(w(0,value));hits+=value
 x.mem_write(B+0x8000,w(*players));x.mem_write(B+0x9000,w(99));x.mem_write(STACK,w(STOP,B,B+0x2000+query*56 if query>=0 else 0,B+0x8000,count,B+0x9000));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+0x9000,4))==expected[-1],n
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--player-controls'],input=b''.join(commands));assert actual==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=len(commands),matches=hits,scope='Full unhooked48aa30 and426fc0; compact views compared with original pointer/handle/type/link fields. Null query, nonpositive count, absent/wrong-type/stale registry slots, direct and linked matches. No live player-list binding claim.')
(root/'artifacts/player-controls.json').write_text(json.dumps(report,indent=2));print(report)
