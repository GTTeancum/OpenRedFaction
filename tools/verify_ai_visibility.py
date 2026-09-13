"""Original4ce740 query construction versus shared PC and NXDK; collision boundary supplied."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;NODE=B;POINT=B+0x100;OUT=B+0x200;STACK=B+0xe000;STOP=B+0xf000;CB=STOP+0x100
r=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(B,0x10000);return m
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_navigation_visible\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
trace=b'';contacts=0;failure=0;queries=0

def hook(m,address,size,context):
 global trace,queries
 original=m is u
 if address!=(0x4df1c0 if original else CB):return
 sp=m.reg_read(UC_X86_REG_ESP);arg=lambda i:r(m,sp+4+4*i)
 if original:
  q=arg(0);assert arg(2)==1 and r(m,q+0x50)==0x45
  assert bytes(m.mem_read(q+4,12))==bytes(12)
  assert bytes(m.mem_read(q+16,36))==f(1,0,0,0,1,0,0,0,1)
  trace=w(m.reg_read(UC_X86_REG_ECX))+bytes(m.mem_read(q+0x34,28));m.mem_write(arg(1),w(contacts));queries+=1
 else:
  trace=w(arg(1))+bytes(m.mem_read(arg(2),12))+bytes(m.mem_read(arg(3),12))+w(arg(4));m.mem_write(arg(5),w(contacts))
 m.reg_write(UC_X86_REG_EAX,0 if original else failure);m.reg_write(UC_X86_REG_EIP,r(m,sp));m.reg_write(UC_X86_REG_ESP,sp+4+(12 if original else 0))
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
def call(m,addr,args):
 m.mem_write(STACK,w(STOP,*args));m.reg_write(UC_X86_REG_ESP,STACK);m.reg_write(UC_X86_REG_FPCW,0x27f);m.emu_start(addr,STOP,count=10000);assert m.reg_read(UC_X86_REG_EIP)==STOP and m.reg_read(UC_X86_REG_ESP)==STACK+4;return m.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4ce740);commands=[];expected=[];accepted=0
for case in range(2048):
 world=0 if case%7==0 else 0x12345678;contacts=rng.choice((0,1,2,256,0xffffffff));coords=[rng.uniform(-10000,10000) for _ in range(6)];radius=rng.uniform(0,5);height=rng.getrandbits(32)
 wire=w(world,contacts,0)+f(*coords,radius)+w(height)
 for m in (u,x):m.mem_write(NODE,bytes(68));m.mem_write(NODE+12,wire[12:24]);m.mem_write(POINT,wire[24:36]);m.mem_write(OUT,w(99))
 trace=bytes(32);result=call(u,0x4ce740,[world,NODE,POINT,struct.unpack('<I',f(radius))[0],height])&255;wanttrace=trace;accepted+=result
 trace=bytes(32);status=call(x,entry,[world,NODE,POINT,struct.unpack('<I',f(radius))[0],height,CB,0,OUT]);assert status==0 and r(x,OUT)==result and trace==wanttrace,case
 commands.append(wire);expected.append(w(0,result,contacts,0)+wanttrace)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-visible'],input=b''.join(commands));assert actual==b''.join(expected),'PC'
failure=0xffffffff;x.mem_write(OUT,w(99));assert call(x,entry,[1,NODE,POINT,0,height,CB,0,OUT])==failure and r(x,OUT)==99
x.mem_write(OUT,w(99));assert call(x,entry,[0,0,0,0x7fc00000,height,0,0,OUT])==0 and r(x,OUT)==1
report=dict(result='PASS',original_pc_nxdk_cases=2048,queries=queries,accepted=accepted,compiled_guards=2,scope='Full4ce740 with actual vector helpers and constructors; only collision4df1c0 supplied. Exact identity/local flags0x45 hierarchy1 query, float displacement, radius, ignored height bits, null-world bypass and full contact-count acceptance. No actual collision geometry or scene binding.')
(root/'artifacts/ai-visibility.json').write_text(json.dumps(report,indent=2));print(report)
