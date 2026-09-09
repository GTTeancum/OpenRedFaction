"""Compare detail selection with the unchanged original dispatcher block."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;obj=base+1024;options=base+2048;position=base+4096
def put(address,value):u.mem_write(address,struct.pack('<I',value))
put(0x17c7bcc,0x66);u.mem_write(0x1818680,bytes(12))
u.mem_write(0x1818b50,struct.pack('<f',1));u.mem_write(0x1818b48,struct.pack('<f',1))
rng=random.Random(520040);cases=[];expected=[];selected=[0,0,0]
for n in range(2000):
    count=n%3+1;flags=rng.choice([0,0,0,1,8,9,0x400]);alternate=rng.randrange(2)
    minimum=rng.randrange(5);scaled=rng.randrange(2);animated=rng.randrange(2)
    metric=rng.randrange(1000)/4;thresholds=[rng.randrange(1000)/4 for _ in range(3)]
    if n%7==0:thresholds[count-1]=metric*(2.5 if scaled and animated else 1)
    if n%19==0:thresholds[count-1]=float('nan')
    if n%23==0:metric=float('nan')
    cases.append(struct.pack('<3fII4if',*thresholds,count,flags,alternate,minimum,scaled,animated,metric))
    u.mem_write(esp,bytes(1024));put(obj,count);u.mem_write(obj+16,struct.pack('<3f',*thresholds))
    u.mem_write(position,struct.pack('<3f',metric,0,0));put(options,flags)
    u.mem_write(0x1c45258,bytes([alternate]));put(0x1c45254,minimum);u.mem_write(0x64ecb9,bytes([scaled]))
    put(esp+0x40,animated);put(esp+0x38,position)
    for register,value in ((UC_X86_REG_ESP,esp),(UC_X86_REG_EDI,obj),(UC_X86_REG_ESI,options),(UC_X86_REG_EBX,flags)):
        u.reg_write(register,value)
    u.emu_start(0x52faae,0x52fb1d,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x52fb1d
    index=u.reg_read(UC_X86_REG_EBP);selected[index]+=1;expected.append(struct.pack('<iI',0,index))
probe=str(root/'build/pc/Release/rf_model_probe.exe')
actual=subprocess.check_output([probe,'--select-lod'],input=b''.join(cases))
assert actual==b''.join(expected)
for count,minimum in [(0,0),(4,0),(3,-1)]:
    case=struct.pack('<3fII4if',0,10,20,count,0,0,minimum,0,0,5)
    assert subprocess.check_output([probe,'--select-lod'],input=case)==struct.pack('<iI',-4,99)
report=dict(result='PASS',cases=len(cases),selected=selected,port_bounds_cases=3,
    scope='Unchanged 0x52faae..0x52fb1d with 0x52fbe2 branch and original metric callees; axis-aligned positions and unit metric scale, thresholds including equality and NaN; port consumes supplied metric, no loading or full renderer claim')
(root/'artifacts/model-lod-verification.json').write_text(json.dumps(report,indent=2));print(report)
