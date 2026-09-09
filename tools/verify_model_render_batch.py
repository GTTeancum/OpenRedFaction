"""Compare resident batch processing to consecutive unchanged original vertex paths."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;out=base+256;material=base+1024;batch=base+2048;obj=base+4096;raw=base+8192;links=base+9000;reuse=base+10000
def put(address,value):u.mem_write(address,struct.pack('<I',value))
rng=random.Random(528);cases=[];expected=[];duplicates=0
for n in range(250):
    vertices=[];distances=[];fresh=[]
    for i in range(8):
        distance=i-rng.choice(fresh) if fresh and rng.randrange(3)==0 else 0
        if not distance:fresh.append(i)
        duplicates+=distance>0;distances.append(distance)
        v=struct.pack('<8f',*(rng.randint(-64,64)/16 for _ in range(8)))+bytes([128,128,0,0])+bytes([rng.randrange(4),rng.randrange(4),255,255])
        vertices.append(v);u.mem_write(raw+i*40,v);u.mem_write(links+i*8,v[32:]);u.mem_write(reuse+i*2,struct.pack('<h',distance))
    matrices=struct.pack('<48f',*(rng.randint(-32,32)/16 for _ in range(48)))
    camera=[0,0,-10];rotation=[1,0,0,0,1,0,0,0,1];screen=[320,-240,320,240];bounds=[0,0,640,480]
    flags=[(n>>i)&1 for i in range(5)]
    view=struct.pack('<23f5I',*camera,*rotation,1,2,*screen,*bounds,20,*flags)
    lights=struct.pack('<21f',*(rng.randint(-16,16)/16 for _ in range(18)),30,40,50)
    output=struct.pack('<I3sB2f',n%2,b'\x40\x80\xc0',180,1,1)
    cases.append(b''.join(vertices)+struct.pack('<8i',*distances)+matrices+view+lights+output)
    for address,size in [(0x1bf7000,96),(0x1bdb2b0,96),(0x1c25e00,96),(0x1c0e700,96),(0x1c3d554,8),(0x1bf29b0,8),(0x1c3f494,24),(out,320)]:u.mem_write(address,b'\xa5'*size)
    u.mem_write(obj+0x960,matrices);put(batch+0x1c,links);u.mem_write(material+8,b'\xb4');u.mem_write(material+12,b'\x40\x80\xc0')
    u.mem_write(0x1818690,view[:12]);u.mem_write(0x18186c8,view[12:48]);u.mem_write(0x1818b7c,view[48:52]);u.mem_write(0x17c7c30,view[52:56]);u.mem_write(0x1cfcb3c,view[72:88]);u.mem_write(0x1818b6c,view[88:92])
    u.mem_write(0x1c3d500,lights);u.mem_write(0x5a7dd8,output[8:12]);u.mem_write(0x5a7ddc,output[12:])
    for address,value in [(0x5a4d19,flags[0]),(0x5a4d18,flags[2]),(0x1818b65,flags[3])]:u.mem_write(address,bytes([value]))
    for i in range(8):
        u.mem_write(esp,bytes(1024));u.mem_write(esp+0x11,bytes([n%2,flags[1],flags[4]]))
        for offset,value in [(0x14,out+i*40+18),(0x18,i),(0x1c,reuse+i*2),(0x28,raw+i*40+24),(0x2c,0x1bf7008+i*12),(0x34,0x1c25e04+i*12),(0x38,batch),(0x58,0x1c3f494+i*3),(0x64,0x1c0e700+i*12),(0x390,obj),(0x398,material)]:put(esp+offset,value)
        for offset,value in [(0x94,screen[0]),(0x90,screen[1]),(0x8c,screen[2]),(0x98,screen[3])]:u.mem_write(esp+offset,struct.pack('<f',value))
        for register,value in [(UC_X86_REG_ESP,esp),(UC_X86_REG_ESI,i*12),(UC_X86_REG_EBX,i),(UC_X86_REG_EDI,raw+i*40),(UC_X86_REG_EBP,raw+i*40+12)]:u.reg_write(register,value)
        u.emu_start(0x52edac,0x52f3cc,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x52f3cc
    cache=b''.join(bytes(u.mem_read(0x1bf7000+i*12,12))+bytes(u.mem_read(0x1bdb2b0+i*12,12))+bytes(u.mem_read(0x1c3d554+i,1))+bytes(u.mem_read(0x1bf29b0+i,1))+bytes(u.mem_read(0x1c3f494+i*3,3))+b'\xa5'*3 for i in range(8))
    expected.append(bytes(4)+cache+bytes(u.mem_read(0x1c0e700,96))+bytes(u.mem_read(0x1c25e00,96))+bytes(u.mem_read(out,320)))
probe=str(root/'build/pc/Release/rf_model_probe.exe')
actual=subprocess.check_output([probe,'--render-batch'],input=b''.join(cases))
assert len(actual)==len(cases)*772
for n,reference in enumerate(expected):assert actual[n*772:(n+1)*772]==reference,n
bad=bytearray(cases[0]);struct.pack_into('<i',bad,320,1)
assert subprocess.check_output([probe,'--render-batch'],input=bad)==struct.pack('<i',-4)+b'\xa5'*768
bad=bytearray(cases[0]);bad[36]=255
assert subprocess.check_output([probe,'--render-batch'],input=bad)==struct.pack('<i',-4)+b'\xa5'*768
report=dict(result='PASS',batches=len(cases),vertices=len(cases)*8,duplicates=duplicates,port_bounds_cases=2,
    scope='Consecutive original 0x52edac..0x52f3cc vertex paths with persistent batch caches, full deformation/projection/lighting/reuse callees; all output bytes exact; supplied streams/matrices/view, per-vertex register setup external, no triangle submission')
(root/'artifacts/model-render-batch-verification.json').write_text(json.dumps(report,indent=2));print(report)
