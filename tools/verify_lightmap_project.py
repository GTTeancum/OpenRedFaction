"""Unhooked original4e49d0 projection compared with shared PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data);m.mem_map(0x30000000,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);binary=root/'build/xbox/main.exe';x=machine(binary)
b=0x30000000;point=b+0x1000;out=b+0x2000;stack=b+0xe000;stop=b+0xf000
entry=int(re.search(r'\s_rf_lightmap_project\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x4e49d0);inputs=[];outputs=[];clamped=0

def shared(wire,want):
 x.mem_write(b,wire[:24]);x.mem_write(point,wire[24:36]);x.mem_write(out,wire[24:32])
 alias=struct.unpack_from('<I',wire,36)[0];target=point if alias else out
 x.mem_write(stack,w(stop,b,point,target));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(target,8))
 assert actual==want,('NXDK',len(inputs),wire.hex(),actual.hex(),want.hex())
 inputs.append(wire);outputs.append(want)

for case in range(4096):
 axes=(case%3,(case//3)%3)
 if case<27:scale=(1,1);offset=(0,0);position=(-0.0,0.5,1)
 elif case==27:
  axes=(0,1);scale=(1-2**-23,1-2**-23);offset=(-1+2**-24,-1+2**-24);position=(1+2**-23,1+2**-23,0)
 elif case%5==0:
  scale=(.05,-.05);offset=(.5,.5);position=tuple(rng.uniform(-3,3) for _ in range(3))
 elif case%7==0:
  scale=tuple(rng.choice([1e30,-1e30,1e-30,-1e-30]) for _ in range(2));offset=(.5,-.5);position=tuple(rng.choice([1e30,-1e30,1e-30]) for _ in range(3))
 else:
  scale=tuple(rng.uniform(-2,2) for _ in range(2));offset=tuple(rng.uniform(-1,1) for _ in range(2));position=tuple(rng.uniform(-3,3) for _ in range(3))
 projection=w(*axes)+f(*scale,*offset);point_bytes=f(*position)
 u.mem_write(b+0x60,w(*axes));u.mem_write(b+0x4c,projection[8:24]);u.mem_write(point,point_bytes)
 u.mem_write(stack,w(stop,point,out,out+4));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,b);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4e49d0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 result=bytes(u.mem_read(out,8));values=struct.unpack('<2f',result);assert all(0<=v<=1 for v in values);clamped+=any(v in (0,1) for v in values)
 shared(projection+point_bytes+w(case&1),w(0)+result)
original_cases=len(inputs)
for mode in range(4):
 wire=bytearray(inputs[0])
 if mode==0:struct.pack_into('<I',wire,0,3)
 elif mode==1:struct.pack_into('<f',wire,8,math.nan)
 elif mode==2:struct.pack_into('<f',wire,16,math.inf)
 else:struct.pack_into('<f',wire,32,math.nan)
 shared(bytes(wire),w(-4)+wire[24:32])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--lightmap-project'],input=b''.join(inputs));assert actual==b''.join(outputs),'PC mismatch'
report=dict(result='PASS',original_cases=original_cases,port_guards=4,clamp_boundary_cases=clamped,alias_cases=original_cases//2,original_sha256=digest,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Unhooked4e49d0 with original axis getters, float stores, offset addition and clamp. Exact PC/NXDK UV bits including finite overflow and signed zero; shared input/output alias cases. Descriptor construction/loading, original aliased pointers, nonfinite original inputs and authored texture ownership are not covered.')
(root/'artifacts/lightmap-project.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
