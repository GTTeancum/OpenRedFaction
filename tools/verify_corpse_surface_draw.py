"""Full original corpse draw through GPU vertex encoding versus shared PC/NXDK."""
import hashlib,json,random,re,runpy,struct,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'\s_rf_corpse_surface_prepare_draw\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
submitted=0

def hook(m,address,size,data):
 global submitted
 sp=m.reg_read(UC_X86_REG_ESP)
 if address==0x551900:
  submitted=struct.unpack('<I',m.mem_read(sp+4,4))[0];assert submitted<=12;return
 ret=struct.unpack('<I',m.mem_read(sp,4))[0];m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
for address in (0x551900,0x550850,0x559e80,0x559d90):u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
def invalid(m,access,address,size,value,data):
 print('invalid',hex(m.reg_read(UC_X86_REG_EIP)),hex(address));return False
u.hook_add(UC_HOOK_MEM_INVALID,invalid)
rng=random.Random(42);commands=[];outputs=[];counts={}
for case in range(1024):
 origin=tuple(rng.randint(-128,128)/8 for _ in range(3));scale=(.125,.25,.0625)
 basis=((1,0,0,0,1,0,0,0,1),(0,0,1,0,1,0,-1,0,0))[case%2];matrix=tuple(basis[i*3+j]*scale[i] for i in range(3) for j in range(3))
 position=tuple(origin[i]+rng.randint(-128,128)/8 for i in range(3))
 if case%8==6:position=tuple(origin[i]+(7.75,0,16)[i] for i in range(3))
 growth=(5,8)[case%2];elapsed=(0.,growth*.5,growth-.00001,growth,growth+1)[case%5];maximum=(.25,.5)[case%2]
 rate=struct.unpack('<f',f(1.5707963705062866/growth))[0]
 effect=f(elapsed,-999,growth,maximum,rate,*position,1,0,0,0,1,.25,0,0,1)+w(1,rng.getrandbits(32),0,0)
 perspective=(case//2)%2;clamp=(case//4)%2;enabled=(case//8)%2;far=(case//16)%2
 clip=w(enabled,perspective,far)+f(1);projection=w(clamp)+f(0,320,240)+struct.pack('<2i',-16,7)
 camera=bytearray(328);camera[48:60]=f(*scale);camera[232:]=f(*origin,*matrix,.98)+w(perspective)+clip+projection
 environment=w(0xdeadbeef,1,1,0)+f(scale[2],scale[2],1,1,0,0,0,0)
 command=effect+bytes(camera)+environment;commands.append(command)
 u.mem_write(stack,w(stop));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x549270,stop,count=10000)
 u.mem_write(0x17c7bcc,w(0x66));u.mem_write(0x17c7c18,w(0));u.mem_write(0x1818690,f(*origin));u.mem_write(0x18186c8,f(*matrix));u.mem_write(0x1818b48,f(*scale));u.mem_write(0x1818b7c,f(.98))
 u.mem_write(0x5a4d18,bytes([enabled,perspective]));u.mem_write(0x1818b65,bytes([far]));u.mem_write(0x1818b6c,f(1));u.mem_write(0x5a445a,projection[:1]);u.mem_write(0x1e652e8,projection[4:8]);u.mem_write(0x1818a5c,projection[8:12]);u.mem_write(0x1818a24,projection[12:16]);u.mem_write(0x17c7bec,projection[16:24])
 for address,value in [(0x62f73c,123),(0x17c7c58,0x118c42),(0x1cfcbd8,base+0x3000),(0x1e652f0,0),(0x1818348,0),(0x181834c,0),(0x1e652f4,0)]:u.mem_write(address,w(value))
 u.mem_write(0x5aa7e4,bytes([1,1]));u.mem_write(0x17c7c34,b'\0');u.mem_write(0x17c7c30,f(0));u.mem_write(0x5a7dd8,f(scale[2],scale[2]));u.mem_write(0x1d86314,f(1));u.mem_write(0x1d4f2d4,f(1));u.mem_write(0x1e652ed,b'\1')
 u.mem_write(base,effect);u.mem_write(base+0x3000,bytes(12*40));u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);submitted=0;u.emu_start(0x42df20,stop,count=200000);assert u.reg_read(UC_X86_REG_EIP)==stop
 gpu=b''.join(bytes(u.mem_read(base+0x3000+i*40,32)) for i in range(submitted))+bytes((12-submitted)*32)
 expected=bytes(u.mem_read(base,84))+w(0,submitted)+gpu;outputs.append(expected);counts[submitted]=counts.get(submitted,0)+1
 x.mem_write(base,command);x.mem_write(base+0x400,bytes(392));x.mem_write(stack,w(stop,base,base+84,base+412,base+0x408,base+0x404));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=200000);assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=bytes(x.mem_read(base,84))+w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x404,388))
 assert actual==expected,('NXDK',case,[(i,a,z) for i,(a,z) in enumerate(zip(actual,expected)) if a!=z][:20])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-surface-draw'],input=b''.join(commands))
assert actual==b''.join(outputs),'PC differs'
assert counts.get(0,0) and counts.get(4,0) and any(k not in (0,4) for k in counts)
report=dict(result='PASS',cases=len(commands),vertex_counts=counts,scope='Full original42df20/517110/558d40/551900 through final GPU vertex writes vs shared PC/NXDK composed draw preparation. Only550850 state binding,559e80 index submission and559d90 GPU batch flush supplied;551900 observed, not replaced. Resolved ordinary color/alpha and camera/depth/UV environment. Exact effect state and32-byte GPU vertices. Texture upload, actual rasterization, room scheduling and live effects excluded.')
(root/'artifacts/corpse-surface-draw.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
