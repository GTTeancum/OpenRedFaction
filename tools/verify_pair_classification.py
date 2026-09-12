"""Check full48be00 routing/flag writes with explicit external predicate boundaries."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xe000;stop=b+0xf000;state=b+0x8000;out=b+0x9000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
fword=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_collision_pair_reject\s+([0-9a-fA-F]+)',mapping)[1],16)
hooks={0x4290d0:16,0x427020:17,0x48c7f0:18};calls={hex(a):0 for a in [*hooks,0x40a0e0,0x48a840]}
views=[];trigger_calls=0;trigger_filters={}
def hook(m,address,size,data):
 global trigger_calls
 if address==0x48c8e0:
  trigger_calls+=1;sp=m.reg_read(UC_X86_REG_ESP);arg=struct.unpack('<I',m.mem_read(sp+4,4))[0]
  filter_value=struct.unpack('<I',m.mem_read(arg+0x2c4,4))[0];trigger_filters[str(filter_value)]=trigger_filters.get(str(filter_value),0)+1
 if address not in hooks and address not in (0x40a0e0,0x48a840):return
 sp=m.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<2I',m.mem_read(sp,8));calls[hex(address)]+=1
 if address in hooks:
  assert arg in (b,b+0x2000);index=(arg-b)//0x2000;value=views[index][hooks[address]]
  if address in (0x48c7f0,0x48c8e0):
   assert struct.unpack('<I',m.mem_read(sp+8,4))[0]==b+(1-index)*0x2000
   if address==0x48c7f0:assert struct.unpack('<I',m.mem_read(sp+12,4))[0]==1
 else:
  index=next(i for i,v in enumerate(views) if v[10]==arg)
  value=(b+0x6000+index*0x400 if views[index][20] else 0) if address==0x40a0e0 else views[index][23]
 m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
def run(m,address,args):
 m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,stop,count=20000)
 assert m.reg_read(UC_X86_REG_EIP)==stop;return m.reg_read(UC_X86_REG_EAX)&255
rng=random.Random(0x48be01);commands=[];answers=[];coverage={};changed_rejections=0
for case in range(32768):
 views=[]
 for i in range(2):
  of=rng.choice((0,0,8,8,0x20000,0x40000,0x4000)) if case%4==0 else rng.choice((0,8))
  bf=rng.choice((0,0x20,0x60,0x40)) if case%4==0 else 0x60
  kind=(case//8)%8 if i==0 else case%8
  view=[of,bf,rng.choice((0,1)),rng.choice((-1,i)),rng.choice((-1,-1,2)),rng.choice((0,0x20)),fword(rng.choice((-.5,0,.025,.05,.050000004,.2,.20000002,.5,.50000006,1,1.5,2,4))),rng.randrange(2)]
  view += [kind,10+i,rng.choice((100+i,11-i)),rng.randrange(2),rng.choice((0,0x20)),rng.choice((4,7,9)),fword(rng.choice((0,1,1.5,2,4))),rng.getrandbits(32)]
  view += [rng.choice((0,0,1,256,257)) for _ in range(3)]+[rng.choice((0,1,2,3,4,5,256,0xffffffff))]
  present=rng.randrange(2);view += [present,rng.choice((0,8)) if present else 0,rng.choice((100,101,10,11)),rng.choice((0,1,256,257))]
  views.append(view)
 for i,v in enumerate(views):
  actor=b+i*0x2000;definition=b+0x5000+i*0x400;name=b+0x7000+i*0x100;owner=b+0x6000+i*0x400
  u.mem_write(actor,bytes(0x1500));u.mem_write(definition,bytes(0x300));u.mem_write(owner,bytes(0x300))
  for offset,index in [(0x24,8),(0x2c,9),(0x30,10),(0x78,14),(0x7c,0),(0x180,6),(0x1a8,1),(0x298,13),(0x2b0,15),(0x2c4,19)]:u.mem_write(actor+offset,w(v[index]))
  u.mem_write(actor+0x294,w(definition if v[8]!=3 or v[11] else 0));u.mem_write(definition+0x1b4,w(v[2]));u.mem_write(definition+0x268,w(v[12]));u.mem_write(actor+0x2a4,w(v[3],v[4]));u.mem_write(0x85cf6c+i*0x550,w(v[5]))
  label=b'Sea_Creature' if v[7] else b'Miner';u.mem_write(actor+0x18,w(len(label),name));u.mem_write(name,label+b'\0');u.mem_write(owner+0x7c,w(v[21]));u.mem_write(owner+0x200,w(v[22]))
 alt=rng.choice((0,1,256));multi=rng.choice((0,0,256));mode=rng.choice((0,1,256));initial=rng.getrandbits(32);alias=0 if case<32000 else rng.choice((0,1,2,4))
 for addr,value in [(0x6fc4d8,alt),(0x64ecb9,multi),(0x6fc4d9,mode)]:u.mem_write(addr,bytes((value&255,)))
 u.mem_write(0x87210c,w(7));u.mem_write(0x5afb78,w(9));u.mem_write(out,w(initial))
 left=0 if alias&1 else b;right=0 if alias&2 else b if alias&4 else b+0x2000
 before=bytes(u.mem_read(b,0x7800));result=run(u,0x48be00,[left,right,out]);flags=bytes(u.mem_read(out,4));assert bytes(u.mem_read(b,0x7800))==before
 payload=w(*views[0],*views[1]);x.mem_write(state,payload);x.mem_write(out,w(initial));actual=run(x,entry,[0 if alias&1 else state,0 if alias&2 else state if alias&4 else state+96,alt,multi,mode,7,9,out])
 assert (actual,bytes(x.mem_read(out,4)))==(result,flags),(case,views,alt,multi,mode,alias,result,actual,hex(initial),flags.hex(),bytes(x.mem_read(out,4)).hex())
 assert bytes(x.mem_read(state,192))==payload
 commands.append(payload+w(alt,multi,mode,7,9,initial,alias));answers.append(w(result)+flags)
 key=f'{views[0][8]}/{views[1][8]}/{result}';coverage[key]=coverage.get(key,0)+1;changed_rejections+=result==1 and flags!=w(initial)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--pair-classify'],input=b''.join(commands));assert actual==b''.join(answers)
assert trigger_calls>0 and all(str(i) in trigger_filters for i in range(6))
report=dict(result='PASS',trigger_calls=trigger_calls,trigger_filters=trigger_filters,cases=len(commands),outcomes=coverage,changed_flags_on_rejection=changed_rejections,predicate_calls=calls,original_sha256=sha,scope='Full48be00 branch routing and flag writes, all kind0..7 combinations, null/self and arbitrary initial flags; exact PC/NXDK. Actual actor/player/use/weapon/string and trigger48c8e0/4c0910 helpers. External disabled/item-mode/projectile predicates and owner lookup/player predicate supplied as explicit boundaries. Their implementations, callback mutation and live scene binding are not verified by this gate.')
(root/'artifacts/pair-classification.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='outcomes'})
