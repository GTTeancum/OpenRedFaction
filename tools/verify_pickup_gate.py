"""Execute the original SP pickup eligibility prefix against PC and NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
word=lambda c,a:struct.unpack('<I',bytes(c.mem_read(a,4)))[0]
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();c=Uc(UC_ARCH_X86,UC_MODE_32);c.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);c.mem_write(p.OPTIONAL_HEADER.ImageBase,im);c.mem_map(B,0x10000);return c
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_pickup_prepare_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
trace=token=eligible=lookups=0;values=[];blob=b''
def hook(c,at,size,original):
 global trace,token,eligible,lookups
 if original and at in (0x45977c,0x45995c):eligible=int(at==0x45977c);c.emu_stop();return
 sp=c.reg_read(UC_X86_REG_ESP);args=sp+4 if original else sp+8
 stage=({0x426fc0:1,0x4a3740:2,0x498e80:3} if original else {B+0xa000:1,B+0xa010:2,B+0xa020:3})[at];trace=trace*10+stage
 if stage==1:
  assert word(c,args)==values[3];kind=values[15] if lookups else values[11];lookups+=1;result=B+0x6000 if kind!=0xffffffff else 0
  if original:c.mem_write(B+0x81b4,w(kind))
  if not original:c.mem_write(word(c,args+4),w(kind))
 elif stage==2:
  assert word(c,args)==values[2];result=token=values[12]
  if not original:c.mem_write(word(c,args+4),w(result))
 else:
  assert bytes(c.mem_read(word(c,args),12))==blob[20:32];assert bytes(c.mem_read(word(c,args+4),12))==blob[32:44]
  if original:assert word(c,args+8)==1 and word(c,args+12)==0
  result=values[13]
  if not original:c.mem_write(word(c,args+8),w(result))
 if not original:result=0xffffffff if values[14]==stage else 0
 c.reg_write(UC_X86_REG_EAX,result);c.reg_write(UC_X86_REG_EIP,word(c,sp));c.reg_write(UC_X86_REG_ESP,sp+4)
for at in (0x426fc0,0x4a3740,0x498e80,0x45977c,0x45995c):u.hook_add(UC_HOOK_CODE,hook,True,begin=at,end=at)
for at in (B+0xa000,B+0xa010,B+0xa020):x.hook_add(UC_HOOK_CODE,hook,False,begin=at,end=at)
def original():
 global trace,token,eligible,lookups
 u.mem_write(B,bytes(0x9000));u.mem_write(B+0x294,w(B+0x5000));u.mem_write(B+0x2bc,w(values[0]));u.mem_write(B+0x3c,blob[20:32]);u.mem_write(B+0x2294,w(B+0x4000));u.mem_write(B+0x2810,w(values[1]));u.mem_write(B+0x202c,w(values[2]));u.mem_write(B+0x2200,w(values[3]));u.mem_write(B+0x27d4,blob[32:44]);u.mem_write(B+0x6294,w(B+0x8000));u.mem_write(0x64ecb9,b'\0\0');u.mem_write(STACK,w(STOP,B,B+0x2000,values[4],0));u.reg_write(UC_X86_REG_ESP,STACK);trace=token=lookups=0;eligible=None;u.emu_start(0x459560,STOP,count=10000);assert eligible is not None;return w(0,token,eligible,trace)
def compiled():
 global trace,lookups
 x.mem_write(B,blob[:44]);x.mem_write(B+0x100,w(0,B+0xa000,B+0xa010,B+0xa020));x.mem_write(B+0x200,w(0xa5a5a5a5,0xa5a5a5a5));x.mem_write(STACK,w(STOP,B,B+0x100,B+0x200,B+0x204));x.reg_write(UC_X86_REG_ESP,STACK);trace=lookups=0;x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+0x200,8))+w(trace)
rng=random.Random(0x459560);cases=[];expected=[];accepted=0
for n in range(2048):
 blob=w(rng.choice([0,0,0,1,0x100]),rng.choice([0,0,0,1,2]),rng.getrandbits(32),rng.getrandbits(32),rng.choice([0,1,2,255,256,257]))+struct.pack('<6f',*[rng.uniform(-500,500) for _ in range(6)])+w(rng.choice([0xffffffff,0,1,4]),rng.choice([0,0x12345678]),rng.choice([0,1,2,255,256,257]),0,rng.choice([0xffffffff,0,1,4]));values=struct.unpack('<16I',blob);result=original();actual=compiled();assert actual==result,(n,values,result.hex(),actual.hex());accepted+=struct.unpack('<4I',result)[2];cases.append(blob);expected.append(result)
for failure in (1,2,3):
 blob=w(0,0,123,456,1)+struct.pack('<6f',1,2,3,4,5,6)+w(0xffffffff,789,0,failure,0xffffffff);values=struct.unpack('<16I',blob);result=compiled();assert result==w(0xffffffff,0xa5a5a5a5,0xa5a5a5a5,[1,112,1123][failure-1]);cases.append(blob);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--pickup-gate'],input=b''.join(cases));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=2048,eligible=accepted,callback_failure_cases=3,scope='Original459560 SP prefix through45977c or45995c with actual4290d0/429f90/427020/486c90. Linked lookup, player lookup and LOS boundaries verify arguments and ordering. Grant, retirement, notifications, multiplayer and live scene binding excluded.')
(root/'artifacts/pickup-gate.json').write_text(json.dumps(report,indent=2));print(report)
