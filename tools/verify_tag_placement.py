"""Compare character tag placement with unhooked original 0x5034f0."""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,handle,data,stack,stop=[0x30000000+i*0x100000 for i in range(6)]
for a in (obj,desc,handle,data,stack,stop): u.mem_map(a,65536)
u.mem_write(handle,struct.pack('<2I',2,obj)); u.mem_write(obj+0x1d50,struct.pack('<I',desc)); u.mem_write(desc+0x48,struct.pack('<I',1))
# A valid cached bone avoids unrelated animation state; original tag lookup and
# placement code remain unhooked and execute normally.
rng=random.Random(0x5034f0)
cases=[]
identity=[1,0,0,0,1,0,0,0,1]
for i in range(1000):
    local=[rng.uniform(-10,10) for _ in range(12)]
    orientation=identity if i%5==0 else [rng.uniform(-2,2) for _ in range(9)]
    position=[0,0,0] if i%7==0 else [rng.uniform(-1000,1000) for _ in range(3)]
    cases.append(struct.pack('<24f',*(local+orientation+position)))
run=subprocess.run([str(root/'build/pc/Release/rf_tag_place_probe.exe')],input=b''.join(cases),capture_output=True,check=True)
assert len(run.stdout)==len(cases)*52
failures=[]
for i,raw in enumerate(cases):
    u.mem_write(obj,raw[:48]); u.mem_write(data,raw[48:])
    u.mem_write(stack+64000,struct.pack('<7I',stop,handle,0,data,data+36,data+128,data+164))
    u.reg_write(UC_X86_REG_ESP,stack+64000); u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x5034f0,stop,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==stop
    want=bytes(u.mem_read(data+128,48)); status,=struct.unpack_from('<i',run.stdout,i*52); got=run.stdout[i*52+4:(i+1)*52]
    if status or want!=got: failures.append(dict(index=i,status=status,expected=want.hex(),actual=got.hex()))
report=dict(result='PASS' if not failures else 'FAIL',samples=len(cases),failures=len(failures),examples=failures[:5],scope='Original character wrapper with cached tag; not animation or other model types')
(root/'artifacts/tag-placement-verification.json').write_text(json.dumps(report,indent=2)); print(report)
assert not failures
