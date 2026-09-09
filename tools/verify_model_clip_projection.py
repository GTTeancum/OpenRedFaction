"""Compare full unchanged 0x5477a0 with the shared clip projection helper."""
import hashlib,json,random,struct,subprocess,sys,math
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;record_address=base+256;stop=base+4096
rng=random.Random(0x5477a0);cases=[];expected=[]
for n in range(2400):
    position=[rng.randint(-1000,1000)/16 for _ in range(3)]
    if n%11==0:position[2]=0
    if n%17==0:position[n%3]=float('nan')
    if n%23==0:position[n%3]=float('inf')
    if n%29==0:position[n%3]=-float('inf')
    bias=[0,.125,-.125,1,float('nan')][n%5]
    if n%31==0:position[2]=bias*20
    scale=[rng.randint(1,640)/2,rng.randint(1,480)/2];offset=[rng.randint(-128,128),rng.randint(-128,128)];clamp=n%2
    record=bytearray(b'\xa5'*48);struct.pack_into('<3f',record,0,*position);record[25]=[4,0,5,6,132,4][n%6]
    cases.append(bytes(record)+struct.pack('<2f2ifI',*scale,*offset,bias,clamp))
    u.mem_write(record_address,bytes(record));u.mem_write(0x1818a5c,struct.pack('<f',scale[0]));u.mem_write(0x1818a24,struct.pack('<f',scale[1]))
    u.mem_write(0x17c7bec,struct.pack('<2i',*offset));u.mem_write(0x1e652e8,struct.pack('<f',bias));u.mem_write(0x5a445a,bytes([clamp]))
    u.mem_write(esp,struct.pack('<2I',stop,record_address));u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x5477a0,stop,count=1000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected.append(bytes(u.mem_read(record_address,48)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--project-clip'],input=b''.join(cases))
assert len(actual)==len(cases)*48
exact=0;maximum=0
for n,reference in enumerate(expected):
    observed=actual[n*48:(n+1)*48];exact+=observed==reference
    assert observed[:12]==reference[:12] and observed[24:]==reference[24:],n
    for offset in (12,16,20):
        x=struct.unpack_from('<f',observed,offset)[0];y=struct.unpack_from('<f',reference,offset)[0]
        if math.isnan(y):assert math.isnan(x),(n,offset,x,y)
        elif math.isinf(y):assert x==y,(n,offset,x,y)
        else:
            error=abs(x-y)/max(1,abs(y));maximum=max(maximum,error);assert error<=2e-6,(n,offset,x,y)
report=dict(result='PASS',cases=len(cases),bit_exact_records=exact,max_scaled_error=maximum,
    scope='Complete unchanged 0x5477a0; projection flags, clamp modes, depth bias including threshold equality, zero/negative/nonfinite coordinates; preserved bytes exact, finite projected floats within 2e-6 scaled; no triangle emission or draw claim')
(root/'artifacts/model-clip-projection-verification.json').write_text(json.dumps(report,indent=2));print(report)
