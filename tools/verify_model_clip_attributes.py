"""Compare UV/RGB edge interpolation against the unchanged x86 attribute block."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;a=base+256;b=base+512;out=base+768;t=base+1024;stub=base+2048
# Process-local harness initializes x87 factor then pops it after the tested block.
u.mem_write(stub,b'\xdd\x05'+struct.pack('<I',t)+b'\xdd\xd8')
rng=random.Random(54954);cases=[];expected=[]
for n in range(3000):
    records=[]
    for i in range(2):
        record=bytearray(rng.randbytes(48));struct.pack_into('<4f',record,28,*(rng.randint(-10000,10000)/16 for _ in range(4)));records.append(bytes(record))
    factor=rng.randrange(1025)/1024;flags=n%8
    case=b''.join(records)+struct.pack('<dII',factor,flags,0);cases.append(case)
    u.mem_write(a,records[0]);u.mem_write(b,records[1]);u.mem_write(out,b'\xa5'*48);u.mem_write(t,struct.pack('<d',factor));u.mem_write(esp,bytes(1024));u.mem_write(esp+0x68,struct.pack('<I',flags))
    for register,value in [(UC_X86_REG_ESP,esp),(UC_X86_REG_ESI,a),(UC_X86_REG_EDI,b),(UC_X86_REG_EBP,out)]:u.reg_write(register,value)
    u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(stub,stub+6,count=10);u.emu_start(0x54954c,0x549626,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x549626
    expected.append(bytes(4)+bytes(u.mem_read(out,48)));u.emu_start(stub+6,stub+8,count=10)
probe=str(root/'build/pc/Release/rf_model_probe.exe');actual=subprocess.check_output([probe,'--clip-attributes'],input=b''.join(cases))
assert actual==b''.join(expected)
for factor,flags in [(-1,5),(2,5),(float('nan'),5),(.5,8)]:
    case=cases[0][:96]+struct.pack('<dII',factor,flags,0)
    assert subprocess.check_output([probe,'--clip-attributes'],input=case)==struct.pack('<i',-4)+b'\xa5'*48
report=dict(result='PASS',cases=len(cases),port_bounds_cases=4,
    scope='Unchanged 0x54954c..0x549626 with original float-to-integer callees; supplied dyadic factors [0,1], all UV0/UV1/RGB flag combinations and preserved bytes exact; intersection factor/position/alpha and pool excluded')
(root/'artifacts/model-clip-attributes-verification.json').write_text(json.dumps(report,indent=2));print(report)
