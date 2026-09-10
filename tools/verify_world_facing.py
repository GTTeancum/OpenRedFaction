"""Compare static-solid facing with the original cached-face branch."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
BASE=0x30000000;STACK=BASE+0x8000;STOP=BASE+0xf000
def machine(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
 u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(b)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,b)
 u.mem_map(BASE,65536);u.reg_write(UC_X86_REG_FPCW,0x37f);return u
exe=ROOT/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe)
def stop(uc,a,n,c):
 if a in (0x55f841,0x55f846):uc.emu_stop()
u.hook_add(UC_HOOK_CODE,stop)
rng=random.Random(0x55f82f);inputs=[];expected=[]
for i in range(2500):
 v=[rng.randint(-4096,4096)/16 for _ in range(7)]
 if i%5==0:v[3]=-(v[0]*v[4]+v[1]*v[5]+v[2]*v[6])
 if i%17==0:v[i%7]=float('nan')
 if i%23==0:v[i%7]=float('inf')
 data=struct.pack('<7f',*v);inputs.append(data)
 u.mem_write(BASE+8,data[:16]);u.mem_write(STACK+0x98,data[16:])
 u.reg_write(UC_X86_REG_ESI,BASE);u.reg_write(UC_X86_REG_ESP,STACK)
 u.emu_start(0x55f824,0,count=10000);end=u.reg_read(UC_X86_REG_EIP);assert end in (0x55f841,0x55f846)
 expected.append(struct.pack('<I',end==0x55f846))
probe=ROOT/'build/pc/Release/rf_model_probe.exe'
actual=subprocess.check_output([str(probe),'--world-facing'],input=b''.join(inputs));assert actual==b''.join(expected)
nxdk=False
if '--nxdk' in sys.argv:
 x=machine(ROOT/'build/xbox/main.exe');mapping=(ROOT/'build/xbox/main.map').read_text()
 address=int(re.search(r'_rf_preview_plane_visible\s+([0-9a-fA-F]+)',mapping)[1],16)
 for i,data in enumerate(inputs):
  x.mem_write(BASE,data);x.mem_write(STACK,struct.pack('<3I',STOP,BASE,BASE+16))
  x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f)
  x.emu_start(address,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP
  assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))==expected[i],i
  assert x.reg_read(UC_X86_REG_FPCW)==0x27f
 nxdk=True
report=dict(result='PASS',cases=len(inputs),original_sha256=sha,pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk=nxdk,
 scope='Original 55f824 facing branch with 4163a0/40a0b0 unmodified; supplied cached plane/viewer, zero boundary and nonfinite cases. No original cache construction or mover transform claim.')
(ROOT/'artifacts/world-facing-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
