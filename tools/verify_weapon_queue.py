"""Compare the complete original selection queue and timer-clear tail."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
player,stack=0x30000000,0x30100000
u.mem_map(player,65536);u.mem_map(stack,65536)
rng=random.Random(0x4acd50)
values=[-2147483648,-2,-1,0,1,63,64,2147483647]
cases=[(old,deadline,new) for old in values for deadline in values for new in values]
cases += [tuple(rng.randrange(-2147483648,2147483648) for _ in range(3)) for _ in range(128)]
out=subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--queue'],input=b''.join(struct.pack('<2i',c[0],c[1])+bytes([0xa5])*8+struct.pack('<i',c[2]) for c in cases))
assert len(out)==20*len(cases)
for k,(old,deadline,new) in enumerate(cases):
    before=bytearray([0xa5])*0x1000
    struct.pack_into('<i',before,0xf80,old);struct.pack_into('<i',before,0xb8,deadline)
    u.mem_write(player,bytes(before));stop=stack+65000
    u.mem_write(stack+64000,struct.pack('<IIi',stop,player,new));u.reg_write(UC_X86_REG_ESP,stack+64000)
    u.emu_start(0x4acd50,stop,count=100)
    assert u.reg_read(UC_X86_REG_EIP)==stop
    result=bytes(u.mem_read(player,0x1000))
    actual=(struct.unpack_from('<i',result,0xf80)[0],struct.unpack_from('<i',result,0xb8)[0])
    assert struct.unpack_from('<3i',out,k*20)==(0,*actual),(k,actual)
    assert out[k*20+12:k*20+20]==bytes([0xa5])*8
    struct.pack_into('<i',before,0xf80,actual[0]);struct.pack_into('<i',before,0xb8,actual[1])
    assert result==before,'Original modified unrelated player bytes'
report=dict(result='PASS',cases=len(cases),scope='Complete unchanged 4acd50 and tail-called 4fa3e0; full 4096-byte player view checked for unrelated writes; queue mutation only, eligibility and activation excluded')
(root/'artifacts/weapon-queue-verification.json').write_text(json.dumps(report,indent=2));print(report)
