"""Observe original 42dc00 dispatch and 42dc50 reservation/attachment lookup.

This does not execute the geometry query or publish/render a successful effect.
The failed-lookup path runs to its real return, including pool mutations.
"""
import hashlib
import itertools
import json
import struct
import sys
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_ESP, UC_X86_REG_EIP,
                               UC_X86_REG_EAX, UC_X86_REG_ECX)

exe = ROOT / 'Installed_Game/RF.exe'
digest = hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe = pefile.PE(str(exe))
data = pe.get_memory_mapped_image()
base = pe.OPTIONAL_HEADER.ImageBase
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(base, (len(data) + 4095) // 4096 * 4096)
u.mem_write(base, data)
B = 0x30000000
u.mem_map(B, 65536)
ACTOR, MODEL, META = B, B + 0x2000, B + 0x2100
NODES = [B + 0x3000 + 0x100 * i for i in range(4)]
STACK, STOP = B + 0xe000, B + 0xf000
FREE, ACTIVE = 0x62f488, 0x62f764
trace = []
dispatch = False
mutation = None
lookup = (-1, -1)


def put(address, value):
    u.mem_write(address, struct.pack('<I', value & 0xffffffff))


def get(address):
    return struct.unpack('<I', u.mem_read(address, 4))[0]


def string(address):
    return bytes(u.mem_read(address, 16)).split(b'\0', 1)[0].decode('ascii')


def returned(value=0, pop=0):
    sp = u.reg_read(UC_X86_REG_ESP)
    u.reg_write(UC_X86_REG_EAX, value & 0xffffffff)
    u.reg_write(UC_X86_REG_ESP, sp + 4 + pop)
    u.reg_write(UC_X86_REG_EIP, get(sp))


def hook(machine, address, size, unused):
    sp = machine.reg_read(UC_X86_REG_ESP)
    if dispatch:
        if address == 0x42dc50:
            trace.append((get(sp + 4), string(get(sp + 8)),
                          get(sp + 12), get(sp + 16)))
            if len(trace) == 1 and mutation is not None:
                put(ACTOR + 0x810, mutation)
            returned()
        return
    if address in (0x40a490, 0x503c00):
        # Execute both original accessors: actor[0] is a raw descriptor pointer,
        # and model[8] supplies the metadata pointer, without a kind check.
        trace.append(address)
    elif address in (0x409f90, 0x4fbcd0):
        # Scratch vector/matrix constructors have no role in this slice.
        returned()
    elif address in (0x51d5b0, 0x51d690):
        assert machine.reg_read(UC_X86_REG_ECX) == META
        trace.append((address, string(get(sp + 4))))
        returned(lookup[address == 0x51d690], 4)
    elif address == 0x5034f0:
        # Stop BEFORE attachment transform evaluation or any geometry work.
        trace.append((address, tuple(get(sp + 4 + 4*i) for i in range(4))))
        machine.emu_stop()


u.hook_add(UC_HOOK_CODE, hook)


def call(entry, args):
    trace.clear()
    for i, value in enumerate([STOP] + args):
        put(STACK + i*4, value)
    u.reg_write(UC_X86_REG_ESP, STACK)
    u.emu_start(entry, STOP, count=10000)
    assert u.reg_read(UC_X86_REG_EIP) in (STOP, 0x5034f0)


def ring(nodes):
    for i, node in enumerate(nodes):
        put(node + 0x4c, nodes[(i+1) % len(nodes)])
        put(node + 0x50, nodes[(i-1) % len(nodes)])


def check_ring(head, nodes):
    assert head == (nodes[0] if nodes else 0)
    for i, node in enumerate(nodes):
        assert get(node + 0x4c) == nodes[(i+1) % len(nodes)]
        assert get(node + 0x50) == nodes[(i-1) % len(nodes)]


dispatch = True
dispatch_cases = 0
for flags, mutation in itertools.product(
        [0, 0x08000000, 0x10000000, 0x18000000, 0xffffffff],
        [None, 0, 0x08000000, 0x10000000, 0x18000000]):
    put(ACTOR + 0x810, flags)
    expected = []
    if flags & 0x08000000:
        expected.append((ACTOR, 'eye', 0x40a00000, 0x3e800000))
    second_flags = mutation if expected and mutation is not None else flags
    if second_flags & 0x10000000:
        expected.append((ACTOR, 'spine', 0x41000000, 0x3f000000))
    call(0x42dc00, [ACTOR])
    assert trace == expected, (flags, mutation, trace, expected)
    dispatch_cases += 1

dispatch = False
gate_cases = 0
for enabled, descriptor, metadata in itertools.product([0, 1], [0, 7, 0xffffffff], [0, META]):
    put(0x5a00f0, enabled)
    put(ACTOR, descriptor)
    put(ACTOR + 0x80, MODEL)
    put(MODEL + 8, metadata)
    if enabled and descriptor and metadata:
        continue
    # Invalid pool heads must remain unobserved behind these gates.
    put(FREE, 0xdead0000)
    put(ACTIVE, 0xbeef0000)
    call(0x42dc50, [ACTOR, 0x595f18, 0x40a00000, 0x3e800000])
    assert trace == ([0x40a490, 0x503c00] if enabled else [])
    assert get(FREE) == 0xdead0000 and get(ACTIVE) == 0xbeef0000
    gate_cases += 1

reservation_cases = 0
failed_lookups = 0
# Highest first-field value wins; ties retain the first encountered node.
# All test values exceed the executable's initial search threshold.
ages = [(1.0,), (1.0, 3.0, 2.0), (3.0, 1.0, 2.0),
        (1.0, 2.0, 3.0), (3.0, 3.0, 2.0)]
threshold = struct.unpack('<f', u.mem_read(0x589510, 4))[0]
assert all(min(values) > threshold for values in ages)
for free_count, values, lookup, name in itertools.product(
        [0, 1, 2], ages, [(-1, -1), (4, -1), (-1, 9)],
        [0x595f18, 0x595f1c]):
    u.mem_write(B, bytes(0x5000))
    put(0x5a00f0, 1)
    put(ACTOR, 7)
    put(ACTOR + 0x80, MODEL)
    put(MODEL + 8, META)
    active = NODES[:len(values)]
    free = [B + 0x4000 + 0x100*i for i in range(free_count)]
    ring(active)
    ring(free)
    for node, value in zip(active, values):
        u.mem_write(node, struct.pack('<f', value))
    put(FREE, free[0] if free else 0)
    put(ACTIVE, active[0])
    if not free:
        selected = active[max(range(len(values)), key=values.__getitem__)]
        active = [node for node in active if node != selected]
        free = [selected]
    before_payload = {node: bytes(u.mem_read(node, 0x4c)) for node in NODES + free}
    call(0x42dc50, [ACTOR, name, 0x40a00000, 0x3e800000])
    expected = [0x40a490, 0x503c00, (0x51d5b0, string(name))]
    if lookup[0] == -1:
        expected.append((0x51d690, string(name)))
    index = lookup[0] if lookup[0] != -1 else lookup[1]
    if index != -1:
        expected.append((0x5034f0, (MODEL, index, ACTOR + 0x48, ACTOR + 0x3c)))
    else:
        assert u.reg_read(UC_X86_REG_EIP) == STOP
        failed_lookups += 1
    assert trace == expected, (free_count, values, lookup, trace)
    check_ring(get(ACTIVE), active)
    check_ring(get(FREE), free)
    for node, payload in before_payload.items():
        assert bytes(u.mem_read(node, 0x4c)) == payload
    reservation_cases += 1

report = dict(result='PASS', original_sha256=digest,
              dispatch_cases=dispatch_cases, gate_cases=gate_cases,
              reservation_cases=reservation_cases,
              failed_lookup_full_returns=failed_lookups,
              selection_threshold=threshold,
              scope='Original 42dc00 dispatch with callback flag mutation; '
                    '42dc50 gates, pool reservation/recycling and ordered attachment '
                    'lookup. Lookup failure runs through original return. Successful '
                    'lookup stops before 5034f0. Geometry, publication, rendering, '
                    'empty-pool corruption and C/PC/NXDK equivalence are not tested.')
(ROOT / 'artifacts/corpse-source-effects.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
