"""Compare complete original 49cd30 world inertia update against PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
rng=random.Random(0x49cd30);commands=[];expected=[]
for case in range(1200):
 local=[rng.uniform(-20,20) for _ in range(9)];orientation=[rng.uniform(-2,2) for _ in range(9)]
 if case%4==0:orientation=[1,0,0,0,1,0,0,0,1]
 if case%4==1:orientation=[0,0,1,0,1,0,-1,0,0]
 if case%4==2:local=[2,0,0,0,4,0,0,0,8]
 raw=floats(*local,*orientation);seed=bytearray([0xa5]*0x170);seed[0x14:0x38]=raw[:36];seed[0x74:0x98]=raw[36:]
 u.mem_write(base,bytes(seed));u.mem_write(stack,pack(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x49cd30,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 result=bytes(u.mem_read(base+0x38,36));seed[0x38:0x5c]=result
 assert bytes(u.mem_read(base,len(seed)))==seed
 commands.append(raw);expected.append(result+pack(0))
original_cases=len(commands)
for index in range(18):
 for bad in (float('inf'),float('nan')):
  values=[1,0,0,0,1,0,0,0,1]*2;values[index]=bad
  commands.append(floats(*values));expected.append(bytes([0xa5])*36+pack(0xfffffffc))
commands.append(floats(*([3.4028234663852886e38]*18)));expected.append(bytes([0xa5])*36+pack(0xfffffffc))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--world'],input=b''.join(commands))
assert len(actual)==len(expected)*40
for i,want in enumerate(expected):assert actual[i*40:(i+1)*40]==want,('PC',i,actual[i*40:(i+1)*40].hex(),want.hex())
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_physics_tensor_world\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for i,(raw,want) in enumerate(zip(commands,expected)):
 for alias in (0,1,2):
  out=(base+0x1000,base,base+36)[alias]
  x.mem_write(base,raw);x.mem_write(base+0x1000,bytes([0xa5])*36);x.mem_write(stack,pack(stop,base,base+36,out))
  x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
  x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
  comparison=raw[(alias-1)*36:alias*36]+want[36:] if alias and i>=original_cases else want
  got=bytes(x.mem_read(out,36))+pack(x.reg_read(UC_X86_REG_EAX))
  assert got==comparison,('NXDK',i,alias,got.hex(),comparison.hex())
report=dict(result='PASS',original_cases=original_cases,guard_cases=len(commands)-original_cases,nxdk_alias_cases=2*len(commands),scope='Complete original 49cd30 and all transpose/matrix callees, no hooks. Every destination body byte checked; all tensor bytes match PC/NXDK for identity, quarter-turn, arbitrary matrices and diagonal/nonsymmetric local tensors. Port guards and both output aliases checked on NXDK. Not exhaustive floating-point equivalence or live body integration.')
(root/'artifacts/physics-tensor-world-verification.json').write_text(json.dumps(report,indent=2));print(report)
