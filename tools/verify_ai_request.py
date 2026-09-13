"""Original4cebd0 request ordering versus shared PC/NXDK, with route services supplied."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
B=0x30000000;GOAL=B;FIRST=B+0x100;START=B+0x200;END1=B+0x300;END2=B+0x400;Q=B+0x1000;LIST=B+0x2000;DATA=B+0x3000;ROUTE=B+0x4000;BE=B+0x5000;OUT=B+0x6000;STACK=B+0xe000;STOP=B+0xf000;CB=[STOP+0x100+i*16 for i in range(6)]
r=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(B,65536);return m
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_navigation_request_run\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
config=[];calls=0;lists=[]
def hook(m,a,size,ctx):
 global calls,lists
 original=m is u;sp=m.reg_read(UC_X86_REG_ESP);arg=lambda i:r(m,sp+4+4*i)
 ops=[0x40cb40,0x4ce4b0,0x4ce800,0x4ce860,0x4ce8c0,0x4ce390] if original else CB
 if a not in ops:return
 step=ops.index(a)+1
 if original and step==6:
  this=m.reg_read(UC_X86_REG_ECX)
  if this!=END2+40:return
  calls|=1<<6;m.mem_write(GOAL+12,f(40,50,60));return # Actual removal executes.
 calls|=1<<step
 if original and step==1:return # Actual list reset executes.
 status=0;value=0
 if not original and config[5]==step:status=0xfffffffe
 elif step==2:m.mem_write(GOAL+12,f(10,20,30))
 elif step==3:value=config[1]
 elif step==4:
  value=config[2]
  if value&255==1:
   if original:
    for p in (LIST,END1+40,END2+40):m.mem_write(p,w(4))
   else:lists=[4]*3
 elif step==5:m.mem_write(Q+52 if original else ROUTE+16,w(3));value=config[3]
 elif step==6:lists=[v-1 for v in lists];m.mem_write(GOAL+12,f(40,50,60))
 if not original and step in (3,4,5) and not status:m.mem_write(arg(1),w(value))
 m.reg_write(UC_X86_REG_EAX,value if original else status);m.reg_write(UC_X86_REG_EIP,r(m,sp));m.reg_write(UC_X86_REG_ESP,sp+4+({2:4,3:8,4:4,5:8}.get(step,0) if original else 0))
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
def run(m,original):
 global calls,lists
 calls=0;lists=[3]*3
 for a in (GOAL,FIRST,START,END1,END2):m.mem_write(a,bytes(68))
 m.mem_write(GOAL,f(1,2,3,4,5,6));m.mem_write(GOAL+53,b'\x07');m.mem_write(FIRST+53,b'\x08');m.mem_write(OUT,w(99))
 if original:
  for i,p in enumerate((LIST,END1+40,END2+40,START+40)):m.mem_write(p,w(3,8,DATA+i*32));m.mem_write(DATA+i*32,w(*range(8)))
  m.mem_write(Q,bytes(64));m.mem_write(Q,w(START,FIRST if config[4] else 0,0,GOAL,END1,END2));m.mem_write(Q+34,bytes([config[0]&255]));m.mem_write(Q+52,w(9));args=[Q];addr=0x4cebd0
 else:
  m.mem_write(ROUTE,bytes(16)+w(9));m.mem_write(Q,w(GOAL,FIRST if config[4] else 0,ROUTE,config[0]));m.mem_write(BE,w(*CB,0));args=[Q,BE,OUT];addr=entry
 m.mem_write(STACK,w(STOP,*args));m.reg_write(UC_X86_REG_ESP,STACK);m.reg_write(UC_X86_REG_ECX,LIST);m.emu_start(addr,STOP,count=10000);assert m.reg_read(UC_X86_REG_EIP)==STOP
 if original:status=0;result=m.reg_read(UC_X86_REG_EAX)&255;count=r(m,Q+52);lists=[r(m,p) for p in (LIST,END1+40,END2+40)]
 else:status=m.reg_read(UC_X86_REG_EAX);result=r(m,OUT);count=r(m,ROUTE+16)
 return w(status,result,count)+bytes(m.mem_read(GOAL,24))+w(m.mem_read(GOAL+53,1)[0],m.mem_read(FIRST+53,1)[0],*lists,calls)
rng=random.Random(0x4cebd0);commands=[];expected=[]
for i in range(2048):
 config=[rng.choice((0,1,2,256,257)),*[rng.choice((0,1,2,256,257,255)) for _ in range(3)],i&1,0]
 want=run(u,True);got=run(x,False);assert got==want,(i,config,got.hex(),want.hex());commands.append(w(*config));expected.append(want)
# Added error policy: always remove successful temporary insertion after search failure.
for step in range(1,7):
 config=[1,1,1,1,1,step];got=run(x,False);assert struct.unpack_from('<2I',got)==(0xfffffffe,99)
 if step==5:assert calls&(1<<6) and lists==[3]*3
 commands.append(w(*config));expected.append(got)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-request'],input=b''.join(commands));assert actual==b''.join(expected),'PC'
report=dict(result='PASS',original_pc_nxdk_cases=2048,callback_failure_cases=6,scope='Full4cebd0 with actual clear/list counts/removal/vector copy; prepare/connect/search services supplied. Exact branch bytes, route counts, rejection bytes, post-cleanup goal copy and stage order. Shared errors preserve output and search failure still cleans temporary links. Not concrete combined graph or scene ownership.')
(root/'artifacts/ai-request.json').write_text(json.dumps(report,indent=2));print(report)
