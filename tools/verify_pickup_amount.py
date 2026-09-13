"""Original45a500 accepted pickup quantity math against PC and compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_FPCW
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
word=lambda c,a:struct.unpack('<I',bytes(c.mem_read(a,4)))[0]
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();c=Uc(UC_ARCH_X86,UC_MODE_32);c.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);c.mem_write(p.OPTIONAL_HEADER.ImageBase,im);c.mem_map(B,0x10000);return c
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_weapon_pickup_amount\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
amount=message=None

def hook(c,at,size,data):
 global amount,message
 sp=c.reg_read(UC_X86_REG_ESP);assert word(c,sp+4)==B
 if at==0x428d90:assert word(c,sp+8)==7;amount=word(c,sp+12)
 else:assert word(c,sp+8)==B+0x5000 and word(c,sp+16)==0;message=word(c,sp+12)
 c.reg_write(UC_X86_REG_EIP,word(c,sp));c.reg_write(UC_X86_REG_ESP,sp+4)
for at in (0x428d90,0x45a100):u.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
def original(blob):
 global amount,message
 quantity,reserve,capacity,difficulty,special,disabled=struct.unpack('<6I',blob)
 u.mem_write(B,bytes(0x6000));u.mem_write(B+0x7c,w(8));u.mem_write(B+0x2ac,w(reserve));u.mem_write(B+0x4294,w(B+0x5000));u.mem_write(0x85cd2c+7*0x550,w(0));u.mem_write(0x85cf68+7*0x550,w(capacity));u.mem_write(0x87243c,w(7 if special else 8));u.mem_write(0x872468,w(9));u.mem_write(0x593e54,w(difficulty));u.mem_write(0x64ecb9,b'\0');u.mem_write(0x6fc4d8,bytes([disabled]));u.mem_write(STACK,w(0,0,0,STOP,B+0x4000,0,7,quantity));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_EBP,B);u.reg_write(UC_X86_REG_EBX,7);u.reg_write(UC_X86_REG_EDI,7*0x550);u.reg_write(UC_X86_REG_FPCW,0x37f);amount=message=None;u.emu_start(0x45a5a0,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP and amount is not None and message is not None;return w(0,amount,message)
def compiled(blob):
 vals=struct.unpack('<6I',blob);x.mem_write(B,w(0xa5a5a5a5,0xa5a5a5a5));x.mem_write(STACK,w(STOP,*vals,B,B+4));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,8))
rng=random.Random(0x45a5a0);cases=[];expected=[]
for n in range(8192):
 quantity=rng.choice([-2147483648,-1,0,1,49,50,99,100,101,2147483647,rng.randint(-2147483648,2147483647)]);blob=w(quantity,rng.randint(-2147483648,2147483647),rng.randint(-2147483648,2147483647),n%4,(n//4)%2,(n//8)%2);result=original(blob);actual=compiled(blob);assert actual==result,(n,struct.unpack('<6i',blob),result.hex(),actual.hex());cases.append(blob);expected.append(result)
blob=w(100,0,100,4,0,0);result=compiled(blob);assert result==w(-4,0xa5a5a5a5,0xa5a5a5a5);cases.append(blob);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--pickup-amount'],input=b''.join(cases));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=8192,bounds_guards=1,scope='Original45a5a0 to45a6c6 accepted SP quantity path, with real ftol,40a510 and4895d0.428d90 inventory and45a100 message boundaries record amounts. All four authored difficulty scales, special units100, scale bypass, full signed input range, capacity subtraction wrap and message conversion. Earlier acceptance, callback mutations and live binding excluded.')
(root/'artifacts/pickup-amount.json').write_text(json.dumps(report,indent=2));print(report)
