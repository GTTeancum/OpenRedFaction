"""Verify the three clipping records assembled by the original model renderer."""
import hashlib,json,re,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EDI,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;triangle=base+256;batch=base+512;reuse=base+1024;uv=base+2048;material=base+4096
rng=random.Random(52599);cases=[];expected=[]
for n in range(2000):
    vertices=[];cache=[];clips=[];distances=[]
    for i in range(4):
        v=struct.pack('<8f',*(rng.randint(-100,100)/16 for _ in range(8)))+bytes(8);vertices.append(v)
        r=struct.pack('<6fBB6s',*(rng.randint(-100,100)/16 for _ in range(6)),rng.randrange(256),0,rng.randbytes(6));cache.append(r)
        clip=struct.pack('<3f',*(rng.randint(-100,100)/16 for _ in range(3)));clips.append(clip)
        # Includes bounded negative distances to test original signed subtraction.
        distance=i-rng.randrange(4);distances.append(distance)
        u.mem_write(0x1c0e700+i*12,clip);u.mem_write(0x1c3f494+i*3,r[26:29]);u.mem_write(0x1c3d554+i,r[24:25]);u.mem_write(uv+i*8,v[24:32]);u.mem_write(reuse+i*2,struct.pack('<h',distance))
    indices=[rng.randrange(4) for _ in range(3)];output=struct.pack('<I',n%2)+rng.randbytes(12)
    cases.append(b''.join(vertices)+struct.pack('<4i',*distances)+b''.join(cache)+b''.join(clips)+struct.pack('<4H',*indices,0)+output)
    u.mem_write(esp,bytes(1024));u.mem_write(esp+0x128,b'\xa5'*144);u.mem_write(esp+0x11,bytes([n%2]));u.mem_write(material,output[4:])
    u.mem_write(batch+0x18,struct.pack('<I',reuse));u.mem_write(batch+12,struct.pack('<I',uv));u.mem_write(triangle,struct.pack('<3H',*indices))
    u.mem_write(esp+0x18,struct.pack('<I',batch));u.mem_write(esp+0x48,struct.pack('<I',triangle));u.mem_write(esp+0x2c,struct.pack('<I',material if n%2 else 0));u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_EDI,triangle)
    u.emu_start(0x52e560,0x52e62c,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x52e62c
    expected.append(bytes(4)+bytes(u.mem_read(esp+0x128,144)))
probe=str(root/'build/pc/Release/rf_model_probe.exe');actual=subprocess.check_output([probe,'--prepare-static-clip'],input=b''.join(cases))
assert actual==b''.join(expected)
for offset,fmt,value in [(352,'<H',4),(352,'<H',32768),(160,'<i',100)]:
    case=bytearray(cases[0]);struct.pack_into('<3H',case,352,0,1,2);struct.pack_into(fmt,case,offset,value)
    assert subprocess.check_output([probe,'--prepare-static-clip'],input=case)==struct.pack('<i',-4)+b'\xa5'*144
# Verify the actual compiled NXDK function and callees, without hooks.
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im);x.mem_map(base,65536)
entry=int(re.search(r'\s_rf_model_prepare_static_clip_triangle\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
destination=base+5000;sentinel=base+60000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
def nxdk(case):
    x.mem_write(base,bytes(case));x.mem_write(destination,b'\xa5'*144)
    args=[base,base+160,base+176,base+304,4,base+352,base+364 if struct.unpack_from('<I',case,360)[0] else 0,destination]
    x.mem_write(esp,w(sentinel,*args));x.reg_write(UC_X86_REG_ESP,esp);x.emu_start(entry,sentinel,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==sentinel
    assert bytes(x.mem_read(base,376))==bytes(case)
    return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(destination,144))
for n,case in enumerate(cases):assert nxdk(case)==expected[n],('NXDK',n)
for offset,fmt,value in [(352,'<H',4),(352,'<H',32768),(160,'<i',100)]:
    case=bytearray(cases[0]);struct.pack_into('<3H',case,352,0,1,2);struct.pack_into(fmt,case,offset,value)
    assert nxdk(case)==struct.pack('<i',-4)+b'\xa5'*144
report=dict(result='PASS',cases=len(cases),records=len(cases)*3,port_bounds_cases=3,nxdk_cases=len(cases),nxdk_guards=3,
    scope='Unchanged 0x52e560..0x52e62c with actual vector-copy callee, initialized record storage; all 144 clipping-input bytes exact including preserved fields, own UV/mask and reused position/RGB; polygon interpolation excluded')
(root/'artifacts/model-static-clip-inputs.json').write_text(json.dumps(report,indent=2));print(report)
