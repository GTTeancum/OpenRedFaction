"""Replay a saved crater and compare its live material owner to installed rock02.

No desktop input or original-game process. This verifies CPU material storage
and physical-face/atlas material IDs, not GPU upload or visual parity.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def decode_tga(data):
    ident, cmap, kind = struct.unpack_from('<3B', data)
    width, height, bits, descriptor = struct.unpack_from('<HHBB', data, 12)
    if cmap or kind != 2 or bits != 24 or descriptor & 0xcf:
        raise ValueError('Expected non-interleaved opaque 24-bit TGA')
    start = 18+ident
    if len(data) < start+width*height*3:
        raise ValueError('Truncated TGA')
    rgba = bytearray(width*height*4)
    for y in range(height):
        for x in range(width):
            sx = width-1-x if descriptor & 16 else x
            sy = y if descriptor & 32 else height-1-y
            b, g, r = data[start+(sy*width+sx)*3:start+(sy*width+sx)*3+3]
            rgba[(y*width+x)*4:(y*width+x+1)*4] = bytes((r, g, b, 255))
    return width, height, bytes(rgba)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkpoint', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path, default=ROOT/'build/pc-expanded')
    args = parser.parse_args()
    folder = args.output_dir.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    exe = args.build_dir.resolve()/'Release/rf_pc_play.exe'
    checkpoint = args.checkpoint.resolve()
    report = dict(result='FAIL', scope=__doc__,
                  binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
                  checkpoint_sha256=hashlib.sha256(checkpoint.read_bytes()).hexdigest())
    try:
        data = b'RFI6'+struct.pack('<I',48)+bytes(48*32)
        (folder/'input.bin').write_bytes(data)
        env = {k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
        env.update(RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(checkpoint),
                   RF_REPLAY_TERRAIN_MATERIAL_AUDIT=str(folder/'material.bin'),
                   RF_REPLAY_TERRAIN_BASE_AUDIT=str(folder/'atlas.csv'),
                   RF_REPLAY_TERRAIN_PHYSICAL_SNAPSHOT=str(folder/'physical.mesh'))
        for name in ('material.bin', 'atlas.csv', 'physical.mesh'):
            (folder/name).unlink(missing_ok=True)
        with (folder/'run.log').open('wb') as log:
            result = subprocess.run([str(exe), '--dev-room-replay', str(ROOT/'Installed_Game'),
                str(folder/'input.bin'), str(folder/'frame.ppm')], cwd=ROOT, env=env,
                stdout=log, stderr=subprocess.STDOUT, timeout=180)
        assert result.returncode == 0, 'Replay failed; inspect run.log'
        live = (folder/'material.bin').read_bytes()
        assert live[:4] == b'RFT1' and len(live) >= 24
        material, width, height, fmt, size = struct.unpack_from('<5I', live, 4)
        assert size == width*height*4 and len(live) == 24+size and fmt == 6
        inventory = json.loads((ROOT/'artifacts/inventory.json').read_text())
        archive = next(a for a in inventory['files'] if a['path'] == 'ui.vpp')
        entry = next(e for e in archive['vpp']['entries'] if e['name'] == 'rock02.tga')
        with (ROOT/'Installed_Game/ui.vpp').open('rb') as source:
            source.seek(entry['offset'])
            tga = source.read(entry['size'])
        expected_width, expected_height, expected = decode_tga(tga)
        assert (width,height) == (expected_width,expected_height)
        assert live[24:] == expected, 'Live substrate differs from independently decoded rock02'
        mesh = (folder/'physical.mesh').read_bytes()
        assert mesh[:4] == b'RGM1'
        nv, nf = struct.unpack_from('<2I',mesh,4)
        assert len(mesh) == 12+nv*20+nf*16
        faces = [struct.unpack_from('<4I',mesh,12+nv*20+i*16) for i in range(nf)]
        generated = [f for f in faces if f[3] == 0xffffffff]
        assert generated and all(f[2] == material for f in generated)
        with (folder/'atlas.csv').open(newline='') as source:
            charts = list(csv.DictReader(source))
        assert charts and all(int(row['material']) == material for row in charts)
        assert sum(int(row['faces']) for row in charts) == len(generated)
        report.update(result='PASS', material=material, width=width, height=height,
                      source_format=fmt, rgba_bytes=size, generated_faces=len(generated),
                      maps=len(charts), asset_sha256=hashlib.sha256(tga).hexdigest(),
                      rgba_sha256=hashlib.sha256(expected).hexdigest(),
                      mean_rgb=[sum(expected[c::4])/(width*height) for c in range(3)])
    finally:
        (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()
