"""Execute original Holey01 construction through bounds/radius finalization.

Original instructions choose the template, construct/copy vectors and submit
face corners. Allocation and mesh containers are supplied capture boundaries.
This verifies factory inputs and bounds/radius math, not the original CSG.
No original bytes or generated mesh data are written into tracked source.
"""
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_ECX, UC_X86_REG_EAX, UC_X86_REG_FPCW
from extract_geomod_template import SHA, run as extract


def run():
    exe = ROOT / 'Installed_Game/RF.exe'
    assert hashlib.sha256(exe.read_bytes()).hexdigest() == SHA
    pe = pefile.PE(str(exe))
    image = pe.get_memory_mapped_image()
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(pe.OPTIONAL_HEADER.ImageBase, (len(image) + 4095) & ~4095)
    cpu.mem_write(pe.OPTIONAL_HEADER.ImageBase, image)
    cpu.mem_map(0, 4096)  # Process-local exception chain used by the factory.
    base = 0x30000000
    cpu.mem_map(base, 0x100000)
    solid, table, vertex_base, face_base = base, base + 0x1000, base + 0x2000, base + 0x3000
    name, stack, stop = base + 0x8000, base + 0xe0000, base + 0xf0000
    pack = lambda *v: struct.pack('<' + 'I' * len(v), *v)
    read = lambda a: struct.unpack('<I', cpu.mem_read(a, 4))[0]
    cpu.mem_write(name, b'Holey01.v3d\0')
    cpu.mem_write(stack, pack(stop, name))
    cpu.reg_write(UC_X86_REG_ESP, stack)
    cpu.reg_write(UC_X86_REG_FPCW, 0x37f)
    vertices, faces, uv, boundaries, properties = [], [], [], [], []
    controlled = {0x573619, 0x4cf060, 0x4cfc80, 0x4cfab0, 0x40a480, 0x4e0140, 0x40a490, 0x4e76f8}
    completed = False

    def hook(u, address, size, _):
        nonlocal completed
        if address not in controlled:
            return
        sp = u.reg_read(UC_X86_REG_ESP)
        arg = lambda i: read(sp + 4 + i * 4)
        ecx = u.reg_read(UC_X86_REG_ECX)
        pop, result = 0, 0
        boundaries.append(hex(address))
        if address == 0x573619:
            assert arg(0) == 0x378 and not vertices
            result = solid
        elif address == 0x4cf060:
            assert ecx == solid
            result = solid
        elif address == 0x4cfc80:
            assert ecx == solid and len(vertices) < 10
            vertices.append(list(struct.unpack('<3I', u.mem_read(arg(0), 12))))
            result = vertex_base + (len(vertices) - 1) * 16
            u.mem_write(result, pack(*vertices[-1]))
            pop = 4
        elif address == 0x40a490:
            assert ecx == solid + 0x78
            result = len(vertices)
        elif address == 0x4cfab0:
            assert ecx == solid and len(vertices) == 10 and len(faces) < 16
            properties.append(list(struct.unpack('<6I',u.mem_read(arg(0),24))))
            faces.append([])
            uv.append([])
            result = face_base + (len(faces) - 1) * 256
            pop = 4
        elif address == 0x40a480:
            assert ecx == solid + 0x78 and arg(0) < 10
            result = table + arg(0) * 4
            u.mem_write(result, pack(vertex_base + arg(0) * 16))
            pop = 4
        elif address == 0x4e0140:
            assert faces and ecx == face_base + (len(faces) - 1) * 256
            index, remainder = divmod(arg(0) - vertex_base, 16)
            assert remainder == 0 and 0 <= index < 10 and len(faces[-1]) < 3
            assert arg(3) == arg(4) == 0
            faces[-1].append(index)
            uv[-1].append([arg(1), arg(2)])
            pop = 20
        else:
            assert len(faces) == 16 and all(len(f) == 3 for f in faces)
            completed = True
            u.emu_stop()
            return
        u.reg_write(UC_X86_REG_EAX, result)
        u.reg_write(UC_X86_REG_EIP, read(sp))
        u.reg_write(UC_X86_REG_ESP, sp + 4 + pop)

    cpu.hook_add(UC_HOOK_CODE, hook)
    cpu.emu_start(0x4e6d60, stop, count=100000)
    assert completed, hex(cpu.reg_read(UC_X86_REG_EIP))
    extract()
    candidate = json.loads((ROOT / 'artifacts/geomod-holey01.json').read_text())
    assert vertices == candidate['vertex_words'], 'Original vertex submissions differ'
    assert faces == candidate['faces'], 'Original face order differs'
    assert uv == candidate['uv_words'], 'Original corner UVs differ'
    assert len(properties)==16 and all(p==[0x100,0,0xffffffff,0xffff0000,0xffffffff,0] for p in properties),properties
    bounds = list(struct.unpack('<10f', cpu.mem_read(solid + 0x48, 40)))
    assert all(math.isfinite(v) for v in bounds) and bounds[6] > 0
    for vertex in candidate['vertices']:
        assert all(bounds[k] <= vertex[k] <= bounds[k+3] for k in range(3))
        assert sum((vertex[k]-bounds[k+7])**2 for k in range(3)) <= (bounds[6]+1e-6)**2
    report = dict(exe_sha256=SHA, entry='004e6d60', stop_before='004e76f8',
        scope=__doc__, vertex_words=vertices, faces=faces, uv_words=uv,
        face_property_words=properties, template_detail_classes=[(p[0]>>8)&3 for p in properties],
        controlled_boundaries=sorted(set(boundaries)),
        bounds_radius_center=bounds,
        bit_exact_match=True, convex=candidate['convex'])
    (ROOT / 'artifacts/geomod-holey01-original.json').write_text(json.dumps(report, indent=2) + '\n')
    # Local CSG fixture: outward corner order, scale10 for world-space tolerances.
    fixture = struct.pack('<I', len(faces))
    for face, corners in zip(faces, uv):
        for index, corner in reversed(list(zip(face, corners))):
            position = [v * 10 for v in candidate['vertices'][index]]
            texcoord = struct.unpack('<2f', pack(*corner))
            fixture += struct.pack('<5f', *position, *texcoord)
    (ROOT / 'artifacts/geomod-holey01-csg.bin').write_bytes(fixture)
    print('PASS: original factory submits identical 10 vertices, 16 faces and 48 corner UV pairs; bounds enclose vertices; radius', bounds[6])


if __name__ == '__main__':
    run()
