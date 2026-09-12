"""Verify original intrusive object-list append/removal instruction regions."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_ECX
b=0x30000000;stack=b+0xe000;stop=b+0xf000;xl=b+0x8000;xn=b+0x9000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
symbol=lambda name:int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
def runx(name,args):
 x.mem_write(stack,w(stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(symbol(name),stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
runx('rf_object_list_init',[xl]);u.mem_write(0x73d890,w(0x73d880,0x73d880));u.mem_write(0x73a850,w(0));u.mem_write(0x73db0c,w(0))
for i in range(16):u.mem_write(b+i*0x100+0x10,bytes(8))
def words(m,address,count):return struct.unpack('<'+'I'*count,m.mem_read(address,count*4))
def snapshot(original):
 m=u if original else x;sentinel=0x73d880 if original else xl
 ids={sentinel:16,0:0xffffffff};ids.update({(b+i*0x100 if original else xn+i*8):i for i in range(16)})
 counts=words(m,0x73a850,1)+words(m,0x73db0c,1) if original else words(m,xl+8,2)
 out=list(counts);out += [ids[p] for p in words(m,sentinel+(0x10 if original else 0),2)]
 for i in range(16):out += [ids[p] for p in words(m,b+i*0x100+0x10 if original else xn+i*8,2)]
 return w(*out)
rng=random.Random(0x4872eb);live=[];commands=[];answers=[];appends=removes=0
for step in range(8192):
 if step%257==0:
  command=[2,rng.choice((0,0x7ffffffe,0x7fffffff,0xfffffffe,0xffffffff)),rng.choice((0,0x7fffffff,0x80000000,0xffffffff))]
  u.mem_write(0x73a850,w(command[1]));u.mem_write(0x73db0c,w(command[2]));x.mem_write(xl+8,w(*command[1:]))
 else:
  detached=[i for i in range(16) if i not in live];append=bool(detached) and (not live or rng.randrange(2)==0)
  i=rng.choice(detached if append else live);command=[0 if append else 1,i,0]
  u.reg_write(UC_X86_REG_ESP,stack)
  if append:
   u.reg_write(UC_X86_REG_ESI,b+i*0x100);u.emu_start(0x4872eb,0x487321,count=1000);assert u.reg_read(UC_X86_REG_EIP)==0x487321
   runx('rf_object_list_append',[xl,xn+i*8]);live.append(i);appends+=1
  else:
   u.reg_write(UC_X86_REG_ECX,b+i*0x100);u.emu_start(0x4867bc,0x4867e1,count=1000);assert u.reg_read(UC_X86_REG_EIP)==0x4867e1
   runx('rf_object_list_remove',[xl,xn+i*8]);live.remove(i);removes+=1
 expected=snapshot(True);assert snapshot(False)==expected,(step,command)
 # Independent forward traversal confirms allocation order, including remove/reappend.
 cursor=words(u,0x73d890,1)[0];order=[]
 while cursor!=0x73d880:
  assert len(order)<16;order.append((cursor-b)//0x100);cursor=words(u,cursor+0x10,1)[0]
 assert order==live
 commands.append(w(*command));answers.append(expected)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_object_registry_probe.exe'),'--list'],input=b''.join(commands));assert actual==b''.join(answers)
report=dict(result='PASS',operations=8192,appends=appends,removals=removes,counter_boundary_seeds=32,original_sha256=sha,scope='Original4872eb..487321 append/count/peak and4867bc..4867e1 unlink/count instruction regions, no hooks. Exact PC/NXDK normalized links and counters after every operation, independent forward allocation order, signed peak and wrapping count. Excludes constructor/destructor, room initialization, handle slot lifetime and level factory call order; no native XEMU activation.')
(root/'artifacts/object-list.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
