"""Whole original 4a0d70 matrix execution against shared PC/NXDK code."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));raw=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(raw)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,raw);u.mem_map(base,0x10000);u.reg_write(UC_X86_REG_FPCW,0x37f);return u
exe=ROOT/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);rng=random.Random(0x4a0d70);inputs=[];expected=[]
pitches=[0,-0.,1.5707963705062866,-1.5707963705062866,.7853981852531433,-.7853981852531433]
yaws=[0,-0.,6.2831854820251465,-6.2831854820251465,3.1415927410125732,-3.1415927410125732]
angles=[(p,y,rng.uniform(-4,4)) for p in pitches for y in yaws]+[(rng.uniform(-1.57079,1.57079),rng.uniform(-6.28318,6.28318),rng.uniform(-4,4)) for _ in range(1164)]
for a in angles:
 payload=f(*a);inputs.append(payload);u.mem_write(base,payload);u.mem_write(stack,w(stop,base+0x100,base));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4a0d70,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 expected.append(w(0)+bytes(u.mem_read(base+0x100,36)))
actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_eye_probe.exe'),'--look-matrix'],input=b''.join(inputs));assert len(actual)==len(expected)*40
exact=0;maximum=0.0
for i,e in enumerate(expected):
 got=actual[i*40:(i+1)*40];assert got[:4]==e[:4]
 error=max(abs(a-b) for a,b in zip(struct.unpack('<9f',got[4:]),struct.unpack('<9f',e[4:])))
 maximum=max(maximum,error);exact+=int(got==e)
 assert error<=1e-16,('PC mismatch',i,angles[i],error)

report=dict(result='PASS',original_sha256=digest,original_fpu_control='0x037f',pc_cases=len(inputs),pc_bit_exact=exact,max_absolute_error=maximum,scope='Whole unmodified 4a0d70 including cross products, normalization and 4fc500. Wrapped pitch/yaw domain only; no live XEMU or input integration claim.')
if '--nxdk' in sys.argv:
 x=machine(ROOT/'build/xbox/main.exe');mapping=(ROOT/'build/xbox/main.map').read_text();entry=int(re.search('_rf_look_orientation'+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
 nxdk_exact=0;nxdk_maximum=0.0
 for i,payload in enumerate(inputs):
  x.mem_write(base,payload);x.mem_write(stack,w(stop,base,base+0x100));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
  got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x100,36));e=expected[i];assert got[:4]==e[:4]
  error=max(abs(a-b) for a,b in zip(struct.unpack('<9f',got[4:]),struct.unpack('<9f',e[4:])))
  nxdk_maximum=max(nxdk_maximum,error);nxdk_exact+=int(got==e)
  assert error<=1e-16,('NXDK mismatch',i,error)
 report.update(nxdk_cases=len(inputs),nxdk_bit_exact=nxdk_exact,nxdk_max_absolute_error=nxdk_maximum)
(ROOT/'artifacts/look-orientation-verification.json').write_text(json.dumps(report,indent=2));print(report)
