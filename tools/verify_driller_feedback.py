"""Original427550 surface-route gates, with real numeric and class callees."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_FPCW
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_contact_driller_feedback\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
word=lambda cpu,a:struct.unpack('<I',bytes(cpu.mem_read(a,4)))[0]
words=lambda cpu,a,n:list(struct.unpack('<'+'I'*n,bytes(cpu.mem_read(a,4*n))))
values=[];digest=calls=0
def record(event,row):
 global digest,calls
 calls+=1
 for v in [event]+row+[0]*(4-len(row)):digest=((digest^v)*16777619)&0xffffffff
def hook(cpu,at,size,original):
 sp=cpu.reg_read(UC_X86_REG_ESP);a=words(cpu,sp+4,5)
 event=({0x426fc0:1,0x40e0b0:2,0x4a5a20:3,0x4a5af0:4} if original else {B+0x8000:1,B+0x8100:2,B+0x8200:3,B+0x8300:4})[at]
 if not original:a=a[1:]
 result=0
 if event==1:
  i=a[0]-100;record(1,[a[0]]);present=values[1]&(1<<i);linked=77 if values[2]&(1<<i) else 999
  if original:
   cpu.mem_write(B+0x4200,w(linked));cpu.mem_write(B+0x5430,w(B+0x6000));cpu.mem_write(B+0x60c4,w(300+i));result=B+0x4000 if present else 0
  else:cpu.mem_write(B+0x400,w(linked,300+i));cpu.mem_write(a[1],w(B+0x400 if present else 0))
 elif event==2:
  record(2,a[:3])
  if values[4]:cpu.mem_write(0x7c7634 if original else B+0x100,w(0))
 elif event==3:
  token=200+(a[0]-B-0x2000)//32 if original else word(cpu,a[0]);record(3,[token]+words(cpu,a[1],3))
  if original:result=values[3]
  else:cpu.mem_write(a[2],w(values[3]))
 else:
  token=200+(a[0]-B-0x2000)//32 if original else word(cpu,a[0]);record(4,[token,a[1]])
 if not original:result=0xffffffff if values[7]==event else 0
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for a in (0x426fc0,0x40e0b0,0x4a5a20,0x4a5af0):u.hook_add(UC_HOOK_CODE,hook,True,begin=a,end=a)
for a in (B+0x8000,B+0x8100,B+0x8200,B+0x8300):x.hook_add(UC_HOOK_CODE,hook,False,begin=a,end=a)
def compiled():
 global digest,calls
 x.mem_write(B,w(77,0,0,0,0,0)+f(1,-2,3));x.mem_write(B+0x100,w(values[0]));x.mem_write(B+0x200,w(*[B+0x300+i*8 for i in range(4)]))
 for i in range(4):x.mem_write(B+0x300+i*8,w(200+i,100+i))
 x.mem_write(B+0x500,w(0,B+0x100,4,B+0x200,*[B+0x8000+i*0x100 for i in range(4)]));x.mem_write(STACK,w(STOP,B,B+0x500));x.reg_write(UC_X86_REG_ESP,STACK);digest=2166136261;calls=0;x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 return w(x.reg_read(UC_X86_REG_EAX),digest,calls,word(x,B+0x100))
commands=[];expected=[];rng=random.Random(0x42776f)
for n in range(1024):
 values=[n%6 if n%6<5 else 0xffffffff,(n//6)%16,(n//96)%16,rng.getrandbits(32),(n//48)%2,0,0,0]
 u.mem_write(B+0x2c,w(77));u.mem_write(B+0x1c0,f(1,-2,3));u.mem_write(0x7c7634,w(values[0]));u.mem_write(0x7c75e4,w(*[B+0x2000+i*32 for i in range(4)]))
 for i in range(4):u.mem_write(B+0x2014+i*32,w(100+i))
 u.mem_write(STACK,w(STOP));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_EBX,B+0x1c0);digest=2166136261;calls=0
 # All <=0 counts return without the loop. Positive counts stop before epilogue.
 end=0x42780a if values[0] in (0,0xffffffff) else 0x4277e0
 u.emu_start(0x42776a,end,count=10000);assert u.reg_read(UC_X86_REG_EIP)==end
 result=w(0,digest,calls,word(u,0x7c7634));actual=compiled();assert actual==result,(n,values,result.hex(),actual.hex());commands.append(w(*values));expected.append(result)
for fail in range(1,5):
 values=[1,1,1,15,0,0,0,fail];result=compiled();assert result[0:4]==w(0xffffffff) and struct.unpack('<I',result[8:12])[0]==fail;commands.append(w(*values));expected.append(result)
values=[5,15,15,15,0,0,0,0];result=compiled();assert result==w(0xfffffffc,2166136261,0,5);commands.append(w(*values));expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--driller-feedback'],input=b''.join(commands));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=1024,callback_failures=4,capacity_guards=1,scope='Driller42776a..4277e0 iteration at actor lookup, shake, direction and mark boundaries. Exact order/arguments, absent/unlinked actors, negative/zero counts, count changed during shake, and full direction word forwarding. Numeric feedback callees and scene binding excluded.')
(root/'artifacts/driller-feedback.json').write_text(json.dumps(report,indent=2));print(report)
