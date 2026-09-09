"""Compare fresh-vertex normalization and lighting with the original call site."""
import hashlib,json,math,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;rgb=base+256;model=base+512
rng=random.Random(5231);cases=[];expected=[]
specials=[(0,0,0),(float('nan'),1,2),(float('inf'),1,2),(-float('inf'),0,1),(1e-40,0,0),(1e38,1e38,0)]
for n in range(2000):
    vector=specials[(n//10)%len(specials)] if n%10==0 else [rng.randint(-2000,2000)/16 for _ in range(3)]
    lights=[]
    for i in range(3):lights.extend([rng.randint(-16,16)/16 for _ in range(3)]+[rng.randint(0,255) for _ in range(3)])
    ambient=[rng.randrange(255) for _ in range(3)]
    case=struct.pack('<24f',*vector,*lights,*ambient);cases.append(case)
    u.mem_write(0x1c25e00,case[:12]);u.mem_write(0x1c3d500,case[12:]);u.mem_write(esp,bytes(1024))
    u.mem_write(esp+0x11,b'\1');u.mem_write(esp+0x58,struct.pack('<I',rgb));u.mem_write(esp+0x398,struct.pack('<I',model))
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_ESI,0)
    u.emu_start(0x52f31e,0x52f34d,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x52f34d
    expected.append(bytes(u.mem_read(0x1c25e00,12))+bytes(u.mem_read(rgb,3)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--render-vertex-lighting'],input=b''.join(cases))
assert len(actual)==len(cases)*15
exact=0;max_error=0
for n,reference in enumerate(expected):
    observed=actual[n*15:n*15+15];exact+=observed==reference
    assert observed[12:]==reference[12:],(n,observed[12:],reference[12:])
    for a,b in zip(struct.unpack('<3f',observed[:12]),struct.unpack('<3f',reference[:12])):
        if math.isnan(b):assert math.isnan(a)
        elif math.isinf(b):assert a==b
        else:
            max_error=max(max_error,abs(a-b));assert abs(a-b)<=1e-6
report=dict(result='PASS',cases=len(cases),bit_exact_records=exact,max_vector_error=max_error,
    scope='Unchanged 0x52f31e..0x52f34d, including full 0x4faaf0 normalization and 0x52fcf0 lighting; RGB exact, normalized vectors within 1e-6 with NaN classification; no clipping/reuse/drawing claim')
(root/'artifacts/render-vertex-lighting-verification.json').write_text(json.dumps(report,indent=2));print(report)
