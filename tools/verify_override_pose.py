"""Original complete override block vs shared pose blend."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;u.mem_map(b,0x10000);stack=b+0xc000
rng=random.Random(0x51b94c)
def basis():
    q=[rng.uniform(-1,1) for _ in range(4)];n=math.sqrt(sum(v*v for v in q));x,y,z,w=[v/n for v in q]
    return [1-2*y*y-2*z*z,2*x*y-2*w*z,2*(z*x+w*y),2*(w*z+x*y),1-2*x*x-2*z*z,2*z*y-2*w*x,2*z*x-2*w*y,2*(w*x+z*y),1-2*x*x-2*y*y]
cases=[]
for i in range(1024):
    current=basis();target=current[:] if i%8==0 else basis()
    cases.append(current+[rng.uniform(-100,100) for _ in range(3)]+target+[rng.choice([-2,-1,-.25,0,.25,.5,1,1.25,2,3])])
wire=b''.join(struct.pack('<22f',*v) for v in cases)
run=subprocess.run([str(root/'build/pc/Release/rf_transform_probe.exe'),'--override'],input=wire,capture_output=True,check=True)
assert len(run.stdout)==len(cases)*52
native=pefile.PE(str(root/'build/xbox/main.exe'));base=native.OPTIONAL_HEADER.ImageBase;ni=native.get_memory_mapped_image()
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(ni)+4095)//4096*4096);x.mem_write(base,ni);x.mem_map(b,0x10000);stop=b+0xff00
entry=int(re.search(r'\s_rf_model_override_pose\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for i in range(len(cases)):
    raw=wire[i*88:(i+1)*88];u.mem_write(b,raw[:48]);u.mem_write(b+0x1398,raw[48:84]);u.mem_write(b+0x13c0,raw[84:88])
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBP,b);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x51b94c,0x51b9db,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x51b9db
    expected=bytes(u.mem_read(b,48));got=run.stdout[i*52:(i+1)*52]
    assert expected[36:]==raw[36:48],(i,'translation')
    assert got==bytes(4)+expected,(i,cases[i],expected.hex(),got.hex())
    x.mem_write(b,raw);x.mem_write(stack,struct.pack('<3I',stop,b,b+48)+raw[84:88])
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
    x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    assert bytes(x.mem_read(b,48))==expected,(i,'NXDK')
report=dict(result='PASS',cases=len(cases),scope='Unhooked original51b94c..51b9db override block vs PC and NXDK C: full matrices, translation preservation, equal/random rotations and wrapped weights; no enabled/generation gates or retained owners')
(root/'artifacts/override-pose-verification.json').write_text(json.dumps(report,indent=2));print(report)
