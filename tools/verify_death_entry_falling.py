"""Run original falling predicate and SP death prefix without predicate hooks."""
import hashlib,itertools,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
def machine(path):
 p=pefile.PE(str(path));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data)
 m.mem_map(base,65536);return m
original=root/'Installed_Game/RF.exe';sha=hashlib.sha256(original.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(original);x=machine(root/'build/xbox/main.exe');u.mem_map(0,4096);u.mem_write(0x64ecb9,bytes(2))
mapping=(root/'build/xbox/main.map').read_text()
symbol=lambda name:int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
def call(m,address,*args):
 m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,stop,count=2000)
 return m.reg_read(UC_X86_REG_EAX)
entered=0
def boundary(m,address,size,data):
 global entered
 if address==0x48c9f0:entered=1;m.emu_stop()
u.hook_add(UC_HOOK_CODE,boundary)
rng=random.Random(0x42a020);commands=[];answers=[];entries=[];entry_answers=[];counts=[0,0]
offsets=[0x810,0x1a8,0x714,0x718,0x71c,0x144,0x148,0x14c,0x150,0x154,0x158]
for kind,mode,use,material in itertools.product((0,1,2,4,5,0xffffffff),(0,1,2,3,8,10,0xffffffff),(0,1,2,8,0x101,0xffffffff),(0,1,0xffffffff)):
 actor=bytearray(rng.randbytes(0x1500));actor[0x24:0x28]=w(kind);actor[0x294:0x298]=w(base+0x2000)
 actor[0x858:0x85c]=w(base+0x3000);actor[0x1380:0x1384]=w(material)
 u.mem_write(base+0x2000,bytes(0x200));u.mem_write(base+0x21b4,w(use));u.mem_write(base+0x2044,w(use));u.mem_write(base+0x3004,w(mode))
 u.mem_write(base,bytes(actor));before=bytes(u.mem_read(base,0x4000))
 falling=call(u,0x42a020,base)&255
 assert u.reg_read(UC_X86_REG_EIP)==stop and bytes(u.mem_read(base,0x4000))==before
 resolved=use if kind in (0,4) else 8 if kind==2 else 7 if kind==5 else 0
 actual=call(x,symbol('rf_entity_falling'),mode,resolved,material)
 assert x.reg_read(UC_X86_REG_EIP)==stop and actual==falling,(kind,mode,use,material,actual,falling)
 commands.append(w(mode,resolved,material));answers.append(w(falling));counts[falling]+=1
 for dying in (0,1):
  values=[rng.getrandbits(32) for _ in offsets];values[0]=(values[0]&~1)|dying
  for off,value in zip(offsets,values):actor[off:off+4]=w(value)
  u.mem_write(base,bytes(actor));u.mem_write(0,w(0xffffffff));entered=0
  call(u,0x41fdc0,base);assert entered==1-dying and u.reg_read(UC_X86_REG_EIP) in (stop,0x48c9f0)
  after=bytes(u.mem_read(base,len(actor)));result=b''.join(after[o:o+4] for o in offsets)
  rest=bytearray(after)
  for off in offsets:rest[off:off+4]=actor[off:off+4]
  assert rest==actor
  x.mem_write(base,w(*values));actual=call(x,symbol('rf_entity_death_entry_sp'),base,falling)
  assert actual==entered and bytes(x.mem_read(base,44))==result
  entries.append(w(*values,falling));entry_answers.append(w(entered)+result)
probe=str(root/'build/pc/Release/rf_entity_probe.exe')
assert subprocess.check_output([probe,'--falling'],input=b''.join(commands))==b''.join(answers)
assert subprocess.check_output([probe,'--death-entry'],input=b''.join(entries))==b''.join(entry_answers)
report=dict(result='PASS',predicate_cases=len(commands),entry_cases=len(entries),grounded=counts[0],falling=counts[1],original_sha256=sha,scope='Unhooked original42a020/429990/486c90 and SP41fdc0 prefix through collision boundary. Exact PC/NXDK predicate and entry state; all other actor bytes preserved. Resolved use-kind supplied to shared helper. Live entry scheduling and collision teardown excluded.')
(root/'artifacts/death-entry-falling.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
