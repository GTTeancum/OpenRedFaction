"""Compare bone normalization/local transforms with original unhooked x86."""
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
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_FPCW
exe = root / 'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest() == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image = pefile.PE(str(exe)).get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image) + 4095) // 4096 * 4096); u.mem_write(0x400000, image)
data, stack, stop = 0x30000000, 0x31000000, 0x32000000
for address in (data, stack, stop): u.mem_map(address, 4096)
inventory = json.loads((root / 'artifacts/inventory.json').read_text())
entries = {(a['path'], e['name']): e for a in inventory['files'] for e in a.get('vpp', {}).get('entries', [])}
records = json.loads((root / 'artifacts/bone-tests/report.json').read_text())['records']
inputs = []
for record in records:
    entry = entries[record['archive'], record['model']]
    with (root / 'Installed_Game' / record['archive']).open('rb') as f:
        f.seek(entry['offset'] + record['offset'] + 12)
        bones = f.read(record['count'] * 56)
    assert len(bones) == record['count'] * 56
    for i in range(record['count']): inputs.append(bones[i * 56 + 24:i * 56 + 52])
asset_cases = len(inputs)
rng = random.Random(0x519720)
for i in range(400):
    scale = [1e-30, 1e30, 1.0][i % 3]
    inputs.append(struct.pack('<7f', *(rng.uniform(-1, 1) * scale for _ in range(4)),
                              *(rng.uniform(-1000, 1000) for _ in range(3))))
expected = []
for raw in inputs:
    u.mem_write(data, raw)
    u.reg_write(UC_X86_REG_FPCW, 0x37f)
    u.mem_write(stack + 4000, struct.pack('<I', stop))
    u.reg_write(UC_X86_REG_ESP, stack + 4000); u.reg_write(UC_X86_REG_ECX, data)
    u.emu_start(0x519720, stop, count=1000)
    assert u.reg_read(UC_X86_REG_EIP) == stop
    u.mem_write(stack + 4000, struct.pack('<3I', stop, data, data + 16))
    u.reg_write(UC_X86_REG_ESP, stack + 4000); u.reg_write(UC_X86_REG_ECX, data + 128)
    u.emu_start(0x4fe900, stop, count=1000)
    assert u.reg_read(UC_X86_REG_EIP) == stop
    expected.append(bytes(u.mem_read(data + 128, 48)))
run = subprocess.run([str(root / 'build/pc/Release/rf_transform_probe.exe')],
                     input=b''.join(inputs), capture_output=True, check=True)
assert len(run.stdout) == len(inputs) * 52
maximum = 0; exact = 0
for i, want in enumerate(expected):
    status, = struct.unpack_from('<i', run.stdout, i * 52)
    actual = run.stdout[i * 52 + 4:(i + 1) * 52]
    assert status == 0, i
    exact += actual == want
    error = max(abs(a - b) for a, b in zip(struct.unpack('<12f', actual), struct.unpack('<12f', want)))
    maximum = max(maximum, error)
    assert actual == want, (i, error)
report = dict(result='PASS', asset_cases=asset_cases, synthetic_cases=400, exact_cases=exact,
              maximum_absolute_error=maximum, tolerance=0,
              scope='Unhooked 0x519720 normalization and 0x4fe900 local transform; no parent composition or animation')
(root / 'artifacts/transform-verification.json').write_text(json.dumps(report, indent=2))
print(report)
