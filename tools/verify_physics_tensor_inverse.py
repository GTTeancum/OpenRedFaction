"""Execute complete original 4fccf0 and compare shared inverse tensor bytes."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
rng=random.Random(0x4fccf0)
matrices=[floats(*([0]*9)),floats(1,0,0,0,1,0,0,0,1),floats(1,2,3,2,4,6,0,0,0)]
for i in range(1000):
 m=[rng.uniform(-20,20) for _ in range(9)]
 if i%5==0:m=[rng.randrange(-8,9) for _ in range(9)];m[6:9]=m[:3]
 if i%5==1:m=[rng.uniform(-1,1)*2**rng.randrange(-15,16) for _ in range(9)]
 matrices.append(floats(*m))
for delta in (2**-23,-2**-24,2**-20,-2**-20):matrices.append(floats(1,1,1,1,1+delta,1,1,1,1+delta))
expected=[];unchanged=0
for i,raw in enumerate(matrices):
 u.mem_write(base,raw);u.mem_write(stack,pack(stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4fccf0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 result=bytes(u.mem_read(base,36));expected.append(result+pack(0));unchanged+=result==raw
original_cases=len(matrices)
for i in range(9):
 for bad in (float('inf'),float('nan')):
  m=[1,0,0,0,1,0,0,0,1];m[i]=bad;matrices.append(floats(*m));expected.append(bytes([0xa5])*36+pack(0xfffffffc))
for scale in (2**-60,2**60,2**-140):
 matrices.append(floats(scale,0,0,0,scale,0,0,0,scale));expected.append(bytes([0xa5])*36+pack(0xfffffffc))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--inverse'],input=b''.join(matrices))
assert len(actual)==len(expected)*40
for i,want in enumerate(expected):assert actual[i*40:(i+1)*40]==want,('PC',i,matrices[i].hex(),actual[i*40:(i+1)*40].hex(),want.hex())
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_physics_tensor_inverse\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for i,(raw,want) in enumerate(zip(matrices,expected)):
 for alias in (False,True):
  out=base if alias else base+0x1000
  x.mem_write(base,raw);x.mem_write(base+0x1000,bytes([0xa5])*36);x.mem_write(stack,pack(stop,base,out))
  x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
  x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
  assert x.reg_read(UC_X86_REG_FPCW)==0x37f
  comparison=raw+want[36:] if alias and i>=original_cases else want
  got=bytes(x.mem_read(out,36))+pack(x.reg_read(UC_X86_REG_EAX))
  assert got==comparison,('NXDK',i,alias,got.hex(),comparison.hex())
report=dict(result='PASS',original_cases=original_cases,unchanged_matrices=unchanged,guard_cases=len(matrices)-original_cases,nxdk_alias_cases=len(matrices),scope='Complete original 4fccf0 and all callees, no hooks. Exact PC/NXDK matrix bytes, integer singular and near-singular fixtures, mixed scales and random nonsymmetric matrices. NXDK output alias and control-word preservation checked; invalid input and unrepresentable determinants are port guards. No exhaustive numeric proof or live Xbox body integration.')
(root/'artifacts/physics-tensor-inverse-verification.json').write_text(json.dumps(report,indent=2));print(report)
