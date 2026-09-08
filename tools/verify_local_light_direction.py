"""Execute original selected-light normalization, flag adjustment and rotation."""
import hashlib,json,math,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;flags_address=base+100;rotation=base+200
rng=random.Random(52951);cases=[];expected=[]
for n in range(2000):
    delta=[rng.uniform(-100,100) for _ in range(3)]
    if n<4:delta=[[0,0,0],[float('nan'),0,0],[float('inf'),1,0],[1e-30,1e-30,1e-30]][n]
    flags=0x400 if n%2 else 0
    matrix=[rng.randint(-32,32)/16 for _ in range(9)]
    raw=struct.pack('<3fI9f',*delta,flags,*matrix);cases.append(raw)
    u.mem_write(esp,bytes(128));u.mem_write(esp+0x2c,raw[:12]);u.mem_write(flags_address,raw[12:16]);u.mem_write(rotation,raw[16:])
    u.mem_write(esp+0x48,struct.pack('<I',flags_address));u.mem_write(esp+0x50,struct.pack('<I',rotation));u.reg_write(UC_X86_REG_ESP,esp)
    u.emu_start(0x52dd51,0x52dd9d,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x52dd9d
    expected.append(bytes(u.mem_read(0x1c3d530,12)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--local-light-direction'],input=b''.join(cases))
assert len(actual)==len(cases)*12
exact=0;maximum_error=0
for n,reference in enumerate(expected):
    observed=actual[n*12:n*12+12];exact+=observed==reference
    for a,b in zip(struct.unpack('<3f',observed),struct.unpack('<3f',reference)):
        if math.isnan(b):assert math.isnan(a)
        elif math.isinf(b):assert a==b
        else:
            maximum_error=max(maximum_error,abs(a-b));assert abs(a-b)<=max(1e-6,abs(b)*2e-6),(n,a,b)
report=dict(result='PASS',cases=len(cases),bit_exact_cases=exact,max_absolute_error=maximum_error,
    scope='Unchanged 0x52dd51..0x52dd9d and complete vector callees; both flag states and singular deltas; 2e-6 relative/1e-6 absolute tolerance; upstream light selection/setup excluded')
(root/'artifacts/local-light-direction-verification.json').write_text(json.dumps(report,indent=2));print(report)
