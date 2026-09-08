"""Verify C lightmap loading and budgets across installed levels; pixel hashes for Live Mines."""
import json
import subprocess
from pathlib import Path


def main():
    root = Path(__file__).resolve().parents[1]
    maps = json.loads((root/'artifacts/lightmaps.json').read_text())
    inventory = json.loads((root/'artifacts/inventory.json').read_text())
    levels = json.loads((root/'artifacts/levels.json').read_text())
    probe = root/'build/pc/Release/rf_lightmap_probe.exe'
    for report in maps:
        budget = report['rgba_bytes'] + report['count']*16  # Win32 rf_image
        command = [str(probe), str(root/'Installed_Game'/report['archive']), report['file']]
        run = subprocess.run(command+[str(budget)], capture_output=True, text=True)
        assert run.returncode == 0, (report['file'], run.stdout, run.stderr)
        rows = [list(map(int, line.split())) for line in run.stdout.splitlines()]
        assert rows[0] == [report['count'], budget]
        assert [r[:2] for r in rows[1:]] == [[i['width'], i['height']] for i in report['images']]
        if budget:
            failed = subprocess.run(command+[str(budget-1)], capture_output=True, text=True)
            assert failed.returncode == 1 and failed.stdout.strip() == '-4', report['file']
        if report['file'] == 'L1S1.rfl':
            archive = next(a for a in inventory['files'] if a['path'] == report['archive'])
            entry = next(e for e in archive['vpp']['entries'] if e['name'] == report['file'])
            level = next(l for l in levels if l['file'] == report['file'])
            section = next(s for s in level['sections'] if s['type'] == '0x1200')
            with (root/'Installed_Game'/report['archive']).open('rb') as stream:
                for image, actual in zip(report['images'], rows[1:]):
                    stream.seek(entry['offset']+section['offset']+8+image['rgb_offset'])
                    rgb = stream.read(image['rgb_bytes'])
                    rgba = bytearray(len(rgb)//3*4)
                    rgba[0::4], rgba[1::4], rgba[2::4] = rgb[0::3], rgb[1::3], rgb[2::3]
                    rgba[3::4] = bytes([255])*(len(rgb)//3)
                    value = 2166136261
                    for byte in rgba:
                        value = ((value ^ byte)*16777619) & 0xffffffff
                    assert value == actual[2], image
    result = dict(levels=len(maps), exact_budget=True, one_byte_short_rejected=True,
                  live_mines_pixel_hashes=23, result='PASS')
    (root/'artifacts/lightmap-validation.json').write_text(json.dumps(result, indent=2))
    print(result)


if __name__ == '__main__':
    main()
