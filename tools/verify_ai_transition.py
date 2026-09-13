"""Original407e20/407e80 AI transition setters versus PC and compiled NXDK."""
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
u=load(exe);x=load(root/'build/xbox/main.exe')
symbols=(root/'build/xbox/main.map').read_text()
entries=[int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16) for name in ('rf_entity_ai_set_action','rf_entity_ai_set_state')]
def call(cpu,address,args):
 cpu.mem_write(stack,w(stop,*args));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.emu_start(address,stop,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==stop;return cpu.reg_read(UC_X86_REG_EAX)
f=lambda v:struct.pack('<f',v)
offsets=(0x280,0x288,0x28c,0x290,0x2b4,0x2bc,0x530)
rng=random.Random(0x407e20);commands=[];expected=[];cases=0
for op in range(2):
 for case in range(1024):
  seed=rng.randbytes(0x600);compact=b''.join(seed[o:o+4] for o in offsets)
  clock=(-2147483648.0,2147483520.0,-1.9,-0.9,0.0,0.9,1.9,12345.75)[case%8] if case<64 else rng.uniform(-1000000,1000000)
  clockbits=struct.unpack('<I',f(clock))[0];requested=(-1,0,1,2,3,16,17,0x7fffffff)[case%8]
  a,b=rng.getrandbits(32),rng.getrandbits(32);na,nb=(0,1,0x100,0xff)[case%4],(0,1,0x100,0xff)[(case//4)%4]
  u.mem_write(base,seed);u.mem_write(0x6460f0,w(clockbits));u.mem_write(0x6fc4d8,bytes([na&255]));u.mem_write(0x64ecb9,bytes([nb&255]))
  call(u,0x407e80 if op else 0x407e20,[base,requested] if op else [base,requested,a,b])
  original=bytes(u.mem_read(base,0x600));want=b''.join(original[o:o+4] for o in offsets)
  restored=bytearray(original)
  for o in offsets:restored[o:o+4]=seed[o:o+4]
  assert restored==seed,'original touched unrelated fields'
  x.mem_write(base,b'\xa5'*32+compact+b'\x5a'*32)
  args=[base+32,requested,clockbits] if op else [base+32,requested,a,b,clockbits,na,nb]
  assert call(x,entries[op],args)==0
  assert bytes(x.mem_read(base,92))==b'\xa5'*32+want+b'\x5a'*32,(op,case)
  commands.append(compact+w(op,requested,a,b,clockbits,na,nb));expected.append(w(0)+want);cases+=1
for op in range(2):
 for clock in (float('nan'),float('inf'),-float('inf'),2147483648.0,-2147483904.0):
  seed=rng.randbytes(28);bits=struct.unpack('<I',f(clock))[0];x.mem_write(base,seed)
  args=[base,2,bits] if op else [base,2,3,4,bits,1,1]
  assert call(x,entries[op],args)==0xfffffffc and bytes(x.mem_read(base,28))==seed
  commands.append(seed+w(op,2,3,4,bits,1,1));expected.append(w(-4)+seed)
 assert call(x,entries[op],[0,2,0] if op else [0,2,3,4,0,1,1])==0xfffffffc
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-transition'],input=b''.join(commands))
assert actual==b''.join(expected),'PC mismatch'
report=dict(result='PASS',original_pc_nxdk_cases=cases,invalid_clock_cases=10,null_state_cases=2,scope='Full unhooked original407e20/407e80 and actual __ftol, exact touched fields plus complete original footprint. PC and linked NXDK setters; low-byte network flags, action2 remap, finite signed32 clock truncation and preservation guards. No AI scheduling or native XEMU claim.')
(root/'artifacts/ai-transition-verification.json').write_text(json.dumps(report,indent=2));print(report)
