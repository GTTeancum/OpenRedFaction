"""Compare resident static batches to complete original vertex loops."""
import hashlib,json,re,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;out=base+256;material=base+1024;batch=base+2048;obj=base+4096;raw=base+8192;links=base+9000;reuse=base+10000
def put(address,value):u.mem_write(address,struct.pack('<I',value))
rng=random.Random(0x52de10);cases=[];expected=[];duplicates=0
for n in range(512):
    vertices=[];distances=[]
    for i in range(8):
        distance=rng.randrange(1,i+1) if i and rng.randrange(3)==0 else -rng.randrange(2)
        duplicates+=distance>0;distances.append(distance)
        values=[rng.randint(-64,64)/16 if n<256 else rng.uniform(-4,4) for _ in range(8)]
        if n%29==0:values[3:6]=[0,0,0]
        if n%31==0:values[3]=float('nan')
        if n%37==0:values[4]=float('inf')
        v=struct.pack('<8f',*values)+bytes(rng.randrange(256) for _ in range(8))
        vertices.append(v)
        u.mem_write(raw+i*12,v[:12]);u.mem_write(links+i*12,v[12:24]);u.mem_write(obj+i*8,v[24:32]);u.mem_write(reuse+i*2,struct.pack('<h',distance))
    camera=[0,0,-10];rotation=[1,0,0,0,1,0,0,0,1];screen=[320,-240,320,240];bounds=[0,0,640,480]
    if n>=256:rotation=[rng.uniform(-1,1) for _ in range(9)]
    flags=[(n>>i)&1 for i in range(5)]
    view=struct.pack('<23f5I',*camera,*rotation,1,2,*screen,*bounds,20,*flags)
    lights=struct.pack('<21f',*(rng.randint(-16,16)/16 for _ in range(18)),30,40,50)
    output=struct.pack('<I3sB2f',(n>>5)&1,b'\x40\x80\xc0',180,1,1)
    use_colors=(n>>6)&1;colors=bytes(rng.randrange(256) for _ in range(24));color_address=base+11000 if use_colors else 0
    cases.append(b''.join(vertices)+struct.pack('<8i',*distances)+view+lights+output+struct.pack('<I',use_colors)+colors)
    for address,size in [(0x1bdb2b0,96),(0x1c0e700,96),(0x1c3d554,8),(0x1bf29b0,8),(0x1c3f494,24),(out,320)]:u.mem_write(address,b'\xa5'*size)
    u.mem_write(base+11000,colors);u.mem_write(material,bytes(64));u.mem_write(material+8,b'\xb4');put(material+28,color_address)
    u.mem_write(batch,bytes(56));put(batch+4,raw);put(batch+8,links);put(batch+12,obj);put(batch+24,reuse);u.mem_write(batch+40,struct.pack('<H',8))
    u.mem_write(0x1818690,view[:12]);u.mem_write(0x18186c8,view[12:48]);u.mem_write(0x1818b7c,view[48:52]);u.mem_write(0x17c7c30,view[52:56]);u.mem_write(0x1cfcb3c,view[72:88]);u.mem_write(0x1818b6c,view[88:92])
    u.mem_write(0x1c3d500,lights);u.mem_write(0x5a7dd8,output[8:12]);u.mem_write(0x5a7ddc,output[12:])
    for address,value in [(0x5a4d19,flags[0]),(0x5a4d18,flags[2]),(0x1818b65,flags[3])]:u.mem_write(address,bytes([value]))
    u.mem_write(esp,bytes(1024));u.mem_write(esp+0x11,bytes([flags[1],(n>>5)&1,flags[4]]))
    for offset,value in [(0x14,0x1c3f494),(0x18,batch),(0x1c,obj),(0x28,reuse),(0x2c,color_address),(0x34,0),(0x38,raw),(0x44,(color_address-0x1c3f494)&0xffffffff),(0x350,material)]:put(esp+offset,value)
    for offset,value in [(0x78,screen[0]),(0x84,screen[1]),(0x80,screen[2]),(0x7c,screen[3])]:u.mem_write(esp+offset,struct.pack('<f',value))
    for register,value in [(UC_X86_REG_ESP,esp),(UC_X86_REG_ESI,out+18),(UC_X86_REG_EBX,0),(UC_X86_REG_EDI,0x1bdb2b4)]:u.reg_write(register,value)
    u.emu_start(0x52e11b,0x52e438,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x52e438
    cache=b''.join(b'\xa5'*12+bytes(u.mem_read(0x1bdb2b0+i*12,12))+bytes(u.mem_read(0x1c3d554+i,1))+bytes(u.mem_read(0x1bf29b0+i,1))+bytes(u.mem_read(0x1c3f494+i*3,3))+b'\xa5'*3 for i in range(8))
    expected.append(bytes(4)+cache+bytes(u.mem_read(0x1c0e700,96))+b'\xa5'*96+bytes(u.mem_read(out,320)))
probe=str(root/'build/pc/Release/rf_model_probe.exe')
actual=subprocess.check_output([probe,'--render-static-batch'],input=b''.join(cases))
assert len(actual)==len(cases)*772
for n,reference in enumerate(expected):
    observed=actual[n*772:(n+1)*772]
    assert observed==reference,(n,[(i,a,b) for i,(a,b) in enumerate(zip(observed,reference)) if a!=b][:20])
bad=bytearray(cases[0]);struct.pack_into('<i',bad,320,1)
assert subprocess.check_output([probe,'--render-static-batch'],input=bad)==struct.pack('<i',-4)+b'\xa5'*768
# Complete compiled NXDK batch function with actual callees, no hooks.
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im);x.mem_map(base,65536)
entry=int(re.search(r'\s_rf_model_geometry_render_static_batch\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
g=base+0x2000;draw=base+0x2100;buffers=base+0x2200;result=base+0x3000;sentinel=base+0xff00
for n,case in enumerate(cases):
    x.mem_write(base,case);x.mem_write(g,w(draw,base,0,base+320,1,8,0,0));x.mem_write(draw,w(0,8,0,0,0))
    x.mem_write(buffers,w(result,result+256,result+352,result+448,8));x.mem_write(result,b'\xa5'*768)
    args=[g,0,base+352,base+464,base+548,base+568 if struct.unpack_from('<I',case,564)[0] else 0,buffers]
    x.mem_write(esp,w(sentinel,*args));x.reg_write(UC_X86_REG_ESP,esp);x.emu_start(entry,sentinel,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==sentinel and x.reg_read(UC_X86_REG_EAX)==0,n
    assert bytes(4)+bytes(x.mem_read(result,768))==expected[n],('NXDK',n)
    assert bytes(x.mem_read(base,592))==case,('input',n)
# Range failures must occur before any output publication.
for guard in range(4):
    x.mem_write(base,cases[0]);x.mem_write(g,w(draw,base,0,base+320,1,8,0,0));x.mem_write(draw,w(0,8,0,0,0));x.mem_write(buffers,w(result,result+256,0,result+448,8));x.mem_write(result,b'\xa5'*768)
    args=[g,0,base+352,base+464,base+548,0,buffers]
    if guard==0:x.mem_write(base+320,w(1))
    if guard==1:x.mem_write(buffers+16,w(7))
    if guard==2:args[1]=1
    if guard==3:x.mem_write(draw,w(1,8,0,0,0))
    x.mem_write(esp,w(sentinel,*args));x.reg_write(UC_X86_REG_ESP,esp);x.emu_start(entry,sentinel,count=100000)
    assert x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(result,768))==b'\xa5'*768,guard
report=dict(result='PASS',batches=len(cases),vertices=len(cases)*8,duplicates=duplicates,port_bounds_cases=1,nxdk_batches=len(cases),nxdk_guards=4,
    scope='Unchanged complete static vertex loop52e11b..52e438 with real projection/lighting/reuse callees; exact full output/cache bytes, supplied view and streams, no triangle submission or native XEMU claim')
(root/'artifacts/model-static-render-batch.json').write_text(json.dumps(report,indent=2));print(report)
