"""Bounded original getter/default evidence; runtime scale selection remains open."""
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP

exe = ROOT / 'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest() == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im = pefile.PE(str(exe)).get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(im) + 4095) // 4096 * 4096)
u.mem_write(0x400000, im)
B = 0x30000000
u.mem_map(B, 0x10000)
CLASS, STACK, OUT, STUB, STOP = B + 0x1000, B + 0xe000, B + 0x2000, B + 0xf000, B + 0xf100
words = lambda *v: struct.pack('<' + 'I' * len(v), *v)
floats = lambda *v: struct.pack('<' + 'f' * len(v), *v)

# Execute the actual absent-tag branch; EBX's 1.0 seed is established at4c3731.
assert im[0xc3731:0xc3736] == b'\xbb\x00\x00\x80\x3f'
u.reg_write(UC_X86_REG_ESI, CLASS)
u.reg_write(UC_X86_REG_EBX, 0x3f800000)
u.emu_start(0x4c3b31, 0x4c3b3d, count=20)
assert bytes(u.mem_read(CLASS + 0x118, 8)) == floats(1, 1)

# Boundaries supplied: owner resolution and player/controlled-owner predicate.
# All arithmetic/primary-alt/global-mode selection in4c8b10 runs unchanged.
u.mem_write(0x40a0e0, b'\xb8' + words(B + 0x3000) + b'\xc3')
u.mem_write(B + 0x294, words(CLASS))
u.mem_write(STUB, b'\xd9\x1d' + words(OUT) + b'\x83\xc4\x08\xc3')
u.mem_write(CLASS + 0x108, floats(40, 60, 120, 180))
records = []
for player in (0, 1):
    u.mem_write(0x48aaf0, b'\xb8' + words(player) + b'\xc3')
    u.ctl_remove_cache(0x48aaf0, 0x48ab00)
    for alternate in (0, 1):
        for mode_a, mode_b in ((0, 0), (1, 0), (0, 1), (1, 1)):
            for scale in (0, .1, .4, 1, 2):
                u.mem_write(0x64ecb9, bytes([mode_a]))
                u.mem_write(0x6fc4d8, bytes([mode_b]))
                u.mem_write(CLASS + 0x120, floats(scale))
                u.mem_write(STACK, words(STUB, B, alternate, STOP))
                u.reg_write(UC_X86_REG_ESP, STACK)
                u.emu_start(0x4c8b10, STOP, count=200)
                assert u.reg_read(UC_X86_REG_EIP) == STOP
                damage = (40, 60, 120, 180)[alternate * 2 + bool(mode_a or mode_b)]
                scale32 = struct.unpack('<f', floats(scale))[0]
                expected = floats(damage if player else damage * scale32)
                assert bytes(u.mem_read(OUT, 4)) == expected, (player, alternate, mode_a, mode_b, scale, bytes(u.mem_read(OUT, 4)).hex(), expected.hex())
                records.append(dict(player=player, alternate=alternate, mode_a=mode_a,
                                    mode_b=mode_b, scale=scale, damage=struct.unpack('<f', expected)[0]))
report = dict(result='PASS', getter_cases=len(records), default_pair=[1, 1],
              boundary='Owner lookup and player predicate supplied; original getter arithmetic executes.',
              unresolved='Selection/interpolation of authored118/11c into runtime120; live NPC integration.',
              records=records)
out = ROOT / 'artifacts/ai-damage-scale.json'
out.write_text(json.dumps(report, indent=2))
print(f'PASS: original default pair and {len(records)} original getter cases; runtime scale selection still open')
