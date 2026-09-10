"""Execute original player release branch with explicit collision/ground boundaries.

The world query result is supplied; collision geometry is not emulated here.
Owner lookup, sphere copying, caller commit and speed setting execute unchanged.
"""
import hashlib
import itertools
import json
from pathlib import Path
import random
import struct
import sys

import pefile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root/'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_FPCW

exe = root/'Installed_Game/RF.exe'
sha = hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image = pefile.PE(str(exe)).get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image)+4095)//4096*4096)
u.mem_write(0x400000, image)
base = 0x30000000
u.mem_map(base, 65536)
entity, cls, spheres, player, decoy, stack = [base+n for n in (0, 0x2000, 0x4000, 0x6000, 0x8000, 0xe000)]
w = lambda *values: struct.pack('<'+'I'*len(values), *[v & 0xffffffff for v in values])
f = lambda *values: struct.pack('<'+'f'*len(values), *values)
read = lambda address: struct.unpack('<I', u.mem_read(address, 4))[0]
rng = random.Random(0x430df8)
events = []
blocked = 0

def boundary(cpu, address, size, data):
    sp = cpu.reg_read(UC_X86_REG_ESP)
    if address == 0x499ed0:
        assert read(sp+4) == entity+0x3c and read(sp+12) == entity+0x88
        endpoint = bytes(cpu.mem_read(read(sp+8), 12))
        # Query starts at (2,3,4); original adds class height and its own epsilon.
        epsilon = struct.unpack('<f', cpu.mem_read(0x5893c4, 4))[0]
        assert endpoint == f(2, 3+.625+epsilon, 4)
        events.append('query')
        cpu.reg_write(UC_X86_REG_EAX, blocked)
    elif address == 0x4a0840:
        assert read(sp+4) == entity
        assert read(entity+0x810) & 0x400 == 0
        events.append('ground')
    else:
        if address == 0x427450:
            assert read(sp+4) == 1
            events.append('speed')
        return
    # Supply just these two environmental boundaries, equivalent to cdecl ret.
    cpu.reg_write(UC_X86_REG_EIP, read(sp))
    cpu.reg_write(UC_X86_REG_ESP, sp+4)

u.hook_add(UC_HOOK_CODE, boundary)
results = []
for count, blocked, owner_kind, crouched in itertools.product(range(9), (0, 1), range(3), (0, 1, 255)):
    seed = bytearray(rng.randbytes(0x1500))
    flags = rng.getrandbits(32) | 0x400
    seed[0x2c:0x30] = w(0x10000)
    seed[0x3c:0x48] = f(2, 3, 4)
    seed[0x184:0x190] = w(count, 8, spheres)
    seed[0x294:0x298] = seed[0x29c:0x2a0] = w(cls)
    seed[0x75c:0x760] = w(-1)
    seed[0x810:0x814] = w(flags)
    u.mem_write(entity, bytes(seed))
    u.mem_write(cls, bytes(0x2000))
    u.mem_write(cls+0x50, f(3.5))
    u.mem_write(cls+0xf74, f(.625))
    initial = rng.randbytes(192)
    centers = [f(*[rng.uniform(-2, 2) for _ in range(3)]) for _ in range(8)]
    u.mem_write(spheres, initial)
    u.mem_write(cls+0xcec, w(count)+b''.join(bytes(24)+center+w(i) for i, center in enumerate(centers)))
    owner_seed = bytearray(rng.randbytes(0x1000))
    owner_seed[:4] = w(decoy)
    owner_seed[0x14:0x18] = w(0x10000)
    owner_seed[0xb1] = crouched
    u.mem_write(player, bytes(owner_seed))
    u.mem_write(decoy, bytes(0x1000))
    u.mem_write(decoy, w(player if owner_kind == 1 else decoy))
    u.mem_write(decoy+0x14, w(0x10001))
    u.mem_write(0x7c75cc, w(0 if owner_kind == 0 else decoy))
    u.mem_write(0x64ecb9, b'\0')
    events.clear()
    u.mem_write(stack, bytes(128))
    u.reg_write(UC_X86_REG_ESP, stack)
    u.reg_write(UC_X86_REG_EBX, player)
    u.reg_write(UC_X86_REG_ESI, entity)
    u.reg_write(UC_X86_REG_EAX, crouched)
    u.reg_write(UC_X86_REG_FPCW, 0x37f)
    # Resolved release branch, before the original test of player byte b1.
    u.emu_start(0x430df8, 0x430e19, count=10000)
    assert u.reg_read(UC_X86_REG_EIP) == 0x430e19
    assert u.reg_read(UC_X86_REG_ESP) == stack
    success = bool(crouched and not blocked)
    expected = bytearray(seed)
    wanted_spheres = bytearray(initial)
    if success:
        expected[0x810:0x814] = w(flags & ~0x400)
        expected[0x8c0:0x8c8] = f(3.5)+w(1)
        owner_seed[0xb1] = 0
        for i in range(count):
            wanted_spheres[i*24:i*24+12] = centers[i]
    assert bytes(u.mem_read(entity, len(seed))) == expected
    assert bytes(u.mem_read(player, len(owner_seed))) == owner_seed
    assert bytes(u.mem_read(spheres, 192)) == wanted_spheres
    assert events == ([] if not crouched else ['query'] if blocked else ['query', 'ground', 'speed'])
    results.append(dict(spheres=count, blocked=blocked, owner_kind=owner_kind,
                        initial_crouch_byte=crouched, success=success, events=list(events)))

report = dict(result='PASS', cases=len(results), original_sha256=sha,
              scope='Original 430df8..430e19 release branch and complete 428a60, with real owner lookup, sphere copy and speed setter. Collision query outcome is supplied; ground callback is observed and skipped. Does not verify world collision, ground refresh, outer ownership/input gates or live campaign traversal.',
              results=results)
(root/'artifacts/player-stand-release-reference.json').write_text(json.dumps(report, indent=2)+'\n')
print(f'PASS: {len(results)} original player release cases')
