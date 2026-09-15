"""Compare settled crater lighting with identical process-local replay input.

This does not select a production lighting policy or prove original-game parity.
It writes local captures only; no emulator or host input is involved.
"""
import json
import os
import struct
import subprocess
from pathlib import Path
from PIL import Image
from dev_destruction_check import ROOT, recording


def main():
    folder = ROOT/'artifacts/geomod-lighting-comparison'
    folder.mkdir(parents=True, exist_ok=True)
    data = recording('approach') + b''.join(
        struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0, 0, int(i == 520), 0, 0, 0)
        for i in range(500, 2500))
    inputs = folder/'inputs.bin'
    inputs.write_bytes(data)
    results = {}
    depths = {}
    cameras = {}
    for mode in ('current', 'shadow'):
        env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
        env['RF_REPLAY_DEPTH_OUT'] = str(folder/(mode+'.depth'))
        if mode == 'shadow':
            env['RF_REPLAY_TERRAIN_SHADOW_REFERENCE'] = '1'
        run = subprocess.run([
            str(ROOT/'build/pc/Release/rf_pc_play.exe'), '--dev-room-replay',
            str(ROOT/'Installed_Game'), str(inputs), str(folder/(mode+'.ppm'))],
            cwd=ROOT, env=env, capture_output=True, text=True)
        (folder/(mode+'.log')).write_text(run.stdout+run.stderr)
        run.check_returncode()
        def row(name):
            return list(map(int, next(line.split()[1:] for line in run.stdout.splitlines()
                                     if line.startswith(name+' '))))
        terrain, bake = row('GEOMOD'), row('TERRAIN_BAKE')
        assert terrain[:3] == [1, 3, 4], terrain
        assert bake[0] > 0 and bake[1] == bake[3] == 0 and bake[4] == 4, bake
        assert row('DEBRIS')[1] == 0 and row('PLAYER_LIFE')[0] == 0
        cameras[mode] = row('DEPTH_CAMERA')
        depths[mode] = (folder/(mode+'.depth')).read_bytes()
        assert depths[mode][:4] == b'RFD1' and len(depths[mode]) == 12+640*480*4
        Image.open(folder/(mode+'.ppm')).save(folder/(mode+'.png'))
        results[mode] = dict(terrain=terrain, bake=bake, atlas=row('TERRAIN_ATLAS'))
    results['identical_camera'] = cameras['current'] == cameras['shadow']
    results['identical_depth'] = depths['current'] == depths['shadow']
    results['scope'] = 'Settled PC lighting comparison; inspect both PNGs. No original-game parity claim.'
    (folder/'report.json').write_text(json.dumps(results, indent=2)+'\n')
    assert results['identical_camera'] and results['identical_depth'], results
    print('PASS: three settled cuts, complete bakes, identical camera and every depth pixel')
    print(folder)


if __name__ == '__main__':
    main()
