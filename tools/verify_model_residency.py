"""Check per-model texture slots, material bytes and Win32 allocation accounting."""
import json, struct, subprocess
from pathlib import Path
from inspect_models import inspect

root = Path(__file__).resolve().parents[1]
archives = []
entries = {}
models = []
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
    if not archive.get('vpp'):
        continue
    path = root/'Installed_Game'/archive['path']
    archives.append(path)
    for entry in archive['vpp']['entries']:
        entries.setdefault(entry['name'].lower(), (path, entry))
        if entry['name'].lower().endswith('.v3c'):
            models.append((path, entry))

def read(path, entry):
    with path.open('rb') as stream:
        stream.seek(entry['offset'])
        return stream.read(entry['size'])

results = []
for path, entry in models:
    raw = read(path, entry)
    records = []
    for section in inspect(raw)['sections']:
        if section['type'] == '0x5355424d':
            start = section['material_offset']
            records.extend(raw[start+i*84:start+(i+1)*84] for i in range(section['materials']))
    names = []
    pairs = []
    for record in records:
        pair = []
        for offset in (0, 48):
            name = record[offset:offset+32].split(b'\0')[0].decode('ascii').lower()
            if name and name not in names:
                names.append(name)
            pair.append(names.index(name) if name else -1)
        pairs.append(pair)
    selected = [a for a in archives if a in {entries[n][0] for n in names}]
    images = [read(*entries[n]) for n in names]
    pixels = sum(struct.unpack_from('<H', im, 12)[0]*struct.unpack_from('<H', im, 14)[0]*4 for im in images)
    resident = 36 + len(records)*236 + len(names)*28 + pixels
    peak = resident + len(records)*100
    args = [str(root/'build/pc/Release/rf_material_probe.exe'), '--model', str(path), entry['name'], str(peak), *map(str, selected)]
    run = subprocess.run(args, capture_output=True, text=True)
    assert run.returncode == 0, (entry['name'], run.stdout, run.stderr)
    lines = run.stdout.splitlines()
    assert list(map(int, lines[0].split())) == [len(records), len(names), resident, peak]
    for record, pair, line in zip(records, pairs, lines[1:], strict=True):
        dst = bytearray(200)
        struct.pack_into('<i', dst, 0x44, -1)
        dst[9:13] = b'\xff'*4
        struct.pack_into('<I', dst, 0x78, 15)
        flags, = struct.unpack_from('<I', record, 80)
        transparent = images[pair[0]][16] == 32
        struct.pack_into('<I', dst, 4, 1 | (8 if transparent or flags & 2 else 0) | (16 if flags & 1 else 0))
        dst[8] = bool(flags & 2)
        for source, target in ((0, 0x14), (48, 0x90)):
            name = record[source:source+32].split(b'\0')[0]+b'\0'
            dst[target:target+len(name)] = name
        struct.pack_into('<i', dst, 0x10, pair[0])
        struct.pack_into('<i', dst, 0xb4, pair[1])
        struct.pack_into('<I', dst, 0xb8, 1)
        dst[0x84:0x90] = record[36:48]
        assert line == f'M {dst.hex()} {struct.unpack_from("<I", record, 32)[0]}', entry['name']
    low = args.copy()
    low[4] = str(peak-1)
    fail = subprocess.run(low, capture_output=True, text=True)
    assert fail.returncode == 1 and fail.stdout.strip() == '-4', (entry['name'], fail.stdout)
    missing = subprocess.run(args[:5]+[str(path)], capture_output=True, text=True)
    assert missing.returncode == 1 and missing.stdout.strip() == '-3', (entry['name'], missing.stdout)
    results.append(dict(name=entry['name'], materials=len(records), textures=len(names), resident_bytes=resident, peak_bytes=peak))

report = dict(result='PASS', models=len(results), materials=sum(r['materials'] for r in results),
              budget_rejections=len(results), missing_texture_rejections=len(results),
              largest=max(results, key=lambda r:r['peak_bytes']),
              scope='Port-owned material bundles with actual installed textures; byte mapping, local slots and exact Win32 budget checked; not original allocator equivalence or rendering',
              details=results)
(root/'artifacts/model-residency-verification.json').write_text(json.dumps(report, indent=2))
print({k:v for k,v in report.items() if k != 'details'})
