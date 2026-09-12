"""Original463d50 loader/40e9e0 constructor with supplied I/O and array growth.

Executes original ordered adjacency filtering/deduplication. This does not
verify the original filesystem layer, allocator, or a portable runtime loader.
"""
import hashlib
import json
import struct
import sys
import pefile
from inspect_navigation_records import ROOT, inspect, sections

sys.path.insert(0, str(ROOT / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX,
                              UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW)

exe = ROOT / 'Installed_Game/RF.exe'
digest = hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe = pefile.PE(str(exe))
image = pe.get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image) + 4095) // 4096 * 4096)
u.mem_write(0x400000, image)
base = 0x30000000
u.mem_map(0, 4096)
u.mem_map(base, 0x4000000)
stack, stop, level = base + 0xe000, base + 0xf000, base + 0x4000
float_return, float_value = base + 0xc000, base + 0xc100
w = lambda *v: struct.pack('<%dI' % len(v), *v)
read = lambda a: struct.unpack('<I', u.mem_read(a, 4))[0]
u.mem_write(float_return, b'\xd9\x05' + w(float_value) + b'\xc2\x08\x00')
u.mem_write(0x6460e8, w(level))
data = b''
cursor = pool = 0
addresses = []
arrays = {}


def take(size):
    global cursor
    assert cursor + size <= len(data)
    result = data[cursor:cursor + size]
    cursor += size
    return result


def hook(cpu, address, size, context):
    global pool
    if address not in (0x5239c0, 0x523970, 0x52c910, 0x52c780, 0x52c820,
                       0x52ca00, 0x52cac0, 0x52c9b0, 0x573619, 0x45ec40):
        return
    sp = cpu.reg_read(UC_X86_REG_ESP)
    result = pop = 0
    if address == 0x5239c0:
        result = 180
    elif address == 0x523970:
        assert read(sp + 4) == 1
        result, pop = 1, 4
    elif address == 0x52c910:
        assert read(sp + 4) <= 180
        result, = struct.unpack('<I', take(4))
        pop = 8
    elif address in (0x52c780, 0x52c820):
        assert read(sp + 4) <= 180
        result = take(1)[0]
        if address == 0x52c780:
            result = int(result != 0)
        pop = 8
    elif address in (0x52ca00, 0x52cac0):
        assert read(sp + 8) <= 180
        cpu.mem_write(read(sp + 4), take(12 if address == 0x52ca00 else 36))
        pop = 12
    elif address == 0x52c9b0:
        assert read(sp + 4) <= 180
        cpu.mem_write(float_value, take(4))
        cpu.reg_write(UC_X86_REG_EIP, float_return)
        return
    elif address == 0x573619:
        assert read(sp + 4) == 124
        result = base + 0x100000 + len(addresses) * 128
        assert result < base + 0x200000
        addresses.append(result)
        cpu.mem_write(result, b'\xa5' * 128)
    else:
        owner = cpu.reg_read(UC_X86_REG_ECX)
        count = read(owner)
        if owner not in arrays:
            assert count == 0
            arrays[owner] = pool
            pool += 0x4000
            assert pool < base + 0x4000000
        assert count < 4096
        result = arrays[owner] + count * 4
        cpu.mem_write(result, w(read(sp + 4)))
        cpu.mem_write(owner, w(count + 1, 4096, arrays[owner]))
        pop = 4
    cpu.reg_write(UC_X86_REG_EAX, result)
    cpu.reg_write(UC_X86_REG_EIP, read(sp))
    cpu.reg_write(UC_X86_REG_ESP, sp + 4 + pop)


u.hook_add(UC_HOOK_CODE, hook)


def verify(payload, label):
    global data, cursor, pool, addresses, arrays
    data, cursor, pool = payload, 0, base + 0x200000
    addresses, arrays = [], {}
    nodes = inspect(data)
    u.mem_write(level + 0x300, w(0, 0, 0))
    u.mem_write(stack, w(stop, 0))
    u.reg_write(UC_X86_REG_ESP, stack)
    u.reg_write(UC_X86_REG_FPCW, 0x27f)
    u.emu_start(0x463d50, stop, count=10000000)
    assert u.reg_read(UC_X86_REG_EIP) == stop, label
    assert u.reg_read(UC_X86_REG_ESP) == stack + 4, label
    assert cursor == len(data) and len(addresses) == len(nodes), label
    assert read(level + 0x300) == len(nodes), label
    for i, (node, address) in enumerate(zip(nodes, addresses)):
        assert read(arrays[level + 0x300] + 4 * i) == address
        expected = bytearray(b'\xa5' * 128)
        expected[:12] = node['position']
        expected[24:40] = node['radius'] * 2 + node['height'] + node['word_024']
        expected[52] = 0
        expected[64:68] = w(node['word_040'])
        expected[68] = node['oriented']
        if node['oriented']:
            expected[72:108] = node['orientation']
        expected[108:112] = w(node['uid'])
        for offset, values in ((40, [addresses[j] for j in node['neighbors']]),
                               (112, node['tags'])):
            owner = address + offset
            if values:
                pointer = arrays[owner]
                expected[offset:offset + 12] = w(len(values), 4096, pointer)
                assert bytes(u.mem_read(pointer, len(values) * 4)) == w(*values), (label, i, offset)
            else:
                expected[offset:offset + 12] = w(0, 0, 0)
        assert bytes(u.mem_read(address, 128)) == expected, (label, i, 'node write footprint')
    return len(nodes)


section_count = node_count = 0
for level_info, payload in sections():
    assert level_info['version'] == 180
    node_count += verify(payload, level_info['file'])
    section_count += 1

# Authored data has few uncommon flags. Exercise normalized bools, both matrix
# branches, duplicate tags, self edges, duplicates and invalid signed indices.
synthetic = w(3)
for i in range(3):
    oriented = (0, 2, 255)[i]
    synthetic += w(100 + i) + bytes([255]) + struct.pack('<5fI', 4, i, 0, 1, 2, i)
    synthetic += bytes([oriented])
    if oriented:
        synthetic += struct.pack('<9f', *range(9))
    synthetic += b'\x02\x00\xff' + struct.pack('<f', .5) + w(3, 9, 9, 11)
for neighbors in ([1, 1, 0, 0xffffffff, 3, 2], [], [2, 0, 2, 0x80000000]):
    synthetic += bytes([len(neighbors)]) + w(*neighbors)
verify(synthetic, 'synthetic duplicate/invalid adjacency')
verify(w(0), 'empty')
report = dict(result='PASS', authored_sections=section_count, authored_nodes=node_count,
              synthetic_cases=2, original_sha256=digest,
              scope='Original463d50 and40e9e0 execute with supplied version180 stream reads, '
                    '124-byte poison allocations and45ec40 array growth. Full node write '
                    'footprints, tags and ordered filtered/deduplicated neighbors compared. '
                    'No portable runtime loader, visibility or native Xbox proof.')
(ROOT / 'artifacts/navigation-loader.json').write_text(json.dumps(report, indent=2))
print(report)
