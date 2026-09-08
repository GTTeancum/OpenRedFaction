"""Compare reconstructed depth ordering with original 0x51cb50, no hooks."""
import hashlib
import json
import random
import struct
import subprocess
import sys
from pathlib import Path
import pefile
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP
exe = root / 'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest() == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image = pefile.PE(str(exe)).get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image) + 4095) // 4096 * 4096); u.mem_write(0x400000, image)
data, stack, stop = 0x30000000, 0x31000000, 0x32000000
u.mem_map(data, 65536); u.mem_map(stack, 4096); u.mem_map(stop, 4096)
inventory = json.loads((root / 'artifacts/inventory.json').read_text())
entries = {(a['path'], e['name']): e for a in inventory['files'] for e in a.get('vpp', {}).get('entries', [])}
records = json.loads((root / 'artifacts/bone-tests/report.json').read_text())['records']
fixtures = []
for r in records:
    e = entries[r['archive'], r['model']]
    with (root / 'Installed_Game' / r['archive']).open('rb') as f:
        f.seek(e['offset'] + r['offset'] + 12); raw = f.read(r['count'] * 56)
    fixtures.append([struct.unpack_from('<i', raw, i * 56 + 52)[0] for i in range(r['count'])])
asset_cases = len(fixtures)
fixtures.extend([[], [-1], [1, -1], [-1, -1, 0, 1], list(range(-1, 255))])
rng = random.Random(0x51cb50)
for _ in range(100):
    count = rng.randrange(1, 65)
    parents = [rng.randrange(-1, i) for i in range(count)]
    permutation = list(range(count)); rng.shuffle(permutation)
    shuffled = [-1] * count
    for i, parent in enumerate(parents):
        shuffled[permutation[i]] = -1 if parent == -1 else permutation[parent]
    fixtures.append(shuffled)
inputs, expected = [], []
for parents in fixtures:
    count = len(parents)
    for i, parent in enumerate(parents): u.mem_write(data + i * 76 + 72, struct.pack('<i', parent))
    u.mem_write(stack + 4000, struct.pack('<4I', stop, data + 32768, count, data))
    u.reg_write(UC_X86_REG_ESP, stack + 4000)
    u.emu_start(0x51cb50, stop, count=200000000)
    assert u.reg_read(UC_X86_REG_EIP) == stop
    expected.append(bytes(u.mem_read(data + 32768, count)) if count else b'')
    inputs.append(struct.pack('<I', count) + struct.pack('<' + 'i' * count, *parents))
run = subprocess.run([str(root / 'build/pc/Release/rf_order_probe.exe')],
                     input=b''.join(inputs), capture_output=True, check=True)
offset = 0
for want in expected:
    status, = struct.unpack_from('<i', run.stdout, offset); offset += 4
    actual = run.stdout[offset:offset + len(want)]; offset += len(want)
    assert status == 0 and actual == want, (status, actual, want)
assert offset == len(run.stdout)
report = dict(result='PASS', asset_skeletons=asset_cases, cases=len(fixtures), original='0x51cb50 + 0x51cba0, unhooked',
              scope='Stable depth order only; animation and pose evaluation not covered')
(root / 'artifacts/bone-order-verification.json').write_text(json.dumps(report, indent=2))
print(report)
