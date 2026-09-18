"""Pack local RF.exe Driller factory geometry; never copy original data into Git.

Decompiler extraction is cross-checked against process-local x86 execution of
4e6d60, using the same container boundaries as inspect_geomod_template.py.
Original vector construction, name selection and bounds/radius math execute.
This does not launch the game or establish live GeoMod behavior. The bounded
STAR decoder supports both26/64-face assets.
"""
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "local/python"))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_ECX, UC_X86_REG_EAX, UC_X86_REG_FPCW

SHA = "b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836"
SOURCE = ROOT / "artifacts/analysis/rf_b8fb9ab4c9bf/4e6d60.c.txt"
COUNTS = {"single": (15, 26), "double": (34, 64)}


def extract(name):
    text = SOURCE.read_text()
    assert SHA in text
    begin = text.index("s_bit_driller_" + name)
    end = text.index("s_bit_driller_single" if name == "double" else "s_Holey_APC", begin + 1)
    text = text[begin:end]
    words = [[int(w, 16) for w in row] for row in re.findall(
        r"FUN_00409fe0\(local_\w+,(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+)\)", text)]
    faces, uv = [], []
    for block in text.split("FUN_004cfab0(local_3c);")[1:]:
        rows = re.findall(r"uVar6 = (0x[0-9a-f]+);\s*uVar3 = (0x[0-9a-f]+);\s*puVar4 = .*?"
                          r"FUN_0040a480\((0x[0-9a-f]+|\d+)\);\s*FUN_004e0140", block)
        assert len(rows) == 3
        faces.append([int(r[2], 0) for r in rows])
        uv.append([[int(r[1], 16), int(r[0], 16)] for r in rows])
    assert (len(words), len(faces)) == COUNTS[name]
    return words, faces, uv


def execute(name, image, image_base):
    vertices, faces, uv = [], [], []
    nv, nf = COUNTS[name]
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(image_base, (len(image) + 4095) & ~4095)
    cpu.mem_write(image_base, image)
    cpu.mem_map(0, 4096)
    base = 0x30000000
    cpu.mem_map(base, 0x100000)
    solid, table, vertex_base, face_base = base, base + 0x1000, base + 0x2000, base + 0x3000
    name_at, stack, stop = base + 0x18000, base + 0xe0000, base + 0xf0000
    pack = lambda *v: struct.pack("<" + "I" * len(v), *v)
    read = lambda a: struct.unpack("<I", cpu.mem_read(a, 4))[0]
    cpu.mem_write(name_at, ("bit_driller_" + name + ".v3d\0").encode())
    cpu.mem_write(stack, pack(stop, name_at))
    cpu.reg_write(UC_X86_REG_ESP, stack)
    cpu.reg_write(UC_X86_REG_FPCW, 0x37f)
    controlled = {0x573619, 0x4cf060, 0x4cfc80, 0x4cfab0, 0x40a480, 0x4e0140, 0x40a490}
    completed = False

    def hook(u, address, size, unused):
        nonlocal completed
        if address not in controlled:
            return
        sp = u.reg_read(UC_X86_REG_ESP)
        arg = lambda i: read(sp + 4 + i * 4)
        ecx = u.reg_read(UC_X86_REG_ECX)
        pop, result = 0, 0
        if address == 0x573619:
            if arg(0) == 0x1cc:
                assert len(vertices) == nv and len(faces) == nf
                completed = True
                u.emu_stop()
                return
            assert arg(0) == 0x378 and not vertices
            result = solid
        elif address == 0x4cf060:
            assert ecx == solid
            result = solid
        elif address == 0x4cfc80:
            assert ecx == solid and len(vertices) < nv
            vertices.append(list(struct.unpack("<3I", u.mem_read(arg(0), 12))))
            result = vertex_base + (len(vertices) - 1) * 16
            u.mem_write(result, pack(*vertices[-1]))
            pop = 4
        elif address == 0x40a490:
            assert ecx == solid + 0x78
            result = len(vertices)
        elif address == 0x4cfab0:
            assert ecx == solid and len(vertices) == nv and len(faces) < nf
            properties = list(struct.unpack("<6I", u.mem_read(arg(0), 24)))
            assert properties == [0x100, 0, 0xffffffff, 0xffff0000, 0xffffffff, 0]
            faces.append([])
            uv.append([])
            result = face_base + (len(faces) - 1) * 256
            pop = 4
        elif address == 0x40a480:
            assert ecx == solid + 0x78 and arg(0) < nv
            result = table + arg(0) * 4
            u.mem_write(result, pack(vertex_base + arg(0) * 16))
            pop = 4
        else:
            assert faces and ecx == face_base + (len(faces) - 1) * 256
            index, remainder = divmod(arg(0) - vertex_base, 16)
            assert remainder == 0 and 0 <= index < nv and len(faces[-1]) < 3
            assert arg(3) == arg(4) == 0
            faces[-1].append(index)
            uv[-1].append([arg(1), arg(2)])
            pop = 20
        u.reg_write(UC_X86_REG_EAX, result)
        u.reg_write(UC_X86_REG_EIP, read(sp))
        u.reg_write(UC_X86_REG_ESP, sp + 4 + pop)

    cpu.hook_add(UC_HOOK_CODE, hook)
    cpu.emu_start(0x4e6d60, stop, count=500000)
    assert completed, hex(cpu.reg_read(UC_X86_REG_EIP))
    bounds = list(struct.unpack("<10f", cpu.mem_read(solid + 0x48, 40)))
    return vertices, faces, uv, bounds


def run():
    exe = ROOT / "Installed_Game/RF.exe"
    assert hashlib.sha256(exe.read_bytes()).hexdigest() == SHA
    pe = pefile.PE(str(exe))
    image = pe.get_memory_mapped_image()
    output = ROOT / "build/data"
    output.mkdir(parents=True, exist_ok=True)
    reports = []
    for name in ("single", "double"):
        words, faces, uv = extract(name)
        actual_words, actual_faces, actual_uv, bounds = execute(name, image, pe.OPTIONAL_HEADER.ImageBase)
        assert (words, faces, uv) == (actual_words, actual_faces, actual_uv)
        vertices = [struct.unpack("<3f", struct.pack("<3I", *row)) for row in words]
        assert all(math.isfinite(v) for v in bounds) and bounds[6] > 0
        sub = lambda a, b: [x-y for x, y in zip(a, b)]
        dot = lambda a, b: sum(x*y for x, y in zip(a, b))
        cross = lambda a, b: [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]
        edges, volume, distances = {}, 0, []
        for vertex in vertices:
            assert all(bounds[k] <= vertex[k] <= bounds[k+3] for k in range(3))
            assert math.dist(vertex, bounds[7:10]) <= bounds[6] + 1e-5
        for face in faces:
            assert len(set(face)) == 3 and all(0 <= i < len(vertices) for i in face)
            a, b, c = [vertices[i] for i in face]
            normal = cross(sub(b, a), sub(c, a))
            length = math.sqrt(dot(normal, normal))
            assert length > 0
            distances.append(-dot(normal, a)/length)
            volume += dot(a, cross(b, c))/6
            for i, j in zip(face, face[1:]+face[:1]):
                edges.setdefault(tuple(sorted((i, j))), []).append((i, j))
        assert len(vertices)-len(edges)+len(faces) == 2
        assert all(len(e) == 2 and e[0] == e[1][::-1] for e in edges.values())
        assert volume < 0 and min(distances) > 0  # Origin is a strict star kernel.
        packed = struct.pack("<4sIIf3f", b"RFCT", 1, len(faces), bounds[6], 0, 0, 0)
        for face, corners in zip(faces, uv):
            for index, corner in reversed(list(zip(face, corners))):
                packed += struct.pack("<5I", *words[index], *corner)
        path = output / ("driller-" + name + ".bin")
        if not path.exists() or path.read_bytes() != packed:
            path.write_bytes(packed)
        report = dict(name=name, vertices=len(vertices), faces=len(faces), edges=len(edges), bytes=len(packed),
                      bounds_radius_center=bounds, signed_volume=volume, minimum_kernel_clearance=min(distances),
                      original_factory_bit_exact=True, current_core_face_limit=64,
                      request_radius=10 if name == "single" else 20,
                      master_flags=0x3e, effective_geometric_scale=1,
                      scale_evidence="467020 flag8 forces scale1 before/after regions; flag4 uses host basis",
                      core_decoder_expected="RF_OK for bounded STAR templates",
                      packed_sha256=hashlib.sha256(packed).hexdigest())
        reports.append(report)
        print(name, json.dumps(report))
    (ROOT / "artifacts/driller-templates.json").write_text(json.dumps(dict(exe_sha256=SHA,
        export_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(), shapes=reports), indent=2)+"\n")


if __name__ == "__main__":
    run()
