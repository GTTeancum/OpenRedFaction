"""Execute original class-cache gate and spawn sphere-copy instructions.

Uses explicit class/body storage fixtures, not a complete entity factory or
model loader. No original callees are replaced. A cold cache stops at the
builder entry; this verifier does not claim to execute its pose sampling.
"""
import hashlib
import json
import random
import struct
import sys
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_ESI

exe = ROOT / 'Installed_Game/RF.exe'
digest = hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image = pefile.PE(str(exe)).get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image) + 4095) // 4096 * 4096)
u.mem_write(0x400000, image)
entity, info, spheres, stack, stop = [0x30000000 + i * 0x100000 for i in range(5)]
for address in (entity, info, spheres, stack, stop):
    u.mem_map(address, 65536)


def put(address, *words):
    u.mem_write(address, struct.pack('<' + 'I' * len(words), *words))


def run(start, end):
    u.emu_start(start, end, count=10000)
    assert u.reg_read(UC_X86_REG_EIP) == end


rng = random.Random(0x42326c)
gate_cases = []
for cached in (False, True):
    for player in (False, True):
        u.mem_write(entity, bytes(65536))
        u.mem_write(info, bytes(65536))
        put(entity + 0x29c, info)
        put(entity + 0x7c, 8 if player else 0)
        flags = 0x24000 | (0x40000000 if cached else 0)
        put(info + 0x724, flags)
        before = bytes(u.mem_read(info, 65536))
        put(stack + 64000, stop, entity)
        u.reg_write(UC_X86_REG_ESP, stack + 64000)
        run(0x423b90, stop if cached else 0x423bd0)
        assert bytes(u.mem_read(info, 65536)) == before
        if not cached:
            sp = u.reg_read(UC_X86_REG_ESP)
            assert struct.unpack('<II', u.mem_read(sp, 8)) == (0x423bad, entity)
        gate_cases.append(dict(cached=cached, player=player,
                               outcome='skip' if cached else 'builder entry'))

copy_cases = 0
for count in range(9):
    for trial in range(20):
        # Arbitrary words also check that this is an exact vector copy, without
        # floating-point interpretation, and preserves all other sphere fields.
        source = bytes(rng.randrange(256) for _ in range(65536))
        target = bytes(rng.randrange(256) for _ in range(256))
        u.mem_write(info, source)
        u.mem_write(spheres, target)
        u.mem_write(entity, bytes(65536))
        put(entity + 0x29c, info)
        put(entity + 0x184, count, count, spheres)
        entity_before = bytes(u.mem_read(entity, 65536))
        expected = bytearray(target)
        for index in range(count):
            offset = 0xcec + 4 + 40 * index + 0x18
            expected[24 * index:24 * index + 12] = source[offset:offset + 12]
        u.reg_write(UC_X86_REG_ESI, entity)
        u.reg_write(UC_X86_REG_ESP, stack + 64000)
        run(0x42326c, 0x4232b5)
        assert bytes(u.mem_read(spheres, 256)) == expected, (count, trial)
        assert bytes(u.mem_read(info, 65536)) == source
        assert bytes(u.mem_read(entity, 65536)) == entity_before
        assert u.reg_read(UC_X86_REG_ESP) == stack + 64000
        copy_cases += 1

report = dict(result='PASS', original_sha256=digest, gate_cases=gate_cases,
              sphere_copy_cases=copy_cases,
              source='entity+29c class, inline array +cec, 40-byte records, center +18',
              destination='entity+184 array, pointer +8, 24-byte records, center +0',
              scope='Original 423b90 cache gate and 42326c..4232b4 with all copy callees; explicit storage fixtures. Cold builder, class first-use ordering, pose sampling and full factory are outside this execution.')
output = ROOT / 'artifacts/player-class-cache.json'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
