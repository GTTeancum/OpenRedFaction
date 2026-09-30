"""Run compiled NXDK grenade flight code with bounded synthetic collision callbacks.

Checks retirement versus world bounce/fuse, without a PC executable or images.
This does not establish scene damage, rendering or collision-mesh correctness;
the XEMU grenade fixture covers scene contact/damage integration separately.
"""
import json
from pathlib import Path
import re
import struct
import sys

import pefile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX


def bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def main():
    pe = pefile.PE(str(ROOT / 'build/xbox/main.exe'))
    image = pe.get_memory_mapped_image()
    origin = pe.OPTIONAL_HEADER.ImageBase
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    machine.mem_map(origin, (len(image) + 4095) // 4096 * 4096)
    machine.mem_write(origin, image)
    base = 0x30000000
    machine.mem_map(base, 0x40000)
    flight, gravity, event = base, base + 0x100, base + 0x200
    sweep, classify, stack, stop = [base + x for x in (0x30000, 0x31000, 0x3e000, 0x3f000)]
    machine.mem_write(sweep, b'\xc3')
    machine.mem_write(classify, b'\xc3')
    machine.mem_write(gravity, bytes(12))
    symbols = (ROOT / 'build/xbox/main.map').read_text()
    entry = int(re.search(r'_rf_grenade_flight_step_objects\s+([0-9a-fA-F]+)', symbols)[1], 16)
    acceptance = 1

    def hook(cpu, address, size, context):
        if address == sweep:
            sp = cpu.reg_read(UC_X86_REG_ESP)
            _, start, delta, radius_bits, contact, matched = struct.unpack('<6I', cpu.mem_read(sp + 4, 24))
            p = struct.unpack('<3f', cpu.mem_read(start, 12))
            d = struct.unpack('<3f', cpu.mem_read(delta, 12))
            radius = struct.unpack('<f', struct.pack('<I', radius_bits))[0]
            found = d[1] < 0 and p[1] + d[1] <= radius
            cpu.mem_write(matched, struct.pack('<I', int(found)))
            if found:
                fraction = max(0, (radius - p[1]) / d[1])
                point = [p[i] + d[i] * fraction for i in range(3)]
                cpu.mem_write(contact, struct.pack('<7f3I', fraction, *point, 0, 1, 0, 0x40000000, 0, 0))
            cpu.reg_write(UC_X86_REG_EAX, 0)
        elif address == classify:
            cpu.reg_write(UC_X86_REG_EAX, acceptance)

    machine.hook_add(UC_HOOK_CODE, hook)

    def initialize(flags, fuse=2):
        machine.mem_write(flight, struct.pack('<7fI2f3I', 0, .11, 0, 0, -2, 0, .1, 0, fuse, 10, 1, 0, flags))
        machine.mem_write(event, bytes(68))

    def step(dt=.1):
        args = (stop, flight, bits(dt), gravity, bits(.5), sweep, classify, 0, event)
        machine.mem_write(stack, struct.pack('<9I', *args))
        machine.reg_write(UC_X86_REG_ESP, stack)
        machine.emu_start(entry, stop, count=1000000)
        assert machine.reg_read(UC_X86_REG_EIP) == stop
        return machine.reg_read(UC_X86_REG_EAX)

    cases = []
    for flags in (0, 0x10):
        initialize(flags)
        assert step() == 0
        assert struct.unpack('<I', machine.mem_read(flight + 40, 4))[0] == 0
        assert struct.unpack('<3I', machine.mem_read(event, 12)) == (0, 1, 0)
        assert struct.unpack('<I', machine.mem_read(event + 64, 4))[0] == 1
        assert step() == 0 and bytes(machine.mem_read(event, 12)) == bytes(12)
        assert bytes(machine.mem_read(event + 64, 4)) == bytes(4)
        cases.append(f'flags{flags}: object consumes once without later fuse explosion')
    acceptance = 0
    initialize(0)
    assert step() == 0
    assert struct.unpack('<f', machine.mem_read(flight + 16, 4))[0] > 0
    assert struct.unpack('<I', machine.mem_read(flight + 40, 4))[0] == 1
    cases.append('ordinary world contact retains bounce')
    initialize(0x10)
    assert step() == 0 and struct.unpack('<f', machine.mem_read(flight + 36, 4))[0] == -1
    assert step(0) == 0 and struct.unpack('<I', machine.mem_read(event, 4))[0] == 1
    cases.append('alternate world contact retains next-step detonation')
    acceptance = 1
    initialize(0, .02)
    assert step() == 0 and struct.unpack('<I', machine.mem_read(event + 64, 4))[0] == 1
    assert bytes(machine.mem_read(event, 4)) == bytes(4)
    cases.append('object hit before same-tick fuse deadline consumes only once')
    acceptance = 2
    initialize(0)
    saved = bytes(machine.mem_read(flight, 52))
    assert step() == 0xfffffffc
    assert bytes(machine.mem_read(flight, 52)) == saved and bytes(machine.mem_read(event, 68)) == bytes(68)
    cases.append('invalid classifier preserves flight and output')
    report = {'result': 'PASS', 'scope': 'Compiled NXDK integrator with synthetic contact callbacks', 'cases': cases}
    (ROOT / 'artifacts/xbox-grenade-contacts.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
