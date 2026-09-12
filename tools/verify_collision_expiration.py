"""Original48cc10 with actual48c7f0 mode0 and all callees, versus PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;second=b+0x2000;pair=b+0x4000;state=b+0x8000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_collision_pair_expired\s+([0-9a-fA-F]+)',mapping)[1],16)
def run(m,address,args):
 m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,stop,count=2000)
 assert m.reg_read(UC_X86_REG_EIP)==stop and m.reg_read(UC_X86_REG_FPCW)==0x27f
 return m.reg_read(UC_X86_REG_EAX)&255
rng=random.Random(0x48cc10);commands=[];answers=[];counts=[0,0]
for case in range(4096):
 flags=rng.choice((0,1,2,3,0x100,0x101,0xffffffff));a=rng.choice((0,2,2,3,7));c=rng.choice((0,2,2,4,0xffffffff))
 geometry=[rng.randrange(-256,257)/8 for _ in range(12)]
 if case<64:
  flags=1;a=c=2;geometry=[0,0,0,0,0,1,0,0,(-1,0,1)[case%3],0,0,(-1,1)[case%2]]
 if 64<=case<128:
  flags=1;a=2;c=0;geometry=[0,0,0,1,1,-1,16777216,1,16777216,0,0,1]
 payload=w(flags,a,c)+f(geometry);commands.append(payload)
 # Poison definitions/owners: existing-pair mode must never resolve them.
 u.mem_write(b,bytes([0xcc])*0x4500);u.mem_write(b+0x24,w(a));u.mem_write(second+0x24,w(c))
 u.mem_write(b+0x3c,f(geometry[:3]));u.mem_write(b+0x60,f(geometry[3:6]));u.mem_write(second+0x3c,f(geometry[6:9]));u.mem_write(second+0x60,f(geometry[9:12]))
 u.mem_write(pair,w(0,b,second,flags));before=bytes(u.mem_read(b,0x4500));result=run(u,0x48cc10,[pair]);assert bytes(u.mem_read(b,0x4500))==before
 x.mem_write(state,payload);actual=run(x,entry,[state]);assert actual==result and bytes(x.mem_read(state,60))==payload,(case,flags,a,c,geometry,result,actual)
 answers.append(w(result));counts[result]+=1
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--pair-expired'],input=b''.join(commands))==b''.join(answers)
report=dict(result='PASS',cases=4096,retained=counts[0],expired=counts[1],original_sha256=sha,x87_control='0x027f',scope='Full original48cc10 and actual48c7f0 mode0 with all real callees, no hooks. Exact PC/NXDK result and no mutation. First-projectile precedence, both/non-projectile pairs, flag gates, equality and finite cancellation. Owner/definition fields poisoned to prove unused. Live pair-list processing excluded.')
(root/'artifacts/collision-expiration.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
