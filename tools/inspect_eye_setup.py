"""Execute original eye-offset setup slices with explicitly synthetic model hooks.

This verifies the caller contract, not VCM loading or animation evaluation.
No game process or host input is used.
"""
import hashlib
import json
import struct
import sys
from pathlib import Path
import pefile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_ESI, UC_X86_REG_EBX

binary = ROOT / 'Installed_Game/RF.exe'
digest = hashlib.sha256(binary.read_bytes()).hexdigest()
assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image = pefile.PE(str(binary)).get_memory_mapped_image()
cases = []
for crouching in (False, True):
    for tag in (-1, 7):
        for flags in (0, 0x20000):
            u = Uc(UC_ARCH_X86, UC_MODE_32)
            u.mem_map(0x400000, (len(image) + 4095) // 4096 * 4096)
            u.mem_write(0x400000, image)
            entity, cls, info, stack = 0x30000000, 0x30002000, 0x30004000, 0x31000000
            u.mem_map(entity, 0x10000)
            u.mem_map(stack, 0x10000)
            def word(address, value):
                u.mem_write(address, struct.pack('<I', value & 0xffffffff))
            def words(address, count):
                return struct.unpack('<' + 'I' * count, u.mem_read(address, count * 4))
            word(entity + 0x29c, cls)
            word(entity + 0x294, info)
            word(info + 0x724, flags)
            word(entity + 0x80, 0x12345678)  # Opaque synthetic model handle.
            word(entity + 0x8e4, 11)
            word(entity + 0x964, 22)
            word(cls + 0x1ec, tag)
            events = []
            def hook(uc, address, size, data):
                if address not in (0x503220, 0x503390, 0x5033f0, 0x503360, 0x5034f0):
                    return
                sp = uc.reg_read(UC_X86_REG_ESP)
                result = 0
                if address == 0x503220:
                    model, name = words(sp + 4, 2)
                    assert bytes(uc.mem_read(name, 4)) == b'eye\0'
                    events.append({'call': 'lookup_eye', 'model': model})
                    result = tag
                elif address == 0x5034f0:
                    model, found, rotation, origin, out_rotation, out_position = words(sp + 4, 6)
                    matrix = struct.unpack('<9f', uc.mem_read(rotation, 36))
                    position = struct.unpack('<3f', uc.mem_read(origin, 12))
                    assert matrix == (1, 0, 0, 0, 1, 0, 0, 0, 1)
                    assert position == (0, 0, 0)
                    assert found == tag and out_position == cls + (0xa8 if crouching else 0x9c)
                    events.append({'call': 'evaluate_tag', 'tag': found, 'identity_rotation': True, 'zero_origin': True})
                    uc.mem_write(out_position, struct.pack('<3f', 1.25, 2.5, -3.75))
                    uc.mem_write(out_rotation, struct.pack('<9f', *matrix))
                else:
                    count = {0x503390: 3, 0x5033f0: 1, 0x503360: 6}[address]
                    events.append({'call': hex(address), 'argument_bits': list(words(sp + 4, count))})
                uc.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
                uc.reg_write(UC_X86_REG_EIP, words(sp, 1)[0])
                uc.reg_write(UC_X86_REG_ESP, sp + 4)  # Hooks are cdecl wrappers.
            u.hook_add(UC_HOOK_CODE, hook)
            sp = stack + 0x8000
            u.reg_write(UC_X86_REG_ESP, sp)
            if crouching:
                u.reg_write(UC_X86_REG_ESI, entity)
                u.reg_write(UC_X86_REG_EBX, 0)
                start, stop = 0x424179, 0x424290
            else:
                word(sp + 4, entity)
                start, stop = 0x423bd0, 0x423d1f
            u.emu_start(start, stop, count=10000)
            assert u.reg_read(UC_X86_REG_EIP) == stop
            actual = struct.unpack('<3f', u.mem_read(cls + (0xa8 if crouching else 0x9c), 12))
            expected = (0, 0, 0) if tag == -1 else ((0, 2.5, 0) if flags else (1.25, 2.5, -3.75))
            assert actual == expected, (actual, expected)
            cases.append(dict(crouching=crouching, tag=tag, flags=flags, offset=actual, events=events))
report = dict(result='PASS', sha256=digest, scope='Original setup slices; synthetic model lookup/evaluation/animation hooks; no asset-derived heights', cases=cases)
(ROOT / 'artifacts/eye-setup.json').write_text(json.dumps(report, indent=2))
print(f'PASS: {len(cases)} original eye setup cases; model evaluation remains synthetic')
