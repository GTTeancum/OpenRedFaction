"""Compare camera metric and detail choice with unchanged original routines."""
import hashlib,json,math,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;obj=base+1024;options=base+2048;position=base+4096;stop=base+60000;stored=stop+256
# Harness-only store of the untouched original x87 return value as a double.
u.mem_write(stop,b'\xdd\x1d'+struct.pack('<I',stored))
def put(address,value):u.mem_write(address,struct.pack('<I',value))
rng=random.Random(5479);cases=[];expected=[]
for n in range(2000):
    count=n%3+1;flags=rng.choice([0,0,0,1,8,9,0x400]);alternate=rng.randrange(2)
    minimum=rng.randrange(5);scaled=rng.randrange(2);animated=rng.randrange(2);mode=0x66 if n%5 else 0
    thresholds=[rng.randrange(2000)/4 for _ in range(3)]
    point=[rng.randint(-2000,2000)/8 for _ in range(3)];camera=[rng.randint(-2000,2000)/8 for _ in range(3)]
    numerator=rng.randint(-20,20)/4;denominator=rng.randint(-20,20)/4
    if n%19==0:point[0]=float('nan')
    if n%23==0:point[1]=float('inf')
    if n%29==0:thresholds[-1]=float('nan')
    cases.append(struct.pack('<3fII4iI8f',*thresholds,count,flags,alternate,minimum,scaled,animated,mode,*point,*camera,numerator,denominator))
    put(0x17c7bcc,mode);u.mem_write(0x1818680,struct.pack('<3f',*camera))
    u.mem_write(0x1818b50,struct.pack('<f',numerator));u.mem_write(0x1818b48,struct.pack('<f',denominator))
    u.mem_write(position,struct.pack('<3f',*point));u.mem_write(esp,struct.pack('<II',stop,position));u.reg_write(UC_X86_REG_ESP,esp)
    u.emu_start(0x5182f0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    u.emu_start(stop,stop+6,count=10);metric=struct.unpack('<d',u.mem_read(stored,8))[0]
    u.mem_write(esp,bytes(1024));put(obj,count);u.mem_write(obj+16,struct.pack('<3f',*thresholds));put(options,flags)
    u.mem_write(0x1c45258,bytes([alternate]));put(0x1c45254,minimum);u.mem_write(0x64ecb9,bytes([scaled]))
    put(esp+0x40,animated);put(esp+0x38,position)
    for register,value in ((UC_X86_REG_ESP,esp),(UC_X86_REG_EDI,obj),(UC_X86_REG_ESI,options),(UC_X86_REG_EBX,flags)):u.reg_write(register,value)
    u.emu_start(0x52faae,0x52fb1d,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x52fb1d
    expected.append((u.reg_read(UC_X86_REG_EBP),metric))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--select-lod-camera'],input=b''.join(cases))
assert len(actual)==len(cases)*16
max_relative=0;exact=0
for n,(index,metric) in enumerate(expected):
    status,choice,value=struct.unpack_from('<iId',actual,n*16);assert status==0 and choice==index,(n,index,choice)
    if math.isnan(metric):assert math.isnan(value)
    elif math.isinf(metric):assert value==metric
    else:
        exact+=value==metric
        error=abs(value-metric)/max(1,abs(metric));max_relative=max(max_relative,error)
        assert error<=1e-12,(n,value,metric)
report=dict(result='PASS',cases=len(cases),exact_finite_metrics=exact,max_scaled_error=max_relative,
    scope='Complete original 0x5182f0 and unchanged callees plus detail-selection block; three-axis positions, metric scales, mode gates and non-finite inputs; all selected indices exact, metric tolerance 1e-12 * max(1,abs(reference)); double approximation is not universal x87 equivalence')
(root/'artifacts/model-lod-camera-verification.json').write_text(json.dumps(report,indent=2));print(report)
