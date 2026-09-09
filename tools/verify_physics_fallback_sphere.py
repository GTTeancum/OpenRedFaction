"""Original fallback sphere preparation with only heap allocation supplied."""
import hashlib,json,struct,sys,subprocess,re,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b);u.mem_map(0,4096)
base=0x30000000;params=base+0x2000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
allocations=[]
commands=bytearray();expected_shared=bytearray();rng=random.Random(49)
radii=[.5,1.0,2.0]+[struct.unpack('<f',floats(rng.uniform(.001,100)))[0] for _ in range(64)]
def hook(cpu,address,size,data):
 if address!=0x573619:return
 sp=cpu.reg_read(UC_X86_REG_ESP);size=read(sp+4);assert size==16*24 and len(allocations)<2
 pointer=base+0x4000+len(allocations)*0x1000;allocations.append(pointer);cpu.mem_write(pointer,bytes([0xa5])*size)
 cpu.reg_write(UC_X86_REG_EAX,pointer);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp))
u.hook_add(UC_HOOK_CODE,hook);matrix=floats(1,0,0,0,1,0,0,0,1);cases=0
for material in (0,1,9,10,0xffffffff):
 for mode in (0x10,0x20,0x40,0x70):
  for mass in (-1.0,0.0,8.0):
   for radius in radii:
    for index in range(10):u.mem_write(0x649f50+28*index,floats(.25,.5,float(index+1)))
    p=bytearray(0xa0);p[0x10:0x14]=pack(material);p[0x14:0x18]=floats(mass);p[0x18:0x3c]=matrix;p[0x48:0x6c]=matrix
    p[0x84:0x88]=floats(radius);p[0x94:0x98]=pack(mode)
    u.mem_write(params,bytes(p));u.mem_write(base,bytes([0xa5])*0x170);u.mem_write(base+0xfc,bytes(12));u.mem_write(0,pack(0))
    allocations.clear();u.mem_write(stack,pack(stop,base,params));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x49ec90,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop and read(0)==0
    density=float(material+1) if 0<material<10 else 1.0;expected_mass=mass if mass>0 else density*radius*radius
    assert bytes(u.mem_read(params+0x14,4))==floats(expected_mass) and bytes(u.mem_read(base+0x10,4))==floats(expected_mass)
    assert len(allocations)==2
    assert bytes(u.mem_read(params+0x88,12))==pack(1,16,allocations[0])
    assert bytes(u.mem_read(base+0xfc,12))==pack(1,16,allocations[1])
    # The sixth word comes from unspecified local storage; don't invent it.
    sphere=floats(0,0,0,radius,-1)
    for pointer in allocations:assert bytes(u.mem_read(pointer,20))==sphere
    assert bytes(u.mem_read(allocations[0],24))==bytes(u.mem_read(allocations[1],24))
    assert bytes(u.mem_read(base+0xf8,4))==floats(radius)
    assert read(params+0x94)==mode and read(base+0x120)==mode
    commands.extend(floats(density,radius,mass))
    expected_shared.extend(bytes(u.mem_read(params+0x14,4))+bytes(u.mem_read(allocations[0],20))+pack(0))
    cases+=1
guards=[]
for index in range(3):
 for bad in (float('inf'),float('nan')):
  values=[1.0,1.0,0.0];values[index]=bad;guards.append(values)
guards.extend([[-1,1,0],[1,-1,0],[3.4028234663852886e38,2,0]])
for values in guards:
 commands.extend(floats(*values));expected_shared.extend(bytes([0xa5])*24+pack(0xfffffffc))
raw=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe')],input=commands)
assert raw==expected_shared,('PC fallback mismatch',next((i for i,(a,b) in enumerate(zip(raw,expected_shared)) if a!=b),None))
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_physics_fallback_prepare\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for at in range(0,len(commands),12):
 x.mem_write(base,bytes([0xa5])*24);x.mem_write(stack,pack(stop)+commands[at:at+12]+pack(base));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert bytes(x.mem_read(base,24))+pack(x.reg_read(UC_X86_REG_EAX))==expected_shared[at//12*28:(at//12+1)*28],('NXDK fallback mismatch',at//12)
report=dict(result='PASS',cases=cases,allocation_bytes_per_case=768,scope='Complete original 49ec90 and callees for no-model fallback sphere; only heap allocation intercepted. Real capacity growth, element copy and destination radius setup execute. Seeded material coefficients, mass generation/preservation, four sphere modes and three radii; no allocator failure, existing spheres, geometric-model inertia or shared physics implementation. Sixth sphere word copied but not assigned semantics/default.')
report.update(pc_cases=cases,nxdk_cases=cases,guard_cases=len(guards),scope='Original fallback sphere path with heap supplied; shared C mass and five defined sphere fields match on PC and compiled NXDK across 67 radii. Nine port-only invalid-input guards also preserve output on both builds. Sphere ownership, inertia inversion and runtime integration excluded; sixth original sphere word intentionally omitted.')
(root/'artifacts/physics-fallback-sphere-verification.json').write_text(json.dumps(report,indent=2));print(report)
