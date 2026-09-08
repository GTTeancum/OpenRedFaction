"""Compare compiled reconstruction with original x86 under isolated CPU emulation.

Requires unicorn==2.1.4. Never starts RF.exe or invokes its Windows imports.
Only the hash routine and its default-locale CRT lowercase helper execute.
"""
import argparse
import hashlib
import json
import random
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('binary', type=Path)
    parser.add_argument('--driver', type=Path, default=Path('build/pc/Release/rf_checksum_driver.exe'))
    parser.add_argument('--inventory', type=Path, default=Path('artifacts/inventory.json'))
    args = parser.parse_args()
    data = args.binary.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836':
        parser.error('Executable does not match the analyzed RF 1.20 NA baseline')
    offset, = struct.unpack_from('<I', data, 0x3c)
    count, = struct.unpack_from('<H', data, offset + 6)
    optional_size, = struct.unpack_from('<H', data, offset + 20)
    optional = offset + 24
    base, = struct.unpack_from('<I', data, optional + 28)
    image_size, = struct.unpack_from('<I', data, optional + 56)
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(base, (image_size + 4095) & ~4095)
    for i in range(count):
        pos = optional + optional_size + i * 40
        rva, raw_size, raw_offset = struct.unpack_from('<III', data, pos + 12)
        cpu.mem_write(base + rva, data[raw_offset:raw_offset + raw_size])
    # Explicitly model the verified CRT C-locale branch; other locales are out of scope.
    cpu.mem_write(0x20852f4, b'\0' * 4)
    stack, input_address, stop = 0x30000000, 0x30010000, 0x30020000
    cpu.mem_map(stack, 0x30000)
    inventory = json.loads(args.inventory.read_text(encoding='utf-8'))
    names = [e['name'].encode('cp1252') for f in inventory['files'] if 'vpp' in f for e in f['vpp']['entries']]
    rng = random.Random(2001)
    cases = [None, b'', b'A', b'a', b'tables.vpp', b'TABLES.VPP'] + names
    cases += [bytes([x]) for x in range(1, 256)]
    cases += [bytes(rng.randrange(1, 256) for _ in range(rng.randrange(1, 256))) for _ in range(1000)]
    results = []
    for value in cases:
        if value is not None:
            cpu.mem_write(input_address, value + b'\0')
        sp = stack + 0xff00
        cpu.mem_write(sp, struct.pack('<II', stop, 0 if value is None else input_address))
        cpu.reg_write(UC_X86_REG_ESP, sp)
        cpu.emu_start(0x52be70, stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original routine exceeded instruction budget')
        results.append(cpu.reg_read(UC_X86_REG_EAX))
    encoded = '\n'.join('NULL' if v is None else v.hex() for v in cases) + '\n'
    completed = subprocess.run([str(args.driver.resolve())], input=encoded, text=True, capture_output=True, check=True)
    actual = [int(line, 16) for line in completed.stdout.splitlines()]
    if actual != results:
        for index, (a, b) in enumerate(zip(actual, results)):
            if a != b:
                raise AssertionError(f'case {index} {cases[index]!r}: reconstructed={a:08x} original={b:08x}')
        raise AssertionError('Output count differs')
    report = dict(binary_sha256=digest, address='0x0052be70', locale='C',
                  cases=len(cases), archive_names=len(names), result='PASS', emulator='unicorn 2.1.4')
    Path('artifacts/checksum-verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
