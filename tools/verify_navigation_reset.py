"""Full original40a950 navigation reset versus shared PC and compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
base=0x30000000;stack=base+0xe000;stop=base+0xf000;route=base+0x100
w=lambda *v:struct.pack('<%dI'%len(v),*(v&0xffffffff for v in v))
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
 u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(b)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,b);u.mem_map(base,0x10000);return u
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_navigation_reset\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def call(cpu,address,args):
 cpu.mem_write(stack,w(stop,*args));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.emu_start(address,stop,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==stop;return cpu.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x40a950);commands=[];expected=[]
for case in range(1024):
 seed=rng.randbytes(0x400);clock=(0,1,0x3ff1a100,0x3ff1a0ff)[case] if case<4 else rng.randrange(0x3ff1a101)
 u.mem_write(base,seed);u.mem_write(0x5a3ed8,w(clock));call(u,0x40a950,[route]);want=bytes(u.mem_read(base,0x400))
 assert want[:0x100]==seed[:0x100] and want[0x274:]==seed[0x274:]
 x.mem_write(base,seed);assert call(x,entry,[route,clock])==0
 assert bytes(x.mem_read(base,0x400))==want,case
 commands.append(seed[0x100:0x274]+w(clock));expected.append(w(0)+want[0x100:0x274])
 # Repeated reset with unchanged clock must preserve the complete result.
 assert call(x,entry,[route,clock])==0 and bytes(x.mem_read(base,0x400))==want
for clock in (-1,0x3ff1a101,0x7fffffff,-0x80000000):
 seed=rng.randbytes(0x400);x.mem_write(base,seed);result=call(x,entry,[route,clock]);assert result!=0 and bytes(x.mem_read(base,0x400))==seed
 commands.append(seed[0x100:0x274]+w(clock));expected.append(w(result)+seed[0x100:0x274])
assert call(x,entry,[0,0])!=0
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--navigation-reset'],input=b''.join(commands))
assert actual==b''.join(expected),'PC mismatch'
report=dict(result='PASS',original_pc_nxdk_cases=1024,repeated_nxdk_resets=1024,invalid_clock_cases=4,null_state_cases=1,scope='Full original40a950 and unchanged4fa360/4fad00 callees, no hooks. Exact372-byte prefix and surrounding bytes vs PC/compiled NXDK, randomized retained fields, game-clock endpoints, repeated calls and guard preservation. No scene ownership, live XEMU execution, navigation selection or AI dispatch.')
(root/'artifacts/navigation-reset-verification.json').write_text(json.dumps(report,indent=2));print(report)
