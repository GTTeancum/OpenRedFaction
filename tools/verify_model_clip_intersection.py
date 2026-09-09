"""Compare all seven original clip-plane position/factor branches."""
import hashlib,json,math,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;a=base+256;b=base+512;stop=base+2048;factor_address=stop+256
u.mem_write(stop,b'\xdd\x1d'+struct.pack('<I',factor_address))
rng=random.Random(549324);cases=[];expected=[]
for n in range(2800):
    values=[rng.randint(-1000,1000)/16 for _ in range(14)];plane=1<<(n%7)
    if n%11==0:values[5]=values[2]
    if n%19==0:values[n%14]=float('nan')
    if n%23==0:values[n%14]=float('inf')
    case=struct.pack('<14fI',*values,plane);cases.append(case)
    u.mem_write(a,case[:12]);u.mem_write(b,case[12:24]);u.mem_write(0x1818b78,case[24:28]);u.mem_write(0x1818b6c,case[28:32]);u.mem_write(0x18183b0,case[32:44]);u.mem_write(0x18186f0,case[44:56])
    u.reg_write(UC_X86_REG_FPCW,0x37f);u.mem_write(esp,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,esp)
    u.emu_start(0x549270,stop,count=10000)
    u.mem_write(esp,struct.pack('<5I',stop,plane,a,b,5));u.reg_write(UC_X86_REG_ESP,esp)
    u.emu_start(0x549310,0x54954c,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x54954c
    position=bytes(u.mem_read(u.reg_read(UC_X86_REG_EBP),12));u.emu_start(stop,stop+6,count=10)
    expected.append(position+bytes(u.mem_read(factor_address,8)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--clip-intersection'],input=b''.join(cases))
assert len(actual)==len(cases)*24
exact=0;maximum=0
for n,reference in enumerate(expected):
    status=struct.unpack_from('<i',actual,n*24)[0];assert status==0
    observed=actual[n*24+4:(n+1)*24];exact+=observed==reference
    for a,b in zip(struct.unpack('<3fd',observed),struct.unpack('<3fd',reference)):
        if math.isnan(b):assert math.isnan(a),(n,a,b)
        elif math.isinf(b):assert a==b,(n,a,b)
        else:
            error=abs(a-b)/max(1,abs(b));maximum=max(maximum,error);assert error<=2e-6,(n,a,b)
report=dict(result='PASS',cases=len(cases),bit_exact_records=exact,max_scaled_error=maximum,
    scope='Original 0x549310 through 0x54954c with allocator/vector callees and pool reset, all seven plane bits, dyadic coordinates and singular/non-finite fixtures; output position/factor tolerance 2e-6 scaled, x87 0x37f; attributes/classification/polygon traversal excluded')
(root/'artifacts/model-clip-intersection-verification.json').write_text(json.dumps(report,indent=2));print(report)
