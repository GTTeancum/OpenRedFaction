"""Original fixed projectile pool initialization/allocation/release versus PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
B=0x30000000;STACK=B+0xff000;STOP=B+0xfff00;OUT=B+0x20000;SIZE=50*788
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();c=Uc(UC_ARCH_X86,UC_MODE_32);a=p.OPTIONAL_HEADER.ImageBase;c.mem_map(a,(len(im)+4095)//4096*4096);c.mem_write(a,im);c.mem_map(B,0x100000);return c
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entries=[int(re.search(r'\s_rf_projectile_pool_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for n in ('init','acquire','release')]
def word(c,a):return struct.unpack('<I',c.mem_read(a,4))[0]
def token(pointer):return (pointer-B)//788+1 if pointer else 0
def run(c,a,args,this=False):
 c.mem_write(STACK,w(STOP,*args));c.reg_write(UC_X86_REG_ESP,STACK)
 if this:c.reg_write(UC_X86_REG_ECX,B)
 c.emu_start(a,STOP,count=10000);assert c.reg_read(UC_X86_REG_EIP)==STOP;return c.reg_read(UC_X86_REG_EAX)
u.mem_write(B,b'\xa5'*(SIZE+32));x.mem_write(B,b'\xa5'*(SIZE+24));u.mem_write(0x879af8,b'\0');run(u,0x48b4d0,[0],True);run(x,entries[0],[B])
rng=random.Random(0x48b590);commands=[];outputs=[];live=set();acquires=releases=exhausted=0
for k in range(4096):
 release=k>=51 and bool(live) and rng.random()<.5
 if release:
  slot=rng.choice(sorted(live));live.remove(slot);command=[1,slot];run(u,0x48b610,[B+slot*788],True);status=run(x,entries[2],[B,slot]);assert status==0;expected_status=0;output_slot=0xffffffff;releases+=1
 else:
  command=[0,0];pointer=run(u,0x48b590,[],True);x.mem_write(OUT,w(-1));status=run(x,entries[1],[B,OUT]);output_slot=token(pointer)-1 if pointer else 0xffffffff;expected_status=0 if pointer else -3
  assert status==expected_status&0xffffffff and word(x,OUT)==output_slot
  if pointer:assert output_slot not in live;live.add(output_slot);acquires+=1
  else:exhausted+=1
 links=[token(word(u,B+i*788)) for i in range(50)];expected=w(expected_status,output_slot,token(word(u,B+0x99e8)),word(u,B+0x99f4),word(u,B+0x9a00),word(u,B+0x99fc),*links)
 got=w(status,output_slot,*[word(x,B+SIZE+i*4) for i in range(4)],*[word(x,B+i*788) for i in range(50)]);assert got==expected,k
 assert word(x,B+SIZE+8)==len(live)
 for i in range(50):assert bytes(u.mem_read(B+i*788+4,784))==bytes(x.mem_read(B+i*788+4,784))==b'\xa5'*784
 commands.append(w(*command));outputs.append(expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--projectile-pool'],input=b''.join(commands))==b''.join(outputs)
# Port guards preserve pool bytes: duplicate release, foreign slot, null output.
run(x,entries[0],[B]);before=bytes(x.mem_read(B,SIZE+24))
for entry,args,status in [(entries[2],[B,0],-3),(entries[2],[B,50],-4),(entries[1],[B,0],-4),(entries[1],[0,OUT],-4)]:
 assert run(x,entry,args)==status&0xffffffff and bytes(x.mem_read(B,SIZE+24))==before
report=dict(result='PASS',operations=4096,acquires=acquires,releases=releases,exhausted=exhausted,nxdk_guards=4,pool_bytes=SIZE+24,original_sha256=digest,scope='Original48b4d0/48b590/48b610 unhooked with heap fallback disabled. PC/NXDK exact slot sequence, free links normalized to indices, free/live/peak counters and untouched payload. Includes exhaustion and LIFO reuse. Registry insertion, object initialization and resource cleanup excluded.')
(root/'artifacts/projectile-pool.json').write_text(json.dumps(report,indent=2));print(report)
