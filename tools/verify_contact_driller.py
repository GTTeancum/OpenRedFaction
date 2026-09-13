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
entry=int(re.search(r'\s_rf_entity_contact_driller_effects\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
word=lambda cpu,at:struct.unpack('<I',bytes(cpu.mem_read(at,4)))[0]
words=lambda cpu,at,n:list(struct.unpack('<'+'I'*n,bytes(cpu.mem_read(at,n*4))))
hash_value=count=0;blob=b''
def record(event,row):
 global hash_value,count
 count+=1
 for v in [event]+row:hash_value=((hash_value^v)*16777619)&0xffffffff
def hook(cpu,at,size,original):
 sp=cpu.reg_read(UC_X86_REG_ESP);a=words(cpu,sp+4,3)
 event=1 if at in (0x467020,B+0x8000) else 2
 if original:
  a=words(cpu,sp+4,7 if event==1 else 8)
  row=a[:3]+words(cpu,a[3],3)+words(cpu,a[4],3)+a[5:] if event==1 else a
 else:row=words(cpu,a[1],11) if event==1 else [word(cpu,a[1])]+words(cpu,a[2],2)+[0xffffffff]+words(cpu,a[2]+8,4)
 record(event,row)
 if event==1 and word_input[9]:
  addr=B+0x2c if original else B;cpu.mem_write(addr,w(word(cpu,addr)^0x12345678))
 cpu.reg_write(UC_X86_REG_EAX,0xffffffff if not original and word_input[10]==event else 0)
 cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for at in (0x467020,0x4892c0):u.hook_add(UC_HOOK_CODE,hook,True,begin=at,end=at)
for at in (B+0x8000,B+0x8100):x.hook_add(UC_HOOK_CODE,hook,False,begin=at,end=at)
def compiled(blob):
 global hash_value,count
 x.mem_write(B,blob[:36]);x.mem_write(B+0x100,w(0,B+0x8000,B+0x8100));x.mem_write(STACK,w(STOP,B,B+0x100));x.reg_write(UC_X86_REG_ESP,STACK);hash_value=2166136261;count=0
 x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,36))+w(hash_value,count)
rng=random.Random(0x42772b);commands=[];expected=[]
for n in range(1024):
 blob=w(rng.getrandbits(32),rng.getrandbits(32))+f(rng.uniform(0,20),*[rng.uniform(-100,100) for _ in range(6)])+w(n%2,0);word_input=struct.unpack('<11I',blob)
 u.mem_write(B,bytes(0x2000));u.mem_write(B,w(word_input[1]));u.mem_write(B+0x2c,blob[:4]);u.mem_write(B+0x78,blob[8:12]);u.mem_write(B+0x1b4,blob[12:24]);u.mem_write(B+0x1c0,blob[24:36]);u.mem_write(STACK,w(STOP));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_EBX,B+0x1c0);hash_value=2166136261;count=0
 # Enter after route gate; stop immediately after damage, before player-list tail.
 u.emu_start(0x42772b,0x42776a,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x42776a
 result=w(0,word(u,B+0x2c))+blob[4:36]+w(hash_value,count)
 assert compiled(blob)==result,(n,result.hex(),compiled(blob).hex());commands.append(blob);expected.append(result)
for failure in (1,2):
 blob=commands[1][:-4]+w(failure);word_input=struct.unpack('<11I',blob);result=compiled(blob);assert result[:4]==w(0xffffffff);assert struct.unpack('<I',result[-4:])[0]==failure;commands.append(blob);expected.append(result)
for radius in (0x7fc00000,0x7f7fffff):
 blob=bytearray(commands[0]);blob[8:12]=w(radius);blob=bytes(blob);word_input=struct.unpack('<11I',blob);result=compiled(blob);assert result==w(0xfffffffc)+blob[:36]+w(2166136261,0);commands.append(blob);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--contact-driller'],input=b''.join(commands));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=1024,callback_failures=2,radius_guards=2,scope='Original42772b..42776a with actual40a490 room accessor. Exact467020 seven-argument geometry request and4892c0 eight-argument damage request, order and post-geometry handle mutation. Callback bodies, linked-player feedback and live scheduling excluded.')
(root/'artifacts/contact-driller.json').write_text(json.dumps(report,indent=2));print(report)
