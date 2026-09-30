"""Two compiled-NXDK carry/loss/fall/contact timelines; no PC game launch."""
import ctypes as ct
import json
from pathlib import Path
import re
import struct
import sys

import pefile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "local/python"))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP

BASE = 0x30000000
STACK, STOP = BASE + 0xE000, BASE + 0xF000
IDENTITY = (1, 0, 0, 0, 1, 0, 0, 0, 1)
YAW90 = (0, 0, -1, 0, 1, 0, 1, 0, 0)


class Bounds(ct.Structure):
    _fields_ = [("radius", ct.c_float), ("minimum", ct.c_float * 3), ("maximum", ct.c_float * 3)]


class Body(ct.Structure):
    # Mirror the public rf_physics_body_state declaration, not original RF offsets.
    _fields_ = [("coefficients", ct.c_float * 3), ("mass", ct.c_float),
                ("local_tensor", ct.c_float * 9), ("world_tensor", ct.c_float * 9),
                ("position", ct.c_float * 3), ("next_position", ct.c_float * 3),
                ("orientation", ct.c_float * 9), ("next_orientation", ct.c_float * 9),
                ("velocity", ct.c_float * 3), ("vector_c8", ct.c_float * 3),
                ("mass_vector_d4", ct.c_float * 3), ("vector_e0", ct.c_float * 3),
                ("vector_ec", ct.c_float * 3), ("bounds", Bounds),
                ("flags", ct.c_uint32), ("state_124", ct.c_uint32),
                ("vector_138", ct.c_float * 3), ("scalar_144", ct.c_float),
                ("reference_15c", ct.c_int32), ("word_164", ct.c_uint32), ("word_168", ct.c_uint32)]


def bits(value):
    return struct.unpack("<I", struct.pack("<f", value))[0]


def main():
    pe = pefile.PE(str(ROOT / "build/xbox/main.exe"))
    blob = pe.get_memory_mapped_image()
    origin = pe.OPTIONAL_HEADER.ImageBase
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(origin, (len(blob) + 4095) // 4096 * 4096)
    cpu.mem_write(origin, blob)
    cpu.mem_map(BASE, 0x10000)
    mapping = (ROOT / "build/xbox/main.map").read_text()

    def call(name, *arguments, returns_status=True):
        entry = int(re.search(r"_" + name + r"\s+([0-9a-fA-F]+)", mapping)[1], 16)
        cpu.mem_write(STACK, struct.pack("<" + "I" * (len(arguments) + 1), STOP, *arguments))
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(entry, STOP, count=10000)
        assert cpu.reg_read(UC_X86_REG_EIP) == STOP, name
        if returns_status:
            assert cpu.reg_read(UC_X86_REG_EAX) == 0, (name, cpu.reg_read(UC_X86_REG_EAX))

    def floats(address):
        return struct.unpack("<3f", cpu.mem_read(address, 12))

    rows = []
    for name, end, new_basis, changed, expected_target, inherited in (
        ("rotation", (0, 0, 0), YAW90, 2, (0, 1, -2), (-4, 0, -4)),
        ("translation", (1, 0, 2), IDENTITY, 1, (3, 1, 2), (2, 0, 4)),
    ):
        interval, point, target, cache = BASE, BASE + 0x100, BASE + 0x120, BASE + 0x140
        support, body_at, object_flags = BASE + 0x160, BASE + 0x400, BASE + 0x180
        cpu.mem_write(interval, struct.pack("<24f2I", 0, 0, 0, *end, *IDENTITY, *new_basis, 123, changed))
        cpu.mem_write(point, struct.pack("<3f", 2, 1, 0))
        call("rf_scene_npc_support_interval_target", interval, point, bits(0.5), target, cache)
        assert floats(target) == expected_target and floats(cache) == inherited, name
        body = Body()
        body.mass = 1
        body.position[:] = expected_target
        body.next_position[:] = expected_target
        body.flags = 0x400000
        cpu.mem_write(body_at, bytes(body))
        cpu.mem_write(support, struct.pack("<Ii", 123, 5))
        cpu.mem_write(object_flags, bytes(4))
        call("rf_scene_npc_support_loss", support, cache, body_at + Body.flags.offset)
        assert struct.unpack("<Ii", cpu.mem_read(support, 8)) == (0, -1), name
        assert floats(cache) == inherited
        # Repeated loss cannot double the inherited contribution or erase it.
        call("rf_scene_npc_support_loss", support, cache, body_at + Body.flags.offset)
        history = []
        for tick in range(1, 4):
            # Released handle resolves to NULL. Actual refresh must retain launch velocity.
            call("rf_physics_support_refresh", 3, 0, cache,
                 body_at + Body.flags.offset, object_flags, returns_status=False)
            call("rf_physics_fall_propose", body_at, bits(0.5), bits(8), cache)
            body = Body.from_buffer_copy(bytes(cpu.mem_read(body_at, ct.sizeof(Body))))
            expected = (expected_target[0] + inherited[0] * 0.5 * tick,
                        expected_target[1] - tick * tick,
                        expected_target[2] + inherited[2] * 0.5 * tick)
            assert tuple(body.next_position) == expected, (name, tick, tuple(body.next_position), expected)
            assert tuple(body.velocity) == (0, -4 * tick, 0), (name, tick, "accumulated carry")
            assert floats(cache) == inherited
            history.append(list(body.next_position))
            body.position[:] = body.next_position
            cpu.mem_write(body_at, bytes(body))
        # Static contact clears launch carry. Ordinary normal-mode landing clears vertical velocity.
        call("rf_scene_npc_support_contact_velocity", cache, 0, 0)
        assert floats(cache) == (0, 0, 0)
        body.velocity[1] = 0
        cpu.mem_write(body_at, bytes(body))
        call("rf_physics_fall_propose", body_at, bits(0.5), bits(8), cache)
        landed = Body.from_buffer_copy(bytes(cpu.mem_read(body_at, ct.sizeof(Body))))
        assert landed.next_position[0] == body.position[0] and landed.next_position[2] == body.position[2]
        # Boarding another moving support replaces the old contribution instead of adding it.
        cpu.mem_write(point, struct.pack("<3f", 3, 0.5, -2))
        call("rf_scene_npc_support_contact_velocity", cache, point, 1)
        assert floats(cache) == (3, 0.5, -2)
        rows.append({"name": name, "carry_target": expected_target, "inherited": inherited,
                     "fall_positions": history, "static_reset": True, "new_support_replaces": True})
    report = {"result": "PASS", "scope": "Actual compiled NXDK transform, support release/refresh, three fall steps and contact-cache replacement; collision admission and live NPC integration not executed", "timelines": rows}
    path = ROOT / "artifacts/xbox-npc-support-lifecycle.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2) + "\n")
    print("PASS: 2 compiled NXDK support-loss timelines")


if __name__ == "__main__":
    main()
