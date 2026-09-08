"""Check shared C directory and spawn parsing against all original RFL files."""
import json
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
inventory = json.loads((root / 'artifacts/inventory.json').read_text())
levels = json.loads((root / 'artifacts/levels.json').read_text())
reader = root / 'build/pc/Release/rf_pc.exe'
reports = []
for level in levels:
    archive_path = Path(inventory['root']) / level['archive']
    completed = subprocess.run([str(reader), str(archive_path), level['file']], capture_output=True, check=True)
    lines = completed.stdout.decode('cp1252').splitlines()
    fields = lines[0].split(maxsplit=4)
    assert fields == ['RFL', str(level['version']), str(level['bytes']), str(level['declared_sections']), level['name']]
    sections = [dict(type=hex(int(line.split()[1], 16)), offset=int(line.split()[2]), size=int(line.split()[3])) for line in lines[5:]]
    expected = [{k: s[k] for k in ('type', 'offset', 'size')} for s in level['sections'] if s['type'] != '0x0']
    assert sections == expected, level['file']
    archive = next(f for f in inventory['files'] if f['path'] == level['archive'])
    entry = next(e for e in archive['vpp']['entries'] if e['name'] == level['file'])
    with archive_path.open('rb') as stream:
        stream.seek(entry['offset'] + level['player_start_offset'] + 8)
        raw = stream.read(48)
    actual = [float(v) for line in lines[1:5] for v in line.split()[1:]]
    # Convert decimal diagnostics back to binary32 and compare bit-for-bit.
    expected_spawn = raw[:12] + raw[24:36] + raw[36:48] + raw[12:24]
    assert struct.pack('<12f', *actual) == expected_spawn, level['file']
    reports.append(dict(file=level['file'], sections=len(sections), result='PASS'))
(root / 'artifacts/level-verification.json').write_text(json.dumps(reports, indent=2))
print(f'PASS: {len(reports)} level directories and bit-exact spawn transforms')
