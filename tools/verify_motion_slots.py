"""Compare slot removal, selected indices and reference release with 0x51c090."""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,motions,stack,stop=[0x30000000+i*0x100000 for i in range(5)]
for a in (obj,desc,motions,stack,stop): u.mem_map(a,65536)
u.mem_write(obj+0x1d50,struct.pack('<I',desc))
for i in range(32): u.mem_write(desc+0xf5c+i*4,struct.pack('<I',motions+i*256))
rng=random.Random(0x51c090); cases=[]
for count in range(17):
    for trial in range(120):
        slots=b''.join(struct.pack('<iif',i//2 if trial%4==0 else i,rng.randrange(-10000,10000),rng.uniform(-1,2)) for i in range(16))
        selected=[rng.randrange(-1,count) for _ in range(3)]
        motion=struct.unpack_from('<i',slots,(trial%count)*12)[0] if count and trial%5 else 31
        references=rng.choice([0,1,2,99])
        cases.append(struct.pack('<I',count)+slots+struct.pack('<5i',*selected,motion,references))
run=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--remove-slot'],input=b''.join(cases),capture_output=True,check=True)
assert len(run.stdout)==len(cases)*220
for i,raw in enumerate(cases):
    motion,references=struct.unpack_from('<2i',raw,208)
    u.mem_write(obj+0x12d0,raw[:196]); u.mem_write(obj+0x1cfc,raw[196:204]); u.mem_write(obj+0x1d48,raw[204:208])
    u.mem_write(motions+motion*256+0x74,struct.pack('<i',references))
    u.mem_write(stack+64000,struct.pack('<2I',stop,motion)); u.reg_write(UC_X86_REG_ESP,stack+64000); u.reg_write(UC_X86_REG_ECX,obj)
    u.emu_start(0x51c090,stop,count=10000); assert u.reg_read(UC_X86_REG_EIP)==stop
    expected=struct.pack('<i',0)+bytes(u.mem_read(obj+0x12d0,196))+bytes(u.mem_read(obj+0x1cfc,8))+bytes(u.mem_read(obj+0x1d48,4))+raw[208:212]+bytes(u.mem_read(motions+motion*256+0x74,4))
    assert run.stdout[i*220:(i+1)*220]==expected,i
report=dict(result='PASS',samples=len(cases),scope='Original removal and reference decrement, counts 0..16, duplicate and absent motion IDs, all stored slots and selected indices')
(root/'artifacts/motion-slot-verification.json').write_text(json.dumps(report,indent=2)); print(report)
