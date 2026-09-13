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
entry=int(re.search(r'\s_rf_entity_contact_dispatch\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
word=lambda cpu,a:struct.unpack('<I',bytes(cpu.mem_read(a,4)))[0]
trace=0;values=[]
def original_hook(cpu,at,size,data):
 global trace
 if at==0x40a0e0:
  trace=5;sp=cpu.reg_read(UC_X86_REG_ESP);assert word(cpu,sp+4)==values[1];cpu.reg_write(UC_X86_REG_EAX,B+0x2000 if values[17] else 0);cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
 else:trace={0x427562:1,0x42764d:3,0x42772b:4,0x427802:2}[at];cpu.reg_write(UC_X86_REG_EAX,2);cpu.emu_stop()
for a in (0x427562,0x42764d,0x42772b,0x427802,0x40a0e0):u.hook_add(UC_HOOK_CODE,original_hook,begin=a,end=a)
def native_hook(cpu,at,size,data):
 global trace
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if at==B+0x8000:trace=1
 elif at==B+0x8100:trace=word(cpu,sp+8)+1
 else:
  trace=5;assert word(cpu,sp+8)==values[1];cpu.mem_write(B+0x400,w(values[1],2));cpu.mem_write(word(cpu,sp+12),w(B+0x400 if values[17] else 0))
 cpu.reg_write(UC_X86_REG_EAX,0xffffffff if values[18] else 0);cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for a in (B+0x8000,B+0x8100,B+0x8200):x.hook_add(UC_HOOK_CODE,native_hook,begin=a,end=a)
def compiled(blob):
 global trace
 x.mem_write(B,blob[:64]);x.mem_write(B+0x100,w(0,0,0,values[16]));x.mem_write(B+0x200,w(0,B+0x8000,B+0x8100,B+0x300));x.mem_write(B+0x300,w(0,B+0x8200,0,0,0));x.mem_write(B+0x500,w(0xa5a5a5a5));x.mem_write(STACK,w(STOP,B,B+0x100,B+0x200,B+0x500));x.reg_write(UC_X86_REG_ESP,STACK);trace=0;x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 return w(x.reg_read(UC_X86_REG_EAX),word(x,B+0x500),trace)
rng=random.Random(0x427550);commands=[];expected=[];routes={}
for n in range(4096):
 special=17 if n%4==0 else 0;inverse=1 if n%4==1 else -0.0 if n%4==2 else 0.0
 blob=w(special,rng.getrandbits(32))+f(inverse)+w([0,0x200,0x400,0x600][(n//4)%4],(n//16)%3,8 if (n//48)%2 else 0)+f(*[rng.uniform(-8,8) for _ in range(10)])+w(8 if n%7==0 else 0,int(n%5!=0),0);values=struct.unpack('<19I',blob)
 u.mem_write(B,bytes(0x5000));u.mem_write(B+0x294,w(B+0x4000));u.mem_write(B+0x4724,w(values[3]));u.mem_write(B+0x41b4,w(values[4]));u.mem_write(B+0x1d0,w(values[5]));u.mem_write(B+0x8c0,blob[24:28]);u.mem_write(B+0x144,blob[28:40]);u.mem_write(B+0x60,blob[40:52]);u.mem_write(B+0x1c0,blob[52:64]);u.mem_write(B+0x1ec,w(special));u.mem_write(B+0x1d4,f(inverse));u.mem_write(B+0x1e4,w(values[1]));u.mem_write(B+0x7c,w(values[16]));u.mem_write(B+0x2024,w(2));u.mem_write(STACK,w(STOP));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ECX,B);u.reg_write(UC_X86_REG_FPCW,0x37f);trace=0;u.emu_start(0x427550,STOP,count=10000)
 result=w(0,u.reg_read(UC_X86_REG_EAX),trace);routes[trace]=routes.get(trace,0)+1;assert compiled(blob)==result,(n,result.hex(),compiled(blob).hex());commands.append(blob);expected.append(result)
for route in range(1,6):
 blob=next(b for b,e in zip(commands,expected) if struct.unpack('<III',e)[2]==route);blob=blob[:-4]+w(1);values=struct.unpack('<19I',blob);result=compiled(blob);assert result==w(0xffffffff,0xa5a5a5a5,route);commands.append(blob);expected.append(result)
# Nonfinite inverse mass ignored for special branch, rejected otherwise.
for special in (0,1):
 blob=bytearray(commands[0]);blob[:4]=w(special);blob[8:12]=w(0x7fc00000);blob=bytes(blob);values=struct.unpack('<19I',blob);result=compiled(blob);assert result== (w(0,2,1) if special else w(0xfffffffc,0xa5a5a5a5,0));commands.append(blob);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--contact-dispatch'],input=b''.join(commands));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=4096,routes=routes,callback_failure_cases=5,finite_precedence_cases=2,scope='Original427550 route/response comparison with timed and vehicle/sound effect bodies as explicit service boundaries; actual numeric gates/class predicates and object lookup branch. Object service uses absent/type2 fixtures; other object types verified separately. Backend internals and live scene scheduling excluded.')
(root/'artifacts/contact-dispatch.json').write_text(json.dumps(report,indent=2));print(report)
