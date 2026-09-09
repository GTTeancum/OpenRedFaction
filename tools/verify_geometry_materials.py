"""Check shared world/mover slots and owned image bytes after source closure."""
import json
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
probe = root / 'build/pc/Release/rf_material_probe.exe'
levels = json.loads((root / 'artifacts/movers.json').read_text())['results']


def check(level, archives):
    lines = subprocess.check_output([
        str(probe), '--geometry', str(root / 'Installed_Game' / level['archive']),
        level['file'], str(32 * 1024 * 1024), *map(str, archives)
    ], text=True).splitlines()
    header = list(map(int, lines[0].split()[1:]))
    unique, slots, textures = [], {}, []
    references = 0
    for line in lines[1:]:
        if line.startswith('N '):
            _, geometry, local, slot, name = line.split(' ', 4)
            key = name.lower()
            if key not in slots:
                slots[key] = len(unique)
                unique.append(name)
            assert int(slot) == slots[key], line
            references += 1
        else:
            textures.append(list(map(int, line.split()[1:])))
    assert header[0] == level['count'] + 1 and header[1] == len(unique)
    assert len(textures) == len(unique)
    if archives:
        named = subprocess.check_output([
            str(probe), '--named', str(32 * 1024 * 1024), *map(str, archives)
        ], input='\n'.join(unique) + '\n', text=True).splitlines()
        for index, line in enumerate(named[1:]):
            # Names can contain spaces; numeric fields are always the final seven.
            fields = list(map(int, line.rsplit(' ', 7)[1:]))
            assert textures[index] == [index, *fields[:5]], (textures[index], fields)
        assert list(map(int, named[0].split()))[:3] == header[1:4]
    else:
        assert header[2] == 0 and header[3] == len(unique)
        assert all(row[2:] == [4294967295, 0, 0, 2166136261] for row in textures)
    return dict(file=level['file'], geometries=header[0], references=references,
                unique=header[1], loaded=header[2], missing=header[3],
                resident_bytes=header[4], peak_bytes=header[5])


results = [check(level, []) for level in levels]
archives = [root / 'Installed_Game' / f'maps{name}.vpp' for name in ('1', '2', '3', '4', '_en')]
live = check(next(level for level in levels if level['file'] == 'L1S1.rfl'), archives)
report = dict(result='PASS', levels=len(results),
              references=sum(item['references'] for item in results), live_mines=live,
              scope='PC: all inventoried mover levels plus world materials, case-insensitive first-use slots; '
                    'missing-image sharing with no archives; Live Mines decoded bytes/status/archive selection '
                    'against ordinary named loader; exact/one-byte-short peak budgets and repeated close. '
                    'Sources and archives closed before image checks. Not scene or Xbox runtime integration.',
              results=results)
(root / 'artifacts/geometry-materials-verification.json').write_text(json.dumps(report, indent=2))
print({key: value for key, value in report.items() if key != 'results'})
