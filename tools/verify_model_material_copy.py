"""Material fixed fields through completion or first original allocation."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
obj,stack=0x30000000,0x30100000
u.mem_map(obj,65536);u.mem_map(stack,65536)
u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:uc.emu_stop(),begin=0x573619,end=0x573619)
rng=random.Random(0x503950);cases=[]
for k in range(2000):
    source=bytearray(rng.randbytes(200));destination=rng.randbytes(200)
    for offset in (0x14,0x48,0x90):
        n=rng.randrange(36);source[offset:offset+n]=b'x'*n;source[offset+n]=0
    for offset in (0x7c,0xb8,0xc0):struct.pack_into('<i',source,offset,rng.choice([-1,0,0,0,1,8]))
    cases.append(struct.pack('<i',rng.choice([1,2,3,4]))+source+destination)
out=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--material-copy'],input=b''.join(cases));assert len(out)==216*len(cases)
complete=allocations=0
for k,wire in enumerate(cases):
    kind,=struct.unpack_from('<i',wire);source=wire[4:204];destination=wire[204:]
    u.mem_write(obj,struct.pack('<i',kind));u.mem_write(obj+0x1000,source)
    u.mem_write(obj+0x2000,bytes([0xa5])*232);u.mem_write(obj+0x2010,destination)
    stop=stack+65000;u.mem_write(stack+64000,struct.pack('<5I',stop,obj,1,obj+0x2010,obj+0x1000));u.reg_write(UC_X86_REG_ESP,stack+64000)
    u.emu_start(0x503950,stop,count=100000);end=u.reg_read(UC_X86_REG_EIP)
    assert end in (stop,0x573619),hex(end)
    counts=[struct.unpack_from('<i',source,o)[0] for o in (0x7c,0xb8,0xc0)]
    plan=[max(0,n) if kind==3 else int(n>0) for n in counts]
    expected=struct.pack('<i',0)+bytes(u.mem_read(obj+0x2010,200))+struct.pack('<3I',*plan)
    assert out[k*216:k*216+216]==expected,(k,end)
    if end==stop:complete+=1;assert not any(plan)
    else:
        allocations+=1
        size,=struct.unpack('<I',bytes(u.mem_read(u.reg_read(UC_X86_REG_ESP)+4,4)))
        assert size==next(n for n in plan if n)*4
    assert bytes(u.mem_read(obj+0x1000,200))==source
    assert bytes(u.mem_read(obj+0x2000,16))==bytes([0xa5])*16
    assert bytes(u.mem_read(obj+0x20d8,16))==bytes([0xa5])*16
bad=bytearray(cases[0]);bad[4+0x14:4+0x14+36]=b'x'*36
expected=struct.pack('<i',-2)+bad[204:]+struct.pack('<3I',99,99,99)
assert subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--material-copy'],input=bad)==expected
report=dict(result='PASS',cases=len(cases),complete=complete,allocation_boundaries=allocations,malformed_rejections=1,scope='Original 503950 fixed-field copy and branch to first allocation; no-allocation paths complete; all record bytes and first allocation size checked; owned-array copying not implemented')
(root/'artifacts/model-material-copy-verification.json').write_text(json.dumps(report,indent=2));print(report)
