"""Compare the compiled C reader with independently collected disk-layout evidence."""
import argparse
import json
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--inventory', type=Path, default=Path('artifacts/inventory.json'))
parser.add_argument('--reader', type=Path, default=Path('build/pc/Release/rf_pc.exe'))
args = parser.parse_args()
inventory = json.loads(args.inventory.read_text(encoding='utf-8'))
reports = []
for record in inventory['files']:
    if 'vpp' not in record:
        continue
    result = subprocess.run([str(args.reader.resolve()), str(Path(inventory['root']) / record['path'])],
                            check=True, capture_output=True)
    lines = result.stdout.decode('cp1252').splitlines()[1:]
    actual = []
    for line in lines:
        offset, size, name = line.split(maxsplit=2)
        actual.append(dict(name=name, size=int(size), offset=int(offset)))
    if actual != record['vpp']['entries']:
        raise AssertionError(f'Directory mismatch: {record["path"]}')
    reports.append(dict(archive=record['path'], entries=len(actual), result='PASS'))
Path('artifacts/archive-verification.json').write_text(json.dumps(reports, indent=2), encoding='utf-8')
print(f'PASS: {len(reports)} archives, {sum(r["entries"] for r in reports)} entries')
