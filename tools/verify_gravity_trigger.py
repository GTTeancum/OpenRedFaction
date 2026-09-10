"""Execute original auto-trigger -> handle lookup -> gravity event chain.

Registrations and runtime fields are fixture inputs, not a recovered loader.
No calls in the action chain are intercepted.
"""
import hashlib
import json
import struct
import sys
from pathlib import Path
import pefile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW

exe = root / 'Installed_Game/RF.exe'
sha = hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe = pefile.PE(str(exe))
image = pe.get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image) + 4095) // 4096 * 4096)
u.mem_write(0x400000, image)
base = 0x30000000
u.mem_map(base, 65536)
trigger, event, array = base, base + 0x1000, base + 0x2000
stack, stop = base + 0xe000, base + 0xf000


def write(address, *values):
    u.mem_write(address, struct.pack('<' + 'I' * len(values), *(v & 0xffffffff for v in values)))


def read(address):
    return struct.unpack('<I', u.mem_read(address, 4))[0]


events = json.loads((root / 'artifacts/events.json').read_text())['results']
triggers = json.loads((root / 'artifacts/triggers.json').read_text())['results']
results = []
for level in events:
    for record in level['records']:
        if record['type_index'] != 44:
            continue
        source_level = next(l for l in triggers if l['file'] == level['file'])
        sources = [r for r in source_level['records'] if record['uid'] in r['links']]
        assert len(sources) == 1 and sources[0]['name'] == 'Trigger Auto'
        source = sources[0]
        assert source['links'] == [record['uid']] and source['flags'] == [0, 0, 0, 1, 0]
        assert not record['links'] and record['delay'] == 0
        value = struct.unpack('<I', struct.pack('<f', record['values'][0]))[0]
        for disabled in (False, True):
            u.mem_write(base, bytes(0x3000))
            write(0x856844, 0)
            write(0x8567ac, trigger)
            write(trigger + 0x28c, 0x856520)
            write(trigger + 0x2c, 0x23450001)
            write(trigger + 0x298, -1, 30000, 0, -1, 0, -1)
            write(trigger + 0x2b0, 8 | (16 if disabled else 0))
            write(trigger + 0x2d4, 1, 1, array)
            handle = 0x12340000
            write(array, handle)
            # Event's object base is +4 relative to the derived event base.
            write(0x7394cc, event + 4)
            write(event, 0x589b3c)
            write(event + 0x24, record['uid'], 6)
            write(event + 0x30, handle)
            write(event + 0x290, 44, 0, -1, 0, 0, 0)
            write(event + 0x2b8, value)
            u.mem_write(0x64ecb9, b'\0\0')
            write(0x5a3ed8, 12345)
            write(0x6460f0, 0x41400000)
            write(0x5a00dc, 0x411ccccd)
            write(0x7c7058, 0, 0xc11ccccd, 0)
            write(0x62f2c8, 0x40a362be)
            write(stack, stop)
            u.reg_write(UC_X86_REG_ESP, stack)
            u.reg_write(UC_X86_REG_FPCW, 0x37f)
            u.emu_start(0x4c01b0, stop, count=100000)
            assert u.reg_read(UC_X86_REG_EIP) == stop
            assert u.reg_read(UC_X86_REG_ESP) == stack + 4
            assert read(0x5a00dc) == (0x411ccccd if disabled else value)
            assert read(0x7c705c) == ((0x411ccccd if disabled else value) ^ 0x80000000)
            assert read(0x62f2c8) == 0x40a362be
            assert read(trigger + 0x2a0) == (0 if disabled else 1)
            assert read(trigger + 0x298) == (0xffffffff if disabled else 42345)
            assert read(trigger + 0x2a8) == (0 if disabled else 0x41400000)
            assert read(trigger + 0x2b0) == (24 if disabled else 72)
            assert read(event + 0x2a8) == (0 if disabled else 0xffffffff)
            assert read(event + 0x2ac) == (0 if disabled else 0x23450001)
            results.append(dict(level=level['file'], trigger_uid=source['uid'],
                                event_uid=record['uid'], gravity=record['values'][0],
                                disabled=disabled))
report = dict(result='PASS', cases=len(results), original_sha256=sha, results=results,
              scope='Unmodified 4c01b0 auto sweep, 4c0220 activation, 4c0320 links, 4b6760/4b6800 handle resolution, common event activation, virtual gravity on, empty-link propagation and trigger timer. Synthetic registrations/runtime initialization; loader, sweep call timing, full campaign and C/NXDK chain equivalence excluded.')
(root / 'artifacts/gravity-trigger-verification.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
