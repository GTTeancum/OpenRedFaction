"""Compare the original non-loop completion pass, before slot compaction."""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EBP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,motions,data,stack=[0x30000000+i*0x100000 for i in range(5)]
for a in (obj,desc,motions,data,stack): u.mem_map(a,65536)
u.mem_write(obj+0x1d50,struct.pack('<I',desc))
for i in range(16):
    u.mem_write(desc+0xf5c+i*4,struct.pack('<I',motions+i*256)); u.mem_write(motions+i*256+0x78,struct.pack('<I',data+i*256))
rng=random.Random(0x51be6a); cases=[]
for count in range(17):
    for trial in range(80):
        ends=[rng.randrange(-10000,10000) for _ in range(16)]
        slots=b''.join(struct.pack('<iif',i,ends[i]+rng.choice([-1,0,1,200]),rng.choice([0,.25,1])) for i in range(16))
        selected=[rng.randrange(-1,count) for _ in range(3)]
        extra=struct.pack('<4I6f',rng.randrange(2),rng.randrange(256),rng.getrandbits(32),rng.getrandbits(32),*(rng.uniform(-10,10) for _ in range(6)))
        cases.append(struct.pack('<I',count)+slots+struct.pack('<3i',*selected)+extra+struct.pack('<16iI',*ends,rng.getrandbits(16)))
run=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--complete-slots'],input=b''.join(cases),capture_output=True,check=True)
assert len(run.stdout)==len(cases)*252
for k,raw in enumerate(cases):
    u.mem_write(obj+0x12d0,raw[:196]); u.mem_write(obj+0x1cfc,raw[196:204]); u.mem_write(obj+0x1d48,raw[204:208])
    frozen,flag=struct.unpack_from('<2I',raw,208); u.mem_write(obj+0x1d4c,bytes([frozen])); u.mem_write(obj+0x1d14,bytes([flag])); u.mem_write(obj+0x1d18,raw[216:248])
    looping,=struct.unpack_from('<I',raw,312)
    for i in range(16):
        u.mem_write(data+i*256+20,raw[248+i*4:252+i*4]); u.mem_write(desc+0x120c+i,bytes([(looping>>i)&1]))
    esp=stack+64000; u.mem_write(esp+0x18,struct.pack('<I',desc+0xf5c)); u.reg_write(UC_X86_REG_ESP,esp); u.reg_write(UC_X86_REG_ESI,obj); u.reg_write(UC_X86_REG_EBP,desc+0xf5c)
    u.emu_start(0x51be6a,0x51bf57,count=10000); assert u.reg_read(UC_X86_REG_EIP)==0x51bf57
    expected=struct.pack('<i',0)+bytes(u.mem_read(obj+0x12d0,196))+bytes(u.mem_read(obj+0x1cfc,8))+bytes(u.mem_read(obj+0x1d48,4))+struct.pack('<2I',u.mem_read(obj+0x1d4c,1)[0],u.mem_read(obj+0x1d14,1)[0])+bytes(u.mem_read(obj+0x1d18,32))
    assert run.stdout[k*252:(k+1)*252]==expected,k
report=dict(result='PASS',samples=len(cases),scope='Original completion pass including primary-state clearing; no cursor advance or slot removal')
(root/'artifacts/motion-completion-verification.json').write_text(json.dumps(report,indent=2)); print(report)
