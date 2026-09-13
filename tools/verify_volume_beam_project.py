"""Original515c00/555b80 beam projection against shared PC/NXDK."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_volume_beam_project\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
submitted=None
width=height=1
def hook(m,address,size,data):
    global submitted
    sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
    count,pointers,flags,mode=struct.unpack('<4I',m.mem_read(sp+4,16));assert flags==1 and mode==0x06110c42 and count<=12
    submitted=bytearray(struct.pack('<II',0,count))
    for i in range(count):
        vertex=struct.unpack('<I',m.mem_read(pointers+i*4,4))[0]
        submitted.extend(m.mem_read(vertex,24));submitted.extend(m.mem_read(vertex+28,8))
    submitted.extend(bytes((12-count)*32))
    m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
for address in (0x551900,):u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
rng=random.Random(5550)
commands=bytearray();results=bytearray();counts={}
for case in range(1024):
    width,height=((32,32),(64,32),(32,64),(127,63))[case%4]
    origin=tuple(rng.randint(-128,128)/8 for _ in range(3))
    basis=((1,0,0,0,1,0,0,0,1),(0,0,1,0,1,0,-1,0,0))[case%2]
    scale=(.125,.25,.0625);matrix=tuple(basis[i*3+j]*scale[i] for i in range(3) for j in range(3))
    position=tuple(origin[i]+rng.randint(-128,128)/8 for i in range(3))
    previous=tuple(v+(0 if case%7==0 else rng.randint(-32,32)/8) for v in position);radius=(case%19)/4
    perspective=(case//2)%2;clamp=(case//4)%2;enabled=(case//8)%2;far=(case//16)%2
    clip=struct.pack('<IIIf',enabled,perspective,far,1)
    projection=struct.pack('<I3f2i',clamp,(0,.01,-.01)[case%3],320,240,-16,7)
    camera=bytearray(328)
    struct.pack_into('<3f',camera,48,*scale)
    camera[232:]=struct.pack('<12ffI',*origin,*matrix,.98,perspective)+clip+projection
    command=bytes(camera)+struct.pack("<7f",*position,*previous,radius);commands.extend(command)
    u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x549270,stop,count=10000)
    u.mem_write(0x17c7bcc,struct.pack('<I',0x66));u.mem_write(0x17c7c18,bytes(4))
    u.mem_write(0x1818690,struct.pack('<3f',*origin));u.mem_write(0x18186c8,struct.pack('<9f',*matrix))
    u.mem_write(0x1818b48,struct.pack('<3f',*scale));u.mem_write(0x1818b7c,struct.pack('<f',.98))
    u.mem_write(0x5a4d18,bytes([enabled,perspective]));u.mem_write(0x1818b65,bytes([far]));u.mem_write(0x1818b6c,struct.pack('<f',1))
    u.mem_write(0x5a445a,projection[:1]);u.mem_write(0x1e652e8,projection[4:8]);u.mem_write(0x1818a5c,projection[8:12]);u.mem_write(0x1818a24,projection[12:16]);u.mem_write(0x17c7bec,projection[16:24])
    u.mem_write(base,struct.pack('<6f',*position,*previous))
    u.mem_write(stack,struct.pack('<3IfI',stop,base,base+12,radius,0x06110c42));u.reg_write(UC_X86_REG_ESP,stack)
    submitted=None;u.emu_start(0x515c00,stop,count=200000);assert u.reg_read(UC_X86_REG_EIP)==stop
    result=bytes(392) if submitted is None else bytes(submitted);results.extend(result)
    count=struct.unpack_from('<I',result,4)[0];counts[count]=counts.get(count,0)+1
    x.mem_write(base,command);x.mem_write(base+0x400,bytes(388))
    x.mem_write(stack,struct.pack('<4IfI',stop,base,base+328,base+340,radius,base+0x400));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=200000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x400,388))
    assert actual==result,(case,[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b][:20])
actual=subprocess.check_output([str(c['probe']),'--volume-beam-project'],input=commands)
assert actual==results,[(i//392,i%392,a,b) for i,(a,b) in enumerate(zip(actual,results)) if a!=b][:20]
report=dict(result='PASS',cases=1024,vertex_counts=counts,scope='Full515c00/555b80 with original math,518360 transform and5159a0 clipping/projection. Only final551900 intercepted. Exact1024 PC/NXDK beam screen polygons, degenerate endpoints and varied camera/clip settings. Native GPU and live scheduling excluded.')
(root/'artifacts/volume-beam-project.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
