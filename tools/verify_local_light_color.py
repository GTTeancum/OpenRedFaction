"""Compare original selected-light attenuation tail with portable arithmetic."""
import hashlib,json,math,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;obj=base+4096
rng=random.Random(5299);cases=[];expected=[]
for n in range(2000):
    radius=rng.uniform(.01,10000);distance=rng.uniform(0,radius*2)
    if n<8:distance,radius=[(0,1),(1,1),(2,1),(0,0),(1,0),(-1,1),(float('nan'),1),(1,float('inf'))][n]
    raw=struct.pack('<5f',distance,radius,*(rng.uniform(-1,2) for _ in range(3)));cases.append(raw)
    u.mem_write(esp,bytes(128));u.mem_write(esp+0x10,raw[:4]);u.mem_write(obj+0x80,raw[4:8]);u.mem_write(obj+0x40,raw[8:])
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_EBP,obj)
    u.emu_start(0x52dd9d,0x52ddda,count=1000);assert u.reg_read(UC_X86_REG_EIP)==0x52ddda
    expected.append(bytes(u.mem_read(0x1c3d53c,12)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--local-light-color'],input=b''.join(cases))
assert len(actual)==len(cases)*12
exact=0;maximum_error=0
for n,reference in enumerate(expected):
    observed=actual[n*12:n*12+12];exact+=observed==reference
    for a,b in zip(struct.unpack('<3f',observed),struct.unpack('<3f',reference)):
        if math.isnan(b):assert math.isnan(a)
        elif math.isinf(b):assert a==b
        else:
            error=abs(a-b);maximum_error=max(maximum_error,error)
            assert error<=max(1e-5,abs(b)*2e-6),(n,a,b)
report=dict(result='PASS',cases=len(cases),bit_exact_cases=exact,max_absolute_error=maximum_error,
    scope='Unmodified 0x52dd9d..0x52ddda selected-light tail; random finite and singular cases, 2e-6 relative/1e-5 absolute tolerance; direction and setup excluded')
(root/'artifacts/local-light-color-verification.json').write_text(json.dumps(report,indent=2));print(report)
