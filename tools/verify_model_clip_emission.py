"""Compare original clipped-vertex emission and fan indices with unchanged callees."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX,UC_X86_REG_ECX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;records_address=base+256;list_address=base+1024;vertices_address=base+4096;indices_address=base+8192;triangle_address=base+9000;attributes_address=base+9100
def word(address,value):u.mem_write(address,struct.pack('<I',value))
rng=random.Random(0x52f727);cases=[];expected=[];generated=0
for n in range(600):
    count=3+n%6;records=bytearray(b'\xa5'*384)
    for i in range(8):
        struct.pack_into('<3f',records,48*i,*[rng.randint(-128,128)/16,rng.randint(-128,128)/16,rng.randint(1,128)/16])
        records[48*i+25]=4 if (i+n)%3 else 0;records[48*i+26]=i%3
        struct.pack_into('<2f',records,48*i+28,rng.randrange(64)/16,rng.randrange(64)/16)
        records[48*i+44:48*i+47]=bytes(rng.randrange(256) for _ in range(3))
        if i<count and records[48*i+25]&4:generated+=1
    clamp=n%2;bias=[0,.125,-.125][n%3];scale=[320,240];offset=[n%31,-n%17]
    projection=struct.pack('<2f2ifI',*scale,*offset,bias,clamp)
    alpha=n%256;depth_scale=.25;reciprocal_scale=2;depth_factor=16
    attributes=struct.pack('<I4B2f',1,40,50,60,alpha,depth_scale,reciprocal_scale)
    triangle=[1,2,3];index_base=65534 if n%7==0 else 32
    cases.append(bytes(records)+projection+attributes+struct.pack('<fI4H',depth_factor,count,*triangle,index_base))
    u.mem_write(records_address,bytes(records));u.mem_write(list_address,struct.pack('<8I',*[records_address+48*i for i in range(8)]))
    u.mem_write(vertices_address,b'\xa5'*2560);u.mem_write(indices_address,b'\xa5'*288);u.mem_write(triangle_address,struct.pack('<3H',*triangle));u.mem_write(attributes_address+8,bytes([alpha]))
    u.mem_write(0x1818a5c,struct.pack('<f',scale[0]));u.mem_write(0x1818a24,struct.pack('<f',scale[1]));u.mem_write(0x17c7bec,struct.pack('<2i',*offset));u.mem_write(0x1e652e8,struct.pack('<f',bias));u.mem_write(0x5a445a,bytes([clamp]))
    u.mem_write(0x5a7dd8,struct.pack('<2f',depth_scale,reciprocal_scale));u.mem_write(0x17c7c30,struct.pack('<f',depth_factor))
    u.mem_write(esp,bytes(1024))
    for off,value in {0x48:5,0x84:64,0x1c:indices_address+6,0x2c:indices_address+288,0x68:vertices_address,0x40:triangle_address,0x30:index_base,0x14:3,0x398:attributes_address,0x5c:count}.items():word(esp+off,value)
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_EAX,list_address);u.reg_write(UC_X86_REG_ECX,count);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x52f6c1,0x52f890,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x52f890
    expected.append(struct.pack('<i',0)+bytes(u.mem_read(esp+0x48,4))+bytes(u.mem_read(esp+0x14,4))+bytes(u.mem_read(records_address,384))+bytes(u.mem_read(vertices_address,2560))+bytes(u.mem_read(indices_address,288)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--emit-clip'],input=b''.join(cases));assert len(actual)==len(cases)*3244
for n,reference in enumerate(expected):
    observed=actual[n*3244:(n+1)*3244]
    assert observed==reference,(n,next((i,a,b) for i,(a,b) in enumerate(zip(observed,reference)) if a!=b))
report=dict(result='PASS',polygons=len(cases),generated_vertices=generated,
    scope='Unchanged 0x52f6c1..0x52f84e with complete projection/depth callees; 3..8 vertex mixed retained/generated polygons, BGR/alpha/UV/depth, preserved bytes, u16 base wrap and fans exact; supplied polygons, no actual clipping integration or draw claim')
(root/'artifacts/model-clip-emission-verification.json').write_text(json.dumps(report,indent=2));print(report)
