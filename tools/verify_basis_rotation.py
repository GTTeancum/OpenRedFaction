"""Original518e10 vs shared basis-to-quaternion conversion."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;u.mem_map(b,0x10000);stack=b+0xf000;stop=b+0xff00
rng=random.Random(0x518e10)
cases=[[1,0,0,0,1,0,0,0,1],[1,0,0,0,-1,0,0,0,-1],[-1,0,0,0,1,0,0,0,-1],[-1,0,0,0,-1,0,0,0,1],[0]*9]
# General finite bases also exercise non-orthogonal inputs without inventing normalization.
for i in range(4096):cases.append([rng.uniform(-4,4) for _ in range(9)])
wire=b''.join(struct.pack('<9f',*v) for v in cases)
run=subprocess.run([str(root/'build/pc/Release/rf_transform_probe.exe'),'--basis-rotation'],input=wire,capture_output=True,check=True)
assert len(run.stdout)==len(cases)*20
native=pefile.PE(str(root/'build/xbox/main.exe'));base=native.OPTIONAL_HEADER.ImageBase;ni=native.get_memory_mapped_image()
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(ni)+4095)//4096*4096);x.mem_write(base,ni);x.mem_map(b,0x10000)
entry=int(re.search(r'\s_rf_model_basis_rotation\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
branches=[0]*4
for i in range(len(cases)):
    raw=wire[i*36:(i+1)*36];v=struct.unpack('<9f',raw)
    if (v[4]+v[8])+v[0]>=0:branch=0
    else:
        axis=int(v[0]<v[4]);axis=2 if v[axis*4]<v[8] else axis;branch=axis+1
    branches[branch]+=1
    u.mem_write(b,raw);u.mem_write(b+0x100,bytes(16));u.mem_write(stack,struct.pack('<2I',stop,b))
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,b+0x100);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x518e10,stop,count=1000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected=bytes(u.mem_read(b+0x100,16));got=run.stdout[i*20:(i+1)*20]
    assert got==bytes(4)+expected,(i,branch,expected.hex(),got.hex())
    x.mem_write(b,raw);x.mem_write(b+0x100,bytes(16));x.mem_write(stack,struct.pack('<3I',stop,b,b+0x100))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
    x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    assert bytes(x.mem_read(b+0x100,16))==expected,(i,'NXDK')
report=dict(result='PASS',cases=len(cases),branches=branches,scope='Original518e10 vs PC and NXDK C, all quaternion bytes; finite bases, four conversion branches, no normalization; no live override evaluation')
(root/'artifacts/basis-rotation-verification.json').write_text(json.dumps(report,indent=2));print(report)
