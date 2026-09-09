"""Check original ground-query preparation against captured XEMU actor poses.

Executes RF.exe instructions and their callees without replacements. Stops before
the world query: this does not establish grounding, landing, or movement mode.
"""
import argparse
import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW


def words(*values):
    return struct.pack('<%dI' % len(values), *values)


def floats(*values):
    return struct.pack('<%df' % len(values), *values)


def f32(value):
    return struct.unpack('<f', floats(value))[0]


def run(snapshot):
    exe = ROOT / 'Installed_Game/RF.exe'
    digest = hashlib.sha256(exe.read_bytes()).hexdigest()
    assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    image = pefile.PE(str(exe)).get_memory_mapped_image()
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0x400000, (len(image) + 4095) // 4096 * 4096)
    cpu.mem_write(0x400000, image)
    base = 0x30000000
    cpu.mem_map(base, 0x10000)
    stack = base + 0xe000
    sphere_bytes = words(*snapshot['spheres'])
    count = len(sphere_bytes) // 24
    assert count and len(sphere_bytes) == count * 24
    spheres = [struct.unpack_from('<6f', sphere_bytes, i * 24) for i in range(count)]
    selected = min(range(count), key=lambda i: spheres[i][1])
    state = snapshot['symbols']['scene_actor_body']['words']
    records = snapshot['symbols']['rf_scene_actor_render_frames']['words']
    assert len(records) == 64 * 5
    dt = f32(1 / 60)
    margin = struct.unpack('<f', cpu.mem_read(0x5894f4, 4))[0]
    falling_depth = struct.unpack('<f', cpu.mem_read(0x5893c4, 4))[0]
    checks = []
    pc_inputs = bytearray()
    pc_expected = bytearray()
    # Mode 3 is the observed original falling descriptor value. Mode 0 and
    # support-speed cases below are synthetic branch checks, not guest modes.
    guest_modes = snapshot['symbols'].get('rf_scene_actor_ground_modes', {}).get('words')
    scenarios = [(3, 0., 0.), (3, 2., 0.), (0, 0., 4.), (0, -2., 4.)]
    if guest_modes is not None:
        speed_word = snapshot['symbols']['rf_scene_actor_movement_values']['words'][0]
        scenarios.append((-1, 0., struct.unpack('<f', words(speed_word))[0]))
    for mode, support_y, speed in scenarios:
        for frame in range(64):
            active_mode = guest_modes[frame] if mode == -1 else mode
            position_words = records[frame * 5 + 2:frame * 5 + 5]
            position = struct.unpack('<3f', words(*position_words))
            cpu.mem_write(base, bytes(0x10000))
            cpu.mem_write(base + 0xf0, words(*position_words))
            cpu.mem_write(base + 0x184, words(count, count, base + 0x4000))
            cpu.mem_write(base + 0x4000, sphere_bytes)
            cpu.mem_write(base + 0x1ac, words(state[69]))
            cpu.mem_write(base + 0x858, words(base + 0x2000))
            cpu.mem_write(base + 0x2004, words(active_mode))
            cpu.mem_write(base + 0x294, words(base + 0x3000))
            cpu.mem_write(base + 0x3050, floats(speed))
            cpu.mem_write(base + 0x8a4, floats(support_y))
            cpu.mem_write(0x5a4014, floats(dt))
            # Already-constructed reusable body with reserved VArray capacity.
            # No allocator or gameplay callee is hooked or replaced.
            cpu.mem_write(0x7c7064, words(1))
            cpu.mem_write(0x7c6ed8, bytes(0x18c))
            cpu.mem_write(0x7c6fd4, words(0, 16, base + 0x5000))
            cpu.mem_write(stack, words(base + 0xf000, base))
            cpu.reg_write(UC_X86_REG_ESP, stack)
            cpu.reg_write(UC_X86_REG_FPCW, 0x37f)
            cpu.emu_start(0x4a0840, 0x4a0a57, count=100000)
            assert cpu.reg_read(UC_X86_REG_EIP) == 0x4a0a57
            sp = cpu.reg_read(UC_X86_REG_ESP)
            start_ptr, end_ptr, body_ptr, hit_ptr = struct.unpack('<4I', cpu.mem_read(sp, 16))
            assert body_ptr == 0x7c6ed8
            start = list(position)
            start[1] = f32(start[1] + margin)
            if support_y > 0:
                start[1] = f32(start[1] + dt * support_y)
            end = list(position)
            # Grounded depth remains in x87 extended precision through the
            # subtraction; only the final end coordinate is stored as float.
            depth = falling_depth if active_mode == 3 else dt * speed + margin
            end[1] = f32(end[1] - depth)
            assert bytes(cpu.mem_read(start_ptr, 12)) == floats(*start)
            assert bytes(cpu.mem_read(end_ptr, 12)) == floats(*end), (mode, frame, struct.unpack('<3f', cpu.mem_read(end_ptr, 12)), end)
            assert bytes(cpu.mem_read(0x7c6fd4, 12)) == words(1, 16, base + 0x5000)
            assert bytes(cpu.mem_read(base + 0x5000, 24)) == sphere_bytes[selected * 24:(selected + 1) * 24]
            radius = f32(abs(spheres[selected][1]) + spheres[selected][3])
            assert bytes(cpu.mem_read(0x7c6fd0, 4)) == floats(radius)
            assert bytes(cpu.mem_read(0x7c6f4c, 36)) == floats(1, 0, 0, 0, 1, 0, 0, 0, 1)
            assert bytes(cpu.mem_read(0x7c6ffc, 4)) == words(state[69] & ~0x1000)
            lower = [f32(min(a, b) - radius) for a, b in zip(start, end)]
            upper = [f32(max(a, b) + radius) for a, b in zip(start, end)]
            assert bytes(cpu.mem_read(0x7c6fe0, 24)) == floats(*lower, *upper)
            assert bytes(cpu.mem_read(hit_ptr + 24, 4)) == floats(1)
            assert count == 3, 'PC probe currently expects the three miner spheres'
            pc_inputs += sphere_bytes + words(*position_words, state[69], int(active_mode == 3)) + floats(dt, speed, support_y)
            query_flags = (state[69] & ~0x1000) | 4 | (0x100 if radius < margin else 0)
            pc_expected += (bytes(cpu.mem_read(start_ptr, 12)) + bytes(cpu.mem_read(end_ptr, 12))
                            + bytes(cpu.mem_read(base + 0x5000, 24)) + floats(radius)
                            + bytes(cpu.mem_read(0x7c6fe0, 24)) + words(selected, query_flags))
            checks.append(dict(frame=frame, mode=active_mode, support_y=support_y,
                               start=start, end=end, lower=lower, upper=upper))
    pc = subprocess.check_output([str(ROOT / 'build/pc/Release/rf_physics_probe.exe'), '--ground'], input=pc_inputs)
    assert pc == pc_expected, 'Shared C ground preparation differs from original'
    guest_ground = snapshot['symbols'].get('rf_scene_actor_ground_records', {}).get('words')
    if guest_ground is not None:
        assert len(guest_ground) == 2112
        offset = 256 * 84 if guest_modes is not None else 0
        for frame in range(64):
            assert words(*guest_ground[frame * 33:frame * 33 + 21]) == pc_expected[offset + frame * 84:offset + (frame + 1) * 84], 'Guest probe differs from original'
    return dict(status='PASS', executable_sha256=digest, checks=len(checks), pc_matches_original=True, guest_preparation_matches_original=guest_ground is not None,
                scope='Original 4a0840 entry through 4a0a57, before world query; no callee replacements. Guest sphere/position inputs with synthetic movement descriptors and support velocities.',
                selected_sphere=selected, selected_sphere_values=spheres[selected],
                margin=margin, falling_depth=falling_depth,
                collision_flags=state[69] & ~0x1000, cases=checks)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('snapshot', type=Path)
    parser.add_argument('--out', type=Path, default=ROOT / 'artifacts/actor-ground-original.json')
    args = parser.parse_args()
    report = run(json.loads(args.snapshot.read_text()))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'cases'}, indent=2))
