"""Check the actual NXDK support transform, without a game/desktop session."""
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


def main():
    binary = ROOT / "build/xbox/main.exe"
    pe = pefile.PE(str(binary))
    blob = pe.get_memory_mapped_image()
    origin = pe.OPTIONAL_HEADER.ImageBase
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(origin, (len(blob) + 4095) // 4096 * 4096)
    cpu.mem_write(origin, blob)
    cpu.mem_map(BASE, 0x10000)
    mapping = (ROOT / "build/xbox/main.map").read_text()
    entry = int(re.search(r"_rf_scene_npc_support_interval_target\s+([0-9a-fA-F]+)", mapping)[1], 16)
    cases = [
        # name, start, end, old basis, new basis, point, duration, flags, target
        ("quarter turn", (0, 0, 0), (0, 0, 0), IDENTITY, YAW90, (2, 1, 0), 0.5, 2, (0, 1, -2)),
        ("translated pivot", (10, 20, 30), (40, 50, 60), IDENTITY, YAW90, (12, 21, 30), 0.5, 3, (40, 51, 58)),
        ("reverse orientation", (10, 20, 30), (40, 50, 60), YAW90, IDENTITY, (10, 21, 28), 0.5, 3, (42, 51, 60)),
        ("pivot point", (10, 20, 30), (40, 50, 60), IDENTITY, YAW90, (10, 20, 30), 0.5, 3, (40, 50, 60)),
        ("translation", (10, 20, 30), (11, 18, 33), IDENTITY, IDENTITY, (12, 21, 30), 0.5, 1, (13, 19, 33)),
        ("unchanged", (10, 20, 30), (10, 20, 30), YAW90, YAW90, (12, 21, 30), 0.5, 0, (12, 21, 30)),
        ("invalid duration", (0, 0, 0), (0, 0, 0), IDENTITY, YAW90, (2, 1, 0), 0, 2, None),
        ("invalid point", (0, 0, 0), (0, 0, 0), IDENTITY, YAW90, (float("nan"), 1, 0), 0.5, 2, None),
    ]
    results = []
    for name, start, end, old, new, point, seconds, changed, expected in cases:
        cpu.mem_write(BASE, struct.pack("<24f2I", *start, *end, *old, *new, 123, changed))
        cpu.mem_write(BASE + 0x100, struct.pack("<3f", *point))
        sentinel = struct.pack("<6f", *([-123] * 6))
        cpu.mem_write(BASE + 0x200, sentinel)
        duration_bits = struct.unpack("<I", struct.pack("<f", seconds))[0]
        cpu.mem_write(STACK, struct.pack("<6I", STOP, BASE, BASE + 0x100,
                                       duration_bits, BASE + 0x200, BASE + 0x20C))
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(entry, STOP, count=10000)
        assert cpu.reg_read(UC_X86_REG_EIP) == STOP, name
        status = cpu.reg_read(UC_X86_REG_EAX)
        output = bytes(cpu.mem_read(BASE + 0x200, 24))
        if expected is None:
            assert status != 0 and output == sentinel, name
            results.append({"name": name, "rejected_without_mutation": True})
        else:
            target, velocity = struct.unpack("<3f", output[:12]), struct.unpack("<3f", output[12:])
            assert status == 0 and target == expected, (name, status, target)
            expected_velocity = tuple((expected[i] - point[i]) / seconds for i in range(3))
            assert velocity == expected_velocity, (name, velocity, expected_velocity)
            results.append({"name": name, "target": target, "velocity": velocity})
    report = {"result": "PASS", "scope": "Compiled NXDK position/point-velocity math and error atomicity; no live collision or rendered behavior claim", "cases": results}
    path = ROOT / "artifacts/xbox-npc-rotating-support.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2) + "\n")
    print(f"PASS: {len(results)} compiled NXDK rotating-support cases")


if __name__ == "__main__":
    main()
