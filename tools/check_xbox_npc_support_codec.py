"""Exercise RFNC migration using compiled NXDK functions, without a PC build.

This is a codec check under x86 emulation, not an Xbox gameplay check.
The companion XEMU vehicle attachment harness checks actual support/carry.
"""
import json
from pathlib import Path
import re
import struct
import sys

import pefile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX


def seal(blob):
    struct.pack_into('<I', blob, 8, len(blob))
    blob[12:16] = bytes(4)
    checksum = 2166136261
    for value in blob:
        checksum = ((checksum ^ value) * 16777619) & 0xffffffff
    struct.pack_into('<I', blob, 12, checksum)
    return bytes(blob)


def main():
    pe = pefile.PE(str(ROOT / 'build/xbox/main.exe'))
    image = pe.get_memory_mapped_image()
    origin = pe.OPTIONAL_HEADER.ImageBase
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    machine.mem_map(origin, (len(image) + 4095) // 4096 * 4096)
    machine.mem_write(origin, image)
    base = 0x30000000
    machine.mem_map(base, 0x40000)
    identity, catalog, rows, output, count = [base + n for n in (0x4000, 0x5000, 0x10000, 0x18000, 0x20000)]
    stack, stop = base + 0x3e000, base + 0x3f000
    machine.mem_write(catalog, struct.pack('<II', 123, 1))
    symbols = (ROOT / 'build/xbox/main.map').read_text()

    def call(name, *args):
        entry = int(re.search(r'_' + name + r'\s+([0-9a-fA-F]+)', symbols)[1], 16)
        machine.mem_write(stack, struct.pack('<' + 'I' * (len(args) + 1), stop, *args))
        machine.reg_write(UC_X86_REG_ESP, stack)
        machine.emu_start(entry, stop, count=2000000)
        assert machine.reg_read(UC_X86_REG_EIP) == stop, name
        return machine.reg_read(UC_X86_REG_EAX)

    def preflight(blob):
        machine.mem_write(base, blob)
        return call('rf_npc_checkpoint_preflight', base, len(blob), identity, catalog, count)

    cases = []
    for version, size in enumerate((528, 540, 544, 548, 552, 564, 568, 572, 588), 1):
        blob = bytearray(64 + size)
        blob[:4] = b'RFNC'
        struct.pack_into('<I', blob, 4, version)
        struct.pack_into('<I', blob, 16, 1)
        struct.pack_into('<I', blob, 56, 123)
        struct.pack_into('<I', blob, 64, 1)
        struct.pack_into('<f', blob, 84, 100)
        struct.pack_into('<ii', blob, 108, -1, -1)
        if version == 9:
            struct.pack_into('<I3f', blob, 64 + 572, 4717, -1.75, 1.75, 0)
        wire = seal(blob)
        assert preflight(wire) == 0, version
        assert call('rf_npc_checkpoint_decode', base, len(wire), identity, catalog, rows, 1, count) == 0, version
        assert call('rf_npc_checkpoint_encode', identity, catalog, rows, 1, output, 4096, count) == 0, version
        written = struct.unpack('<I', machine.mem_read(count, 4))[0]
        encoded = bytes(machine.mem_read(output, written))
        expected = bytearray(blob[:64] + blob[64:] + bytes(588 - size))
        struct.pack_into('<I', expected, 4, 9)
        assert encoded == seal(expected), ('migration', version)
        cases.append(f'RFNC{version} decode and RFNC9 re-encode')

    for name, uid, velocity in (
        ('velocity without support', 0, (1, 0, 0)),
        ('invalid support UID', 0xffffffff, (0, 0, 0)),
        ('nonfinite support velocity', 4717, (float('nan'), 0, 0)),
    ):
        malformed = bytearray(wire)
        struct.pack_into('<I3f', malformed, 64 + 572, uid, *velocity)
        assert preflight(seal(malformed)) != 0, name
        cases.append('reject ' + name)
    report = dict(result='PASS', cases=cases,
                  scope='Compiled NXDK RFNC codec only; legacy basic rows and new support fields. No legacy gameplay-load claim.')
    (ROOT / 'artifacts/xbox-npc-support-codec.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
