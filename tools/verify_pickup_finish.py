"""Original SP459560 accepted path, with real timer and explicit service boundaries."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
word=lambda c,a:struct.unpack('<I',bytes(c.mem_read(a,4)))[0]
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();c=Uc(UC_ARCH_X86,UC_MODE_32);c.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);c.mem_write(p.OPTIONAL_HEADER.ImageBase,im);c.mem_map(B,0x10000);return c
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_pickup_finish_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
trace=0;values=[]
def hook(c,at,size,original):
 global trace
 if original and at==0x45995c:c.emu_stop();return
 if original and at==0x4597ed:
  assert c.mem_read(B+0x6050,1)==b'\1';trace=trace*10+3;return
 sp=c.reg_read(UC_X86_REG_ESP);a=sp+4
 stage=({0x45a3d0:1,B+0xa100:1,0x459490:2,0x48ab40:4,0x48a570:5,0x459520:6} if original else {B+0xa010:1,B+0xa020:2,B+0xa030:3,B+0xa040:4,B+0xa050:5,B+0xa060:6})[at];trace=trace*10+stage
 if stage==1:
  if not original:assert word(c,a+4)==values[5];a+=8
  assert [word(c,a+i*4) for i in range(3)]==values[:3]
  if original and at==0x45a3d0:
   assert [word(c,word(c,a+i*4)) for i in (3,4,5)]==[0xffffffff]*3
  c.mem_write(B+0x2ac if original else B+12,w(values[9]))
  if not original:c.mem_write(word(c,a+12),w(values[8]))
 else:
  if original:
   assert word(c,a)==(B+0x2000 if stage==2 else B)
   if stage==2:assert word(c,a+4)==B
  else:
   assert word(c,a+4)==B
   if stage==3:assert word(c,a+8)==values[6]
  if stage==5:
   c.mem_write(B+0x2ac if original else B+12,w(values[10]));c.mem_write(0x5a3ed8 if original else B+0x80,w(17))
 result=(values[8] if stage==1 else 0) if original else (0xffffffff if values[11]==stage else 0)
 c.reg_write(UC_X86_REG_EAX,result);c.reg_write(UC_X86_REG_EIP,word(c,sp));c.reg_write(UC_X86_REG_ESP,sp+4)
for at in (0x45a3d0,B+0xa100,0x459490,0x48ab40,0x48a570,0x459520,0x4597ed,0x45995c):u.hook_add(UC_HOOK_CODE,hook,True,begin=at,end=at)
for n in range(1,7):x.hook_add(UC_HOOK_CODE,hook,False,begin=B+0xa000+n*16,end=B+0xa000+n*16)
def original():
 global trace
 u.mem_write(B,bytes(0x9000));u.mem_write(B+0x202c,w(values[0]));u.mem_write(B+0x2c,w(values[1]));u.mem_write(B+0x2a4,w(values[2]));u.mem_write(B+0x2ac,w(values[3],values[4]));u.mem_write(B+0x4048,w(values[5]));u.mem_write(B+0x298,w(0));u.mem_write(0x5a3ed8,w(values[7]));u.mem_write(0x64ecb9,b'\0\0');u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_EDI,B+0x2000);u.reg_write(UC_X86_REG_EBX,B+0x4000);u.reg_write(UC_X86_REG_EBP,values[6]);trace=0;u.emu_start(0x45977c,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x45995c;return w(0,int(values[8]==0),word(u,B+0x2ac),word(u,B+0x2b0),word(u,0x5a3ed8),trace)
def compiled(blob):
 global trace
 x.mem_write(B,blob[:20]);x.mem_write(B+0x80,w(values[7]));x.mem_write(B+0x100,w(0,B+0x80,*[B+0xa000+n*16 for n in range(1,7)]));x.mem_write(B+0x200,w(0xa5a5a5a5));x.mem_write(STACK,w(STOP,B,values[5],values[6],B+0x100,B+0x200));x.reg_write(UC_X86_REG_ESP,STACK);trace=0;x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return w(x.reg_read(UC_X86_REG_EAX),word(x,B+0x200),word(x,B+12),word(x,B+16),word(x,B+0x80),trace)
rng=random.Random(0x45977c);cases=[];expected=[];routes={}
for n in range(1024):
 blob=w(rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.choice([-1,0,100]),rng.randrange(10000),rng.choice([0,B+0xa100]),rng.choice([0,B+0x6000]),rng.randrange(1072800001),rng.choice([0,0,0,1,256,0xffffffff]),rng.choice([-1,0,1,100]),rng.choice([-50,0,1,100,1072800000]),0);values=list(struct.unpack('<12I',blob));result=original();actual=compiled(blob);assert actual==result,(n,values,result.hex(),actual.hex());cases.append(blob);expected.append(result);t=struct.unpack('<6I',result)[5];routes[t]=routes.get(t,0)+1
for stage in range(1,7):
 blob=w(123,456,789,1,55,0,B+0x6000 if stage==3 else 0,99,0,-1 if stage==4 else 100,10,stage);values=list(struct.unpack('<12I',blob));result=compiled(blob);assert result==w(-1,0xa5a5a5a5,-1 if stage==4 else (10 if stage>=5 else 100),27 if stage==6 else 55,17 if stage>=5 else 99,[1,12,13,124,125,1256][stage-1]);cases.append(blob);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--pickup-finish'],input=b''.join(cases));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=1024,callback_failures=6,routes=routes,scope='Original45977c to45995c SP with actual4fa360 timer and player byte write. Default/custom grants, NPC reaction, hide/retire and pickup sound are explicit boundaries. Exact arguments, default output sentinels, post-grant/post-hide delay changes and callback failures. Service internals and live scene binding excluded.')
(root/'artifacts/pickup-finish.json').write_text(json.dumps(report,indent=2));print(report)
