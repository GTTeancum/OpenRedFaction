"""Compare kind0/kind0 classification to unmodified48be00 and callees."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
fword=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_collision_actor_pair_reject\s+([0-9a-fA-F]+)',mapping)[1],16)
def run(m,address,args):
 m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,stop,count=10000)
 assert m.reg_read(UC_X86_REG_EIP)==stop;return m.reg_read(UC_X86_REG_EAX)&255
rng=random.Random(0x48be00);commands=[];answers=[];counts={};samples=[]
for case in range(8192):
 views=[]
 for i in range(2):
  of=rng.choice((0,0,0,8,8,0x20000,0x40000,0x4000));bf=rng.choice((0x20,0x20,0x60,0x60,0,0x40))
  use=rng.choice((0,0,1,8));primary=rng.choice((-1,i));secondary=rng.choice((-1,-1,2));wf=rng.choice((0,0x20))
  extent=rng.choice((0,.25,.5,1,2,4,8,-1));equal=rng.randrange(2)
  # Directed actor/player size grid in both orders, away from earlier gates.
  if case<512:
   of=8 if i==case%2 else 0;bf=0x60;use=0;primary=secondary=-1;wf=0
   extent=(.25,.5,1,2,4,8,16,32)[(case//2//(8 if i else 1))%8]
  view=[of,bf,use,primary,secondary,wf,fword(extent),equal];views.append(view)
  actor=b+i*0x2000;definition=b+0x5000+i*0x400;name=b+0x6000+i*0x100
  u.mem_write(actor,bytes(0x1500));u.mem_write(actor+0x24,w(0));u.mem_write(actor+0x7c,w(of));u.mem_write(actor+0x1a8,w(bf));u.mem_write(actor+0x180,w(fword(extent)))
  u.mem_write(actor+0x294,w(definition));u.mem_write(definition+0x1b4,w(use));u.mem_write(actor+0x2a4,w(primary,secondary));u.mem_write(0x85cf6c+i*0x550,w(wf))
  label=b'sEa_cReAtUrE' if equal else b'Miner';u.mem_write(actor+0x18,w(len(label),name));u.mem_write(name,label+b'\0')
 alt=rng.choice((0,0,1,256));multi=rng.choice((0,0,2,256));initial=rng.getrandbits(32);alias=0 if case<8000 else case%4
 if case<512:alt=multi=0
 u.mem_write(0x6fc4d8,bytes((alt&255,)));u.mem_write(0x64ecb9,bytes((multi&255,)));u.mem_write(b+0x7000,w(initial))
 left=0 if alias==1 else b;right=0 if alias==2 else b if alias==3 else b+0x2000
 before=bytes(u.mem_read(b,0x7000));result=run(u,0x48be00,[left,right,b+0x7000]);flags=bytes(u.mem_read(b+0x7000,4))
 assert bytes(u.mem_read(b,0x7000))==before
 x.mem_write(b+0x8000,w(*views[0]));x.mem_write(b+0x8020,w(*views[1]));x.mem_write(b+0x8040,w(initial))
 xa=0 if alias==1 else b+0x8000;xb=0 if alias==2 else b+0x8000 if alias==3 else b+0x8020
 actual=run(x,entry,[xa,xb,alt,multi,b+0x8040])
 assert actual==result and bytes(x.mem_read(b+0x8040,4))==flags,(case,views,alias,result,actual,flags.hex())
 assert bytes(x.mem_read(b+0x8000,64))==w(*views[0],*views[1])
 commands.append(w(*views[0],*views[1],alt,multi,initial,alias));answers.append(w(result)+flags)
 key=str(result)+':'+str(struct.unpack('<I',flags)[0] if flags!=w(initial) else 'preserved');counts[key]=counts.get(key,0)+1
 if case<8:samples.append(dict(views=views,result=result,flags=struct.unpack('<I',flags)[0]))
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--actor-pair'],input=b''.join(commands))==b''.join(answers)
report=dict(result='PASS',cases=len(commands),outcomes=counts,original_sha256=sha,scope='Full original48be00 for two kind0 actors including common gates, real player/use-kind/weapon/string helpers, no hooks. PC/NXDK result and flag writes exact; actor facts unchanged. Directed asymmetric size grid, common rejection flags, names, weapons, use-kind and null/self. Other object families, live fact binding and pair discovery excluded.')
(root/'artifacts/actor-pair-classification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
