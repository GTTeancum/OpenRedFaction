"""Verify process-local crater light addition/removal; captures require visual review."""
import json
import csv
import os
import struct
import subprocess
from PIL import Image
from dev_destruction_check import ROOT, recording


def main():
    folder = ROOT / 'artifacts/geomod-dynamic-cycle'
    folder.mkdir(parents=True, exist_ok=True)
    data = recording('approach') + b''.join(
        struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0, 0, int(i == 520), 0, 0, 0)
        for i in range(500, 2500))
    results, depths, pixels, cameras, atlases = {}, {}, {}, {}, {}
    for mode, frames, enabled in [('base', 1500, False), ('lit', 1500, True),
                                   ('removed', 2500, True), ('control', 2500, False)]:
        inputs = folder / (mode + '.bin')
        inputs.write_bytes(data[:8 + frames * 48])
        env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
        env['RF_REPLAY_TERRAIN_BASE_AUDIT'] = str(folder / (mode + '.csv'))
        env['RF_REPLAY_DEPTH_OUT'] = str(folder / (mode + '.depth'))
        if enabled:
            env['RF_REPLAY_TERRAIN_TEST_LIGHT'] = '1'
        run = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'),
            '--dev-room-replay', str(ROOT / 'Installed_Game'), str(inputs),
            str(folder / (mode + '.ppm'))], cwd=ROOT, env=env, capture_output=True, text=True)
        (folder / (mode + '.log')).write_text(run.stdout + run.stderr)
        run.check_returncode()
        def row(name):
            return list(map(int, next(line.split()[1:] for line in run.stdout.splitlines()
                                     if line.startswith(name + ' '))))
        terrain, bake = row('GEOMOD'), row('TERRAIN_BAKE')
        assert terrain[:3] == [1, 3, 4], terrain
        assert bake[0] > 0 and bake[1] == bake[3] == 0 and bake[4] == 4, bake
        assert row('DEBRIS')[1] == 0 and row('PLAYER_LIFE')[0] == 0
        depths[mode] = (folder / (mode + '.depth')).read_bytes()
        assert depths[mode][:4] == b'RFD1' and len(depths[mode]) == 12 + 640*480*4
        pixels[mode] = (folder / (mode + '.ppm')).read_bytes()
        cameras[mode] = row('DEPTH_CAMERA')
        Image.open(folder / (mode + '.ppm')).save(folder / (mode + '.png'))
        with (folder / (mode + '.csv')).open() as source:
            atlases[mode] = list(csv.DictReader(source))
        assert atlases[mode], 'Missing completed atlas maps'
        results[mode] = dict(terrain=terrain, bake=bake)
        print(mode, flush=True)
    assert depths['base'] == depths['lit'] and depths['removed'] == depths['control']
    assert cameras['base'] == cameras['lit'] and cameras['removed'] == cameras['control']
    assert pixels['base'] != pixels['lit'], 'Test light made no visible change'
    assert pixels['removed'] == pixels['control'], 'Removed light left stale pixels'
    assert atlases['base'] == atlases['control'] == atlases['removed'], 'Light removal changed retained maps or base texels'
    assert len(atlases['base']) == len(atlases['lit'])
    changed_maps = 0
    for base, lit in zip(atlases['base'], atlases['lit']):
        assert {k:v for k,v in base.items() if k != 'packed'} == {k:v for k,v in lit.items() if k != 'packed'}, 'Light changed mapping identity or bindings'
        changed_maps += base['packed'] != lit['packed']
    assert changed_maps, 'Diagnostic light changed no atlas texels'
    results['atlas'] = dict(maps=len(atlases['base']), changed_maps=changed_maps, exact_restoration=True)
    results['scope'] = 'PC diagnostic light; unchanged depth, visible change, exact removal restoration. Inspect PNGs; no native or visual parity claim.'
    (folder / 'report.json').write_text(json.dumps(results, indent=2) + '\n')
    print('PASS: visible light change, identical depth, exact restoration')


if __name__ == '__main__':
    main()
