"""Original427550 surface-route gates, with real numeric and class callees."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_timed_contact\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
PERIOD=1072800000;word=lambda cpu,a:struct.unpack('<I',bytes(cpu.mem_read(a,4)))[0]
words=lambda cpu,a,n:list(struct.unpack('<'+'I'*n,bytes(cpu.mem_read(a,4*n))))
values=[];digest=calls=0
def hook(cpu,at,size,original):
 global digest,calls
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if original:
  a=words(cpu,sp+4,8);row=a[:2]+words(cpu,a[2],3)+words(cpu,a[3],3)+a[4:]
 else:row=words(cpu,word(cpu,sp+8),12)
 calls+=1
 for v in row:digest=((digest^v)*16777619)&0xffffffff
 if values[12]:
  clock=0x5a3ed8 if original else B+36;now=word(cpu,clock);now=now-(PERIOD-17) if now>PERIOD-17 else now+17;cpu.mem_write(clock,w(now));cpu.mem_write(B+0x1414 if original else B,w(7))
 cpu.reg_write(UC_X86_REG_EAX,0xffffffff if not original and values[13] else 0);cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook,True,begin=0x4c16e0,end=0x4c16e0);x.hook_add(UC_HOOK_CODE,hook,False,begin=B+0x8000,end=B+0x8000)
def compiled(blob):
 global digest,calls
 x.mem_write(B,blob[:48]);x.mem_write(B+0x100,w(0,B+36,B+40,B+0x8000));x.mem_write(STACK,w(STOP,B,B+0x100));x.reg_write(UC_X86_REG_ESP,STACK);digest=2166136261;calls=0;x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,40))+w(digest,calls)
commands=[];expected=[];rng=random.Random(0x427562);emissions=0
for n in range(2048):
 now=rng.choice([0,PERIOD,PERIOD-2000,PERIOD-1999,rng.randrange(PERIOD+1)]);deadline=rng.choice([-1,0,now,PERIOD,rng.randrange(PERIOD+1)])
 radius=struct.unpack('<f',w([0x401fffff,0x40200000,0x40200001][n%3]))[0] if n%2 else rng.uniform(0,8)
 blob=w(deadline&0xffffffff)+f(radius)+w(rng.getrandbits(32))+f(*[rng.uniform(-10,10) for _ in range(6)])+w(now,rng.getrandbits(32),rng.getrandbits(32),n%2,0);values=struct.unpack('<14I',blob)
 u.mem_write(B,bytes(0x1800));u.mem_write(B,w(values[2]));u.mem_write(B+0x1414,blob[:4]);u.mem_write(B+0x78,blob[4:8]);u.mem_write(B+0x3c,blob[12:24]);u.mem_write(B+0x1b4,blob[24:36]);u.mem_write(B+0x1ec,w(1));u.mem_write(0x5a3ed8,w(now));u.mem_write(0x856854,blob[40:48]);u.mem_write(STACK,w(STOP));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ECX,B);digest=2166136261;calls=0;u.emu_start(0x427550,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_EAX)==2
 result=w(0,word(u,B+0x1414))+blob[4:36]+w(word(u,0x5a3ed8),digest,calls);emissions+=calls;actual=compiled(blob);assert actual==result,(n,values,result.hex(),actual.hex());commands.append(blob);expected.append(result)
blob=w(0)+f(2.5)+w(3)+f(1,2,3,4,5,6)+w(0,4,5,1,1);values=struct.unpack('<14I',blob);result=compiled(blob);assert result[:8]==w(0xffffffff,7) and result[40:44]==w(17);commands.append(blob);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--timed-contact'],input=b''.join(commands));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=2048,emissions=emissions,callback_failure_cases=1,scope='Full original427550 nonzero1ec branch with actual4fa3f0/4fa360 and40a490.4c16e0 emission boundary only supplied. Exact radius threshold/arguments, disabled and wrapped deadlines, post-callback clock/reset, error preserves completed mutation. Effect internals and scene binding excluded.')
(root/'artifacts/timed-contact.json').write_text(json.dumps(report,indent=2));print(report)
