"""Audit original negative effective-bone writes into the final playback slot."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;u.mem_map(b,0x10000);actor=b+0x4000;cls=b+0x6000
rng=random.Random(0x42000c);negative=0
for case in range(1024):
    indices=[rng.choice([-1,-1,0,1,15,49]) for _ in range(2)]
    payload=rng.randbytes(0x2000);u.mem_write(b,payload);u.mem_write(actor+0x29c,struct.pack('<I',cls))
    u.mem_write(cls+0x13d8,struct.pack('<2i',*indices));u.reg_write(UC_X86_REG_EAX,b);u.reg_write(UC_X86_REG_ESI,actor)
    u.emu_start(0x42000c,0x42003a,count=100);assert u.reg_read(UC_X86_REG_EIP)==0x42003a
    expected=bytearray(payload)
    for bone in indices:expected[0x13bc+bone*48]=0
    assert bytes(u.mem_read(b,len(payload)))==expected
    if -1 in indices:
        negative+=1
        before=struct.unpack_from('<I',payload,0x12d4+15*12+4)[0]
        after=struct.unpack('<I',u.mem_read(b+0x138c,4))[0]
        assert after==before&0xffffff00
report=dict(result='PASS',cases=1024,negative_cases=negative,scope='Unhooked original42000c..42003a: complete pose memory comparison; index -1 clears low byte of playback slot15 tick at138c. Synthetic effective class fields; real class reachability not established.')
(root/'artifacts/death-missing-bone-original.json').write_text(json.dumps(report,indent=2));print(report)
