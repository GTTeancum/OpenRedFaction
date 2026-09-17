"""Separate transient blast appearance from retained crater material and lighting."""
import csv
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]


def main():
    folder=ROOT/'artifacts/crater-effect-decay';folder.mkdir(parents=True,exist_ok=True)
    source=(ROOT/'artifacts/debris-relaunch/inputs.bin').read_bytes()
    assert source[:8]==b'RFI6'+struct.pack('<I',48) and len(source)==8+550*48
    exe=ROOT/'build/pc/Release/rf_pc_play.exe';results=[]
    for frames in (550,900):
        out=folder/str(frames);out.mkdir(exist_ok=True)
        replay=source+bytes((frames-550)*48);(out/'input.bin').write_bytes(replay)
        env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
        env.update(RF_REPLAY_LEVEL='glass_house.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',
                   RF_REPLAY_DEPTH_OUT=str(out/'depth.bin'),
                   RF_REPLAY_TERRAIN_MATERIAL_AUDIT=str(out/'material.bin'),
                   RF_REPLAY_TERRAIN_BASE_AUDIT=str(out/'atlas.csv'),
                   RF_REPLAY_TERRAIN_PHYSICAL_SNAPSHOT=str(out/'physical.mesh'))
        with (out/'run.log').open('wb') as log:
            subprocess.run([str(exe),'--spawn-replay',str(ROOT/'Installed_Game'),str(out/'input.bin'),str(out/'frame.ppm')],
                           cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
        lines=(out/'run.log').read_text().splitlines()
        state=lambda name:list(map(int,next(line for line in lines if line.startswith(name+' ')).split()[1:]))
        with (out/'atlas.csv').open() as atlas:charts=list(csv.DictReader(atlas))
        used=[row for row in charts if int(row['faces'])>0]
        words=[v[0] for row in used for v in struct.iter_unpack('<H',bytes.fromhex(row['packed']))]
        assert words
        results.append(dict(frames=frames,camera=state('DEPTH_CAMERA')[1:],particles=state('SCENE_PARTICLES'),used_maps=len(used),samples=len(words),
            mean_light_rgb=[sum(((v>>shift)&31)/31 for v in words)/len(words) for shift in (10,5,0)],
            hashes={name:hashlib.sha256((out/name).read_bytes()).hexdigest() for name in ('material.bin','physical.mesh','atlas.csv','frame.ppm')}))
    stable={name:results[0]['hashes'][name]==results[1]['hashes'][name] for name in ('material.bin','physical.mesh','atlas.csv')}
    stable['camera']=results[0]['camera']==results[1]['camera']
    report=dict(result='PASS' if all(stable.values()) else 'FAIL',binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),input_sha256=hashlib.sha256(source).hexdigest(),
                stable=stable,runs=results,scope='Whole material, physical mesh and atlas comparisons across fixed-camera ordinary idle continuation; atlas statistics are not screen-weighted and do not establish original visual parity.')
    (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    assert all(stable.values()),'Persistent surface changed during idle; inspect evidence'


if __name__=='__main__':main()
