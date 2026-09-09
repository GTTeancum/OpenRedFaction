"""Compare shared sphere mass/tensor accumulation to original 49ec90..49edf0."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
f32=lambda v:struct.unpack('<f',floats(v))[0]
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;params=base+0x2000;source=base+0x3000;stack=base+0xe000;stop=base+0xf000
u.mem_map(base,0x10000)
rng=random.Random(0x49ec90);commands=[];expected=[];cases=0
prepare='--prepare' in sys.argv
for case in range(640):
 count=(1,2,3,16,17,32)[case%6];material=(0,1,9,10,0xffffffff)[case%5]
 density=f32((0,.25,1,2.75)[case%4] if case<128 else rng.uniform(.001,25))
 initial=floats((-1,0,-8)[case%3],*([0]*9 if case%3==0 else [rng.uniform(-2,2) for _ in range(9)]))
 records=[]
 for i in range(count):
  center=[0,0,0] if case<16 else [rng.uniform(-20,20) for _ in range(3)]
  radius=0 if case<8 else rng.uniform(.001,5)
  records.append(floats(*center,radius,-1)+pack(0x12340000+i))
 spheres=b''.join(records)
 for index in range(10):u.mem_write(0x649f58+28*index,floats(density if index==(material if 0<material<10 else 0) else 99))
 p=bytearray(0xa0);p[0x10:0x14]=pack(material);p[0x14:0x3c]=initial
 p[0x88:0x94]=pack(count,count,source);p[0x94:0x98]=pack((0x10,0x20,0x40,0x70)[case%4])
 u.mem_write(params,bytes(p));u.mem_write(source,spheres);u.mem_write(stack,pack(stop,base,params))
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x49ec90,0x49edf0,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x49edf0
 if prepare:
  u.emu_start(0x49edf0,0x49edf5,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x49edf5
 result=bytes(u.mem_read(params+0x14,40));p[0x14:0x3c]=result
 assert bytes(u.mem_read(params,len(p)))==p and bytes(u.mem_read(source,len(spheres)))==spheres
 commands.append(pack(count)+floats(density)+initial+spheres);expected.append(result+pack(0));cases+=1

# Port-only guards must leave the caller's output untouched.
initial=floats(0,*([0]*9));sphere=floats(1,2,3,1,-1)+pack(0)
guards=[(0,1,initial,b''),(1,-1,initial,sphere),(1,float('inf'),initial,sphere),(1,float('nan'),initial,sphere)]
for offset in (0,4,8,12):
 for bad in (float('inf'),float('nan')):
  altered=bytearray(sphere);altered[offset:offset+4]=floats(bad);guards.append((1,1,initial,bytes(altered)))
altered=bytearray(sphere);altered[12:16]=floats(-1);guards.append((1,1,initial,bytes(altered)))
for offset in range(0,40,4):
 altered=bytearray(initial);altered[offset:offset+4]=floats(float('nan'));guards.append((1,1,bytes(altered),sphere))
guards.append((1,3.4028234663852886e38,initial,floats(1,2,3,2,-1)+pack(0)))
guards.append((1,1,initial,floats(3.4028234663852886e38,2,3,1,-1)+pack(0)))
for count,density,seed,records in guards:
 commands.append(pack(count)+floats(density)+seed+records);expected.append(bytes([0xa5])*40+pack(0xfffffffc))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--prepare' if prepare else '--accumulate'],input=b''.join(commands))
assert len(actual)==len(expected)*44
for i,want in enumerate(expected):assert actual[i*44:(i+1)*44]==want,('PC',i,actual[i*44:(i+1)*44].hex(),want.hex())

xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_physics_spheres_'+('prepare' if prepare else 'accumulate')+r'\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for i,(command,want) in enumerate(zip(commands,expected)):
 count=struct.unpack_from('<I',command)[0]
 x.mem_write(params,command[8:48]);x.mem_write(source,command[48:] or bytes(24));x.mem_write(base,bytes([0xa5])*40)
 x.mem_write(stack,pack(stop,source,count)+command[4:8]+pack(params,base))
 x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=bytes(x.mem_read(base,40))+pack(x.reg_read(UC_X86_REG_EAX))
 assert got==want,('NXDK',i,got.hex(),want.hex())
report=dict(result='PASS',original_cases=cases,pc_cases=cases,nxdk_cases=cases,guard_cases=len(guards),scope='Original 49ec90 entry through existing-sphere accumulation, stopping before 4fccf0 inversion; no intercepted calls. All mass/tensor bytes compared against PC and compiled NXDK. Nonpositive initial mass, nonsymmetric initial tensors, seeded material lookup, 1..32 spheres, zero and random centers/radii. Port guards preserve output. Not matrix inversion, full physics initialization, exhaustive float equivalence or live Xbox runtime.')
if prepare:report['scope']='Original 49ec90 entry through existing-sphere mass/tensor generation and complete 4fccf0 inversion. No intercepted calls. PC/NXDK exact output comparisons and port guards; not full body initialization or live Xbox integration.'
(root/('artifacts/physics-sphere-prepare-verification.json' if prepare else 'artifacts/physics-sphere-mass-verification.json')).write_text(json.dumps(report,indent=2));print(report)
