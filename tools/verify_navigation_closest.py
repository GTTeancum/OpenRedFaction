"""Full509100 segment projection versus shared PC/compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
base=0x30000000;node=base+0x100;out=base+0x200;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<%dI'%len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<%df'%len(v),*v)
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(b)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,b);u.mem_map(base,0x10000);return u
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_navigation_closest_point\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def call(cpu,address,args):
 cpu.mem_write(stack,w(stop,*args));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(address,stop,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==stop;return cpu.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x509100);commands=[];expected=[]
u.mem_write(stop,b'\xd9\x1d'+w(out+12))
for case in range(4096):
 values=[rng.uniform(-100,100) for _ in range(9)]
 if case%8==0:values[6:9]=values[3:6]
 elif case%8==1:values[:3]=values[3:6]
 elif case%8==2:values[:3]=values[6:9]
 wire=f(*values);u.mem_write(base,wire);u.mem_write(out,bytes([0xa5])*16)
 call(u,0x509100,[base,base+12,base+24,out]);u.emu_start(stop,stop+6,count=1)
 want=bytes(u.mem_read(out,16));assert bytes(u.mem_read(base,36))==wire
 x.mem_write(base,wire);x.mem_write(out,bytes([0xa5])*16)
 assert call(x,entry,[base,base+12,base+24,out,out+12])==0
 assert bytes(x.mem_read(out,16))==want,(case,want.hex(),bytes(x.mem_read(out,16)).hex())
 assert bytes(x.mem_read(base,36))==wire
 commands.append(wire);expected.append(w(0)+want)
for offset in range(0,36,4):
 wire=bytearray(commands[0]);wire[offset:offset+4]=w(0x7fc12345);x.mem_write(base,bytes(wire));x.mem_write(out,bytes([0xa5])*16)
 status=call(x,entry,[base,base+12,base+24,out,out+12]);assert status!=0 and bytes(x.mem_read(out,16))==bytes([0xa5])*16
 commands.append(bytes(wire));expected.append(w(status)+bytes([0xa5])*16)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--navigation-closest'],input=b''.join(commands));assert actual==b''.join(expected),'PC mismatch'
report=dict(result='PASS',cases=4096,coincident_endpoints=512,nonfinite_guards=9,scope='Full original509100 and all original callees, no hooks;53-bit x87. Exact closest point and float-stored returned distance vs PC/compiled NXDK, randomized geometry, coincident endpoints and points on endpoints. Inputs preserved; nonfinite inputs rejected without output mutation. Does not reconstruct pair basis/box tests or live scene navigation.')
(root/'artifacts/navigation-closest-verification.json').write_text(json.dumps(report,indent=2));print(report)
