"""Compare the concave empty-grid fallback's PC/NXDK sphere bytes.

Runs compiled NXDK body preparation through the owned-body allocation boundary;
that boundary is intercepted to inspect the generated spheres, not to emulate
allocation or motion. This is port-policy validation, not retail or XEMU parity.
"""
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import pefile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW

pack = lambda *v: struct.pack('<' + 'I' * len(v), *v)
floats = lambda *v: struct.pack('<' + 'f' * len(v), *v)
pc = subprocess.check_output([str(ROOT / 'build/pc/Release/rf_geomod_disconnected_tests.exe')], cwd=ROOT, text=True)
expected = {}
for line in pc.splitlines():
    if line.startswith('THIN_SPHERES '):
        _, axis, count, data = line.split()
        expected[int(axis)] = (int(count), bytes.fromhex(data))
assert len(expected) == 3
pe = pefile.PE(str(ROOT / 'build/xbox/main.exe'))
image = pe.get_memory_mapped_image()
origin = pe.OPTIONAL_HEADER.ImageBase
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(origin, (len(image) + 4095) & ~4095)
u.mem_write(origin, image)
base = 0x30000000
u.mem_map(base, 0x10000)
stack, stop = base + 0xe000, base + 0xf000
symbols = (ROOT / 'build/xbox/main.map').read_text()
symbol = lambda name: int(re.search('_' + name + r'\s+([0-9a-fA-F]+)', symbols)[1], 16)
captured = []

def body_boundary(cpu, address, size, context):
    sp = cpu.reg_read(UC_X86_REG_ESP)
    ret, parameters, spheres, count, budget, output = struct.unpack('<6I', cpu.mem_read(sp, 24))
    assert count <= 64 and budget == 4096
    captured.append((count, bytes(cpu.mem_read(spheres, count * 24))))
    cpu.reg_write(UC_X86_REG_EAX, 0)
    cpu.reg_write(UC_X86_REG_ESP, sp + 4)
    cpu.reg_write(UC_X86_REG_EIP, ret)

u.hook_add(UC_HOOK_CODE, body_boundary, begin=symbol('rf_physics_body_open'), end=symbol('rf_physics_body_open'))
outline = [(0, 0), (10, 0), (10, .2), (.2, .2), (.2, 10), (0, 10)]
vertices, faces = [], []
for i in range(6):
    faces.append((len(vertices), 4, 0, 0xffffffff))
    for j, index in enumerate([i, i, (i + 1) % 6, (i + 1) % 6]):
        vertices.append((outline[index][0] - 5, .05 if j in (1, 2) else -.05, outline[index][1] - 5))
for side in range(2):
    for i in range(1, 5):
        faces.append((len(vertices), 3, 0, 0xffffffff))
        for index in [0, i + 1 if side else i, i if side else i + 1]:
            vertices.append((outline[index][0] - 5, .05 if side else -.05, outline[index][1] - 5))
for axis in range(3):
    u.mem_write(base, bytes(0xd000))
    u.mem_write(base, pack(base + 0x1000, base + 0x3000, len(vertices), len(faces), 0))
    u.mem_write(base + 76, pack(1) + floats(8))  # mass_ready, birth_radius
    u.mem_write(base + 148, floats(2.5))  # mass.spacing
    u.mem_write(base + 176, floats(1))  # mass.mass
    for i, vertex in enumerate(vertices):
        rotated = [0., 0., 0.]
        for k in range(3):
            rotated[(k + axis) % 3] = vertex[k]
        u.mem_write(base + 0x1000 + i * 20, floats(*rotated, 0, 0))
    for i, face in enumerate(faces):
        u.mem_write(base + 0x3000 + i * 16, pack(*face))
    u.mem_write(stack, pack(stop, base) + floats(.5, .25) + pack(4096, base + 0x5000))
    u.reg_write(UC_X86_REG_ESP, stack)
    u.reg_write(UC_X86_REG_FPCW, 0x27f)
    captured.clear()
    u.emu_start(symbol('rf_geomod_piece_body_open'), stop, count=10000000)
    assert u.reg_read(UC_X86_REG_EIP) == stop and u.reg_read(UC_X86_REG_EAX) == 0
    assert captured == [expected[axis]], ('NXDK mismatch', axis, captured, expected[axis])
report = dict(result='PASS', orientations=3, spheres=[expected[i][0] for i in range(3)], scope=__doc__)
(ROOT / 'artifacts/thin-piece-spheres-verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report, indent=2))
