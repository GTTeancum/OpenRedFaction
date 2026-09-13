"""Original427550 surface-route gates, with real numeric and class callees."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_FPCW
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_contact_apc_effects\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
word=lambda cpu,a:struct.unpack('<I',bytes(cpu.mem_read(a,4)))[0]
words=lambda cpu,a,n:list(struct.unpack('<'+'I'*n,bytes(cpu.mem_read(a,4*n))))
values=[];digest=calls=0
def record(event,row):
 global digest,calls
 calls+=1
 for v in [event]+row:digest=((digest^v)*16777619)&0xffffffff
def name(cpu,a):
 b=bytearray()
 while cpu.mem_read(a,1)!=b'\0':b+=cpu.mem_read(a,1);a+=1
 return b.decode()
def hook(cpu,at,size,original):
 sp=cpu.reg_read(UC_X86_REG_ESP);a=words(cpu,sp+4,8)
 event=({0x503220:1,0x437570:2,0x5034f0:3,0x467020:4,0x4892c0:5} if original else {B+0x8000+i*0x100:i+1 for i in range(5)})[at]
 if not original:a=a[1:]
 result=0
 if event==1:
  assert name(cpu,a[1])=='bumper_1';record(1,[a[0]])
  if original:result=values[16]
  else:cpu.mem_write(a[2],w(values[16]))
  if values[18]:addr=B+0x80 if original else B+8;cpu.mem_write(addr,w(word(cpu,addr)^123))
 elif event==2:
  assert name(cpu,a[0])=='Holey_APC.v3d';record(2,[])
  if original:result=values[17]
  else:cpu.mem_write(a[1],w(values[17]))
 elif event==3:
  record(3,a[:2]+words(cpu,a[2],9)+words(cpu,a[3],3)+words(cpu,a[4],9)+words(cpu,a[5],3))
  pos=struct.unpack('<3f',bytes(cpu.mem_read(a[3],12)));cpu.mem_write(a[5],f(*[v+1 for v in pos]))
 elif event==4:
  record(4,a[:3]+words(cpu,a[3],3)+words(cpu,a[4],3)+a[5:7] if original else words(cpu,a[0],11))
  if values[18]:addr=B+0x2c if original else B;cpu.mem_write(addr,w(word(cpu,addr)^456))
 else:record(5,a[:8] if original else [word(cpu,a[0])]+words(cpu,a[1],2)+[0]+words(cpu,a[1]+8,4))
 if not original:result=0xffffffff if values[19]==event else 0
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for a in (0x503220,0x437570,0x5034f0,0x467020,0x4892c0):u.hook_add(UC_HOOK_CODE,hook,True,begin=a,end=a)
for a in range(B+0x8000,B+0x8500,0x100):x.hook_add(UC_HOOK_CODE,hook,False,begin=a,end=a)
def compiled(blob):
 global digest,calls
 x.mem_write(B,blob[:64]);x.mem_write(B+0x100,w(0,*[B+0x8000+i*0x100 for i in range(5)]));x.mem_write(STACK,w(STOP,B,B+0x100));x.reg_write(UC_X86_REG_ESP,STACK);digest=2166136261;calls=0;x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,64))+w(digest,calls)
commands=[];expected=[];rng=random.Random(0x42764d)
for n in range(1024):
 blob=w(*[rng.getrandbits(32) for _ in range(3)])+f(*[rng.uniform(-16,16) for _ in range(13)])+w(rng.getrandbits(32),rng.getrandbits(32),n%2,0);values=struct.unpack('<20I',blob)
 u.mem_write(B,bytes(0x1000));u.mem_write(B,w(values[1]));u.mem_write(B+0x2c,w(values[0]));u.mem_write(B+0x80,w(values[2]));u.mem_write(B+0x78,blob[12:16]);u.mem_write(B+0x3c,blob[16:28]);u.mem_write(B+0x48,blob[28:64]);u.mem_write(STACK,bytes(256));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_EBP,B+0x60);digest=2166136261;calls=0
 u.emu_start(0x42764d,0x4276d8,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x4276d8
 state=w(word(u,B+0x2c),values[1],word(u,B+0x80))+blob[12:64];result=w(0)+state+w(digest,calls);actual=compiled(blob);assert actual==result,(n,result.hex(),actual.hex());commands.append(blob);expected.append(result)
for failure in range(1,6):
 blob=commands[1][:-4]+w(failure);values=struct.unpack('<20I',blob);result=compiled(blob);assert result[:4]==w(0xffffffff) and struct.unpack('<I',result[-4:])[0]==failure;commands.append(blob);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--apc-contact-effects'],input=b''.join(commands));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=1024,callback_failures=5,scope='Original42764d..4276d8 with actual zero constructors and40a490 room lookup. Exact bumper/model names, placement inputs/default outputs, geometry/damage arguments, order and callback model/handle mutations. Asset lookup/placement/geometry/damage implementations and live binding excluded.')
(root/'artifacts/apc-contact-effects.json').write_text(json.dumps(report,indent=2));print(report)
