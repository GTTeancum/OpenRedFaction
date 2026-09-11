"""Original player-constructor flash initialization vs linked NXDK reset."""
import runpy
from pathlib import Path
globals().update(runpy.run_path(str(Path(__file__).with_name('verify_screen_flash.py'))))
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EDI
entry=int(re.search(r'\s_rf_screen_flash_reset\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x4a34e5)
for case in range(512):
    owner=bytearray(rng.randbytes(0x1204));original.mem_write(base,bytes(owner))
    eax=rng.getrandbits(32);original.reg_write(UC_X86_REG_EAX,eax)
    original.reg_write(UC_X86_REG_ESI,base);original.reg_write(UC_X86_REG_EBX,0)
    original.reg_write(UC_X86_REG_EDI,0xffffffff);original.reg_write(UC_X86_REG_ESP,stack)
    original.emu_start(0x4a34e5,0x4a3523,count=1000)
    assert original.reg_read(UC_X86_REG_EIP)==0x4a3523
    result=bytes(original.mem_read(base+0x10d0,8))
    assert result==b'\0\0\0\xff\0\0\0\0'
    for offset,value in ((0x1088,eax),(0x10c8,0),(0x10cc,0),(0x48,0xffffffff),(0x4c,0xffffffff)):
        owner[offset:offset+4]=pack(value)
    owner[0x10d0:0x10d8]=result
    assert bytes(original.mem_read(base,len(owner)))==owner
    xbox.mem_write(base,bytes([0xa5])*24);xbox.mem_write(stack,pack(stop,base+8))
    xbox.reg_write(UC_X86_REG_ESP,stack);xbox.emu_start(entry,stop,count=1000)
    assert xbox.reg_read(UC_X86_REG_EIP)==stop and xbox.reg_read(UC_X86_REG_EAX)==0
    assert bytes(xbox.mem_read(base,24))==bytes([0xa5])*8+result+bytes([0xa5])*8
original.mem_write(stack,pack(stop));original.reg_write(UC_X86_REG_ESP,stack)
original.emu_start(0x50bcc0,stop,count=1000)
assert original.reg_read(UC_X86_REG_EIP)==stop
mode=struct.unpack('<I',original.mem_read(0x17756c0,4))[0]
assert mode==0x18000
report=dict(result='PASS',cases=512,original_sha256=digest,flash_render_mode=mode,
 scope='Bounded constructor4a34e5..4a3523, including real50cc40, with prepared owner/EBX0/EDI-1. Exact initialized flash bytes versus NXDK; all neighboring constructor writes accounted. PC scene owner reset covered by CTest. Not full player factory.')
(root/'artifacts/screen-flash-reset.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
