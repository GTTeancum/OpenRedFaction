"""Full original40c570 navigation candidate predicate against PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<%dI'%len(v),*v)
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
 u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(b)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,b);u.mem_map(base,0x10000);return u
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_navigation_candidate_allowed\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def call(cpu,address,args):
 cpu.mem_write(stack,w(stop,*args));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(address,stop,count=10000)
 assert cpu.reg_read(UC_X86_REG_EIP)==stop;return cpu.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x40c570);special=[0,0x80000000,0x3f800000,0x3f7fffff,0x3f800001,0xbf800000,0x7f800000,0xff800000,0x7fc12345,0xffc12345,1,0x80000001]
commands=[];expected=[];counts=[0,0]
for case in range(4096):
 dims=[rng.choice(special) if case<2048 else rng.getrandbits(32) for _ in range(4)]
 if case%4==0:dims[2:]=dims[:2]
 mode=rng.choice((0,1,2,255,256,257,0xffffff01));blocked=rng.choice((0,1,0xffffffff,0x100))
 radius,height,cr,ch=dims;seed=bytearray(rng.randbytes(0x80));seed[0x1c:0x24]=w(cr,ch);seed[0x40:0x44]=w(blocked)
 u.mem_write(base,bytes(seed));result=call(u,0x40c570,[radius,height,mode,base])&255
 assert result in (0,1) and bytes(u.mem_read(base,0x80))==seed
 args=[radius,height,mode,cr,ch,blocked];actual=call(x,entry,args)
 assert actual==result,(case,args,result,actual)
 commands.append(w(*args));expected.append(w(result));counts[result]+=1
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--navigation-candidate'],input=b''.join(commands));assert actual==b''.join(expected)
report=dict(result='PASS',cases=4096,rejected=counts[0],accepted=counts[1],scope='Complete original40c570, no hooks; normalized AL compared with PC and compiled NXDK. Random float bits, signed zeros, infinities, NaNs, adjacent finite boundaries, equality, exact low-byte mode1 and arbitrary word40. Original node bytes unchanged. Does not select candidates, compute geometry scores or run scene/XEMU navigation.')
(root/'artifacts/navigation-candidate-verification.json').write_text(json.dumps(report,indent=2));print(report)
