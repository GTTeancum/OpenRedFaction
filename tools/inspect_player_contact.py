"""Execute original flag-80 contact response with explicit non-liquid fixtures.

All response callees execute unchanged. Stop at the damage call after observing
its impact argument; do not emulate damage, object ownership or a full collision.
"""
import hashlib
import json
import math
from pathlib import Path
import random
import struct
import sys

import pefile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root/'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW

exe = root/'Installed_Game/RF.exe'
digest = hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image = pefile.PE(str(exe)).get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image)+4095)//4096*4096)
u.mem_write(0x400000, image)
base, stack, stop = 0x30000000, 0x3000e000, 0x3000f000
u.mem_map(base, 65536)
pack = lambda values: struct.pack('<'+'f'*len(values), *values)
put = lambda address, *values: u.mem_write(address, struct.pack('<'+'I'*len(values), *values))
observed = set()
coverage = {}
impact = []
branches = {0x49d94c: 'player_response', 0x49da01: 'contact_normal_boost',
            0x49da68: 'outgoing_normal_retained', 0x49da81: 'normal_cleared',
            0x49daa1: 'incoming_normal_correction', 0x49daf5: 'free_tangent',
            0x49db01: 'direction_test', 0x49db5e: 'grounded_y_clamp',
            0x49db71: 'direction_tangent_added'}


def observe(cpu, address, size, data):
    if address in branches:
        observed.add(branches[address])
    if address == 0x49cd80:
        sp = cpu.reg_read(UC_X86_REG_ESP)
        assert struct.unpack('<I', cpu.mem_read(sp+4, 4))[0] == base
        impact.append(bytes(cpu.mem_read(sp+8, 4)))
        cpu.emu_stop()


u.hook_add(UC_HOOK_CODE, observe)
rng = random.Random(0x49d94c)
cases = []
for mode in (1, 3, 8):
    for index in range(256):
        normal = [rng.uniform(-1, 1) for _ in range(3)]
        length = math.sqrt(sum(v*v for v in normal))
        normal = [v/length for v in normal]
        velocity = [rng.uniform(-4, 4) for _ in range(3)]
        if index % 16 == 0:
            normal = [0, 1, 0]
            velocity = [0, -2 if index % 32 else 2, 0]
        auxiliary = [rng.uniform(-1, 1) for _ in range(3)]
        support = [rng.uniform(-.5, .5) for _ in range(3)] if index % 2 else [0]*3
        contact = [rng.uniform(-.5, .5) for _ in range(3)] if index % 3 else [0]*3
        direction = [rng.uniform(-1, 1) for _ in range(3)] if index % 4 else [0]*3
        yaw = rng.uniform(-math.pi, math.pi)
        orientation = [math.cos(yaw), 0, -math.sin(yaw), 0, 1, 0, math.sin(yaw), 0, math.cos(yaw)]
        command = pack(velocity+auxiliary+normal+support+contact+direction+orientation)
        u.mem_write(base, bytes(0x4000))
        put(base+0x858, base+0x2000)
        put(base+0x2004, mode)
        put(base+0x294, base+0x3000)
        put(base+0x1e4, 0xffffffff)
        put(base+0x1a8, 0x80)
        for offset, a, b in [(0x144, 0, 24), (0x1c0, 24, 36), (0x8a0, 36, 48),
                             (0x1d8, 48, 60), (0x714, 60, 72), (0xfc, 72, 108)]:
            u.mem_write(base+offset, command[a:b])
        before = bytes(u.mem_read(base, 0x4000))
        observed.clear()
        impact.clear()
        put(stack, stop, base)
        u.reg_write(UC_X86_REG_ESP, stack)
        u.reg_write(UC_X86_REG_FPCW, 0x37f)
        u.emu_start(0x49d7e0, stop, count=100000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x49cd80 and len(impact) == 1
        assert 'player_response' in observed
        after = bytes(u.mem_read(base, 0x4000))
        assert before[:0x144] == after[:0x144] and before[0x150:] == after[0x150:]
        output = after[0x144:0x15c]+impact[0]
        assert all(math.isfinite(v) for v in struct.unpack('<7f', output))
        for branch in observed:
            coverage[branch] = coverage.get(branch, 0)+1
        cases.append(dict(mode=mode, input_words=list(struct.unpack('<27I', command)),
                          output_words=list(struct.unpack('<7I', output)), branches=sorted(observed)))

assert set(coverage) == set(branches.values()), coverage
report = dict(result='PASS', original_sha256=digest, cases=len(cases), branch_coverage=coverage,
              scope='Original 49d7e0 and unmodified response callees, player physics flag 80, no contact object/liquid/inverse mass, non-rotating actor, modes 1/3/8, identity-up yaw matrices. Stops at damage entry. Only entity velocity changes; angular auxiliary is preserved. No shared C comparison or full player physics claim.',
              records=cases)
folder = root/'artifacts'
folder.mkdir(exist_ok=True)
(folder/'player-contact-reference.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps({key: value for key, value in report.items() if key != 'records'}, indent=2))
