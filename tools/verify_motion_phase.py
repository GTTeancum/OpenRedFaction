"""Compare weighted loop-phase advance with original 0x51ba80 prefix."""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,motions,data,stack=[0x30000000+i*0x100000 for i in range(5)]
for a in (obj,desc,motions,data,stack): u.mem_map(a,65536)
def f32(v): return struct.unpack('<f',struct.pack('<f',v))[0]
rng=random.Random(0x51bbc9); cases=[]; inputs=[]
for n in range(1600):
    count=n%16+1; phase=f32(rng.random()); elapsed=f32(rng.uniform(0,2)); delta=int(elapsed*30*160)
    slots=[(rng.randrange(160,20000),f32(rng.choice([.25,.5,1,2]) if n%3==0 else rng.uniform(.01,2)),int(i==0 or rng.randrange(2))) for i in range(count)]
    cases.append((phase,elapsed,slots)); inputs.append(struct.pack('<Ifi',count,phase,delta)+b''.join(struct.pack('<ifI',*s) for s in slots))
run=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--advance-phase'],input=b''.join(inputs),capture_output=True,check=True)
assert len(run.stdout)==len(cases)*16
for k,(phase,elapsed,slots) in enumerate(cases):
    u.mem_write(obj,bytes(0x2000)); u.mem_write(obj+0x1d50,struct.pack('<I',desc)); u.mem_write(obj+0x12d0,struct.pack('<I',len(slots)))
    u.mem_write(obj+0x1d04,struct.pack('<f',phase)); u.mem_write(desc+0xf58,struct.pack('<I',len(slots)))
    for i,(duration,weight,loop) in enumerate(slots):
        u.mem_write(desc+0xf5c+i*4,struct.pack('<I',motions+i*256)); u.mem_write(desc+0x120c+i,bytes([loop]))
        u.mem_write(motions+i*256+0x78,struct.pack('<I',data+i*256)); u.mem_write(data+i*256+16,struct.pack('<2i',160,160+duration))
        u.mem_write(obj+0x12d4+i*12,struct.pack('<2if',i,160,weight))
    u.mem_write(stack+64000,struct.pack('<If',0,elapsed))
    u.reg_write(UC_X86_REG_ESP,stack+64000); u.reg_write(UC_X86_REG_ECX,obj); u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x51ba80,0x51bc2c,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==0x51bc2c
    expected=bytes(u.mem_read(obj+0x1d04,4))+bytes(u.mem_read(obj+0x1d48,4))+struct.pack('<I',u.mem_read(stack+64000-0x308+0x17,1)[0])
    status,=struct.unpack_from('<i',run.stdout,k*16)
    assert status==0 and run.stdout[k*16+4:(k+1)*16]==expected,(k,status,run.stdout[k*16+4:(k+1)*16].hex(),expected.hex())
report=dict(result='PASS',samples=len(cases),scope='Original update through shared phase and dominant-slot selection; positive looping contribution, not cursor/events')
(root/'artifacts/motion-phase-verification.json').write_text(json.dumps(report,indent=2)); print(report)
