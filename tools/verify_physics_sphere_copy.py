"""Original authored sphere transfer, growth and radius with heap supplied."""
import hashlib,json,struct,sys,subprocess
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b);u.mem_map(0,4096)
base=0x30000000;u.mem_map(base,0x20000);params=base+0x2000;source=base+0x3000;stack=base+0x1e000;stop=base+0x1f000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
allocations=[];freed=[];cases=[]
commands=bytearray(pack(0));expected=bytearray()
def hook(cpu,address,size,data):
 if address not in (0x573619,0x57360e):return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=read(sp+4)
 if address==0x573619:
  assert arg in (384,768);pointer=base+0x5000+len(allocations)*0x1000
  allocations.append((pointer,arg));cpu.mem_write(pointer,bytes([0xa5])*arg);cpu.reg_write(UC_X86_REG_EAX,pointer)
 else:assert arg in [p for p,n in allocations];freed.append(arg)
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp))
u.hook_add(UC_HOOK_CODE,hook);matrix=floats(1,0,0,0,1,0,0,0,1)
for count in (1,2,16,17,32):
 for positive_parameter in (False,True):
  spheres=b''.join(floats((i%3)*.5,0,0,.5+(i%2)*.5,.25 if positive_parameter and i==count-1 else -1)+pack(0x12340000+i) for i in range(count))
  p=bytearray(0xa0);p[0x14:0x18]=floats(8);p[0x18:0x3c]=matrix;p[0x48:0x6c]=matrix;p[0x84:0x88]=floats(99)
  p[0x88:0x94]=pack(count,count,source);p[0x94:0x98]=pack(0x70)
  u.mem_write(params,bytes(p));u.mem_write(source,spheres);u.mem_write(base,bytes([0xa5])*0x170);u.mem_write(base+0xfc,bytes(12));u.mem_write(0,pack(0))
  allocations.clear();freed.clear();u.mem_write(stack,pack(stop,base,params));u.reg_write(UC_X86_REG_ESP,stack)
  u.emu_start(0x49ec90,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop and read(0)==0
  capacity=16 if count<=16 else 32;pointer=read(base+0x104)
  assert bytes(u.mem_read(base+0xfc,12))==pack(count,capacity,pointer)
  assert bytes(u.mem_read(pointer,count*24))==spheres and bytes(u.mem_read(source,count*24))==spheres
  assert [n for a,n in allocations]==([384] if count<=16 else [384,768])
  assert freed==([] if count<=16 else [allocations[0][0]])
  flags=0x70|(0x2000 if positive_parameter else 0);assert read(base+0x120)==flags and read(params+0x94)==flags
  radius=max((i%3)*.5+.5+(i%2)*.5 for i in range(count))
  assert bytes(u.mem_read(base+0xf8,4))==floats(radius),(count,positive_parameter,bytes(u.mem_read(base+0xf8,4)).hex(),radius)
  cases.append(dict(count=count,parameter_positive=positive_parameter,capacity=capacity,radius=radius))
  commands.extend(pack(count)+spheres);expected.extend(bytes(u.mem_read(pointer,count*24)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--spheres'],input=commands)
assert actual==expected,'Shared owned sphere bytes differ'
report=dict(result='PASS',cases=cases,scope='Complete original physics preparation with positive mass and existing authored sphere lists. Only heap allocate/free intercepted. Ordered 24-byte copies, 16-to-32 growth/free, parameter-10 flag propagation and axis-aligned bounding radius checked. Generated inertia, allocation failure and general-center rounding excluded.')
report['pc_owned_cases']=len(cases)+1
(root/'artifacts/physics-sphere-copy-verification.json').write_text(json.dumps(report,indent=2));print(report)
