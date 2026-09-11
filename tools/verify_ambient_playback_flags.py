"""Execute original 522530; supply allocation and DirectSound boundaries.

This proves playback-mode forwarding, not device output or metadata parsing.
"""
import hashlib
import json
import struct
import sys
from pathlib import Path
import pefile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX

path = root / 'Installed_Game/RF.exe'
digest = hashlib.sha256(path.read_bytes()).hexdigest()
assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe = pefile.PE(str(path))
data = pe.get_memory_mapped_image()
m = Uc(UC_ARCH_X86, UC_MODE_32)
m.mem_map(pe.OPTIONAL_HEADER.ImageBase, (len(data)+4095)//4096*4096)
m.mem_write(pe.OPTIONAL_HEADER.ImageBase, data)
base = 0x30000000
stack, stop, obj, vtable = base+0xe000, base+0xf000, base+0x1000, base+0x1100
m.mem_map(base, 65536)
def words(*values):
    return struct.pack('<'+'I'*len(values), *(v & 0xffffffff for v in values))
def read(address, count=1):
    return struct.unpack('<'+'I'*count, m.mem_read(address, count*4))

trace = []
def hook(machine, address, size, user):
    sp = machine.reg_read(UC_X86_REG_ESP)
    result, pop = 0, 0
    if address == 0x522470:
        result = 3  # Owned test voice slot; allocator policy is supplied.
    elif address == 0x522420:
        trace.append(('gain', read(sp+4)[0]))
        result = -600  # Conversion supplied; argument forwarding is observed.
    elif address == base+0x2000:
        trace.append(('volume', *read(sp+4, 2)))
        pop = 8
    elif address == base+0x2010:
        trace.append(('pan', *read(sp+4, 2)))
        pop = 8
    elif address == base+0x2020:
        trace.append(('play', *read(sp+4, 4)))
        pop = 16
    machine.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
    machine.reg_write(UC_X86_REG_ESP, sp+4+pop)
    machine.reg_write(UC_X86_REG_EIP, read(sp)[0])

for address in (0x522470, 0x5211a0, 0x521160, 0x522420,
                base+0x2000, base+0x2010, base+0x2020):
    m.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
m.mem_write(obj, words(vtable))
for offset, function in ((0x3c, base+0x2000), (0x40, base+0x2010), (0x30, base+0x2020)):
    m.mem_write(vtable+offset, words(function))
voice = 0x1ad7520+3*44
cases = 0
for mode in (0, 1, 2, 127, 128, 255, 256, 257, 0xffffffff):
    for initial_flags in (0, 2, 0x80, 0x80000000):
        trace.clear()
        m.mem_write(0x1aed354, b'\x01')
        m.mem_write(0x1aed358, words(1234))
        m.mem_write(voice, bytes(44))
        m.mem_write(voice, words(obj))
        m.mem_write(voice+40, words(initial_flags))
        # sample zero, gain .25, centered pan, mode, allocation priority zero.
        m.mem_write(stack, words(stop, 0, 0x3e800000, 0, mode, 0))
        m.reg_write(UC_X86_REG_ESP, stack)
        m.emu_start(0x522530, stop, count=10000)
        assert m.reg_read(UC_X86_REG_EIP) == stop
        assert m.reg_read(UC_X86_REG_ESP) == stack+4
        assert m.reg_read(UC_X86_REG_EAX) == 1234
        assert trace == [('gain', 0x3e800000), ('volume', obj, (-600)&0xffffffff),
                         ('pan', obj, 0), ('play', obj, 0, 0, int(bool(mode&255)))], (mode, trace)
        assert read(voice+40)[0] == initial_flags | int((mode&255) == 1)
        assert read(voice+20)[0] == 0x3e800000
        assert read(voice+16)[0] == 1234
        cases += 1
report = dict(result='PASS', cases=cases, original_sha256=digest,
              scope='Original 522530 instructions with supplied allocator, buffer duplication, '
                    '3D-interface absence, gain conversion and DirectSound calls. Actual pan '
                    'conversion executes. Every nonzero mode low byte sends Play flag 1; '
                    'only low byte exactly 1 sets internal bit 0. No device/audio or metadata-parser proof.')
(root/'artifacts/ambient-playback-flags.json').write_text(json.dumps(report, indent=2))
print(report)
