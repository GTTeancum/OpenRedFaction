"""Verify retained crater seeds against actual completed atlas rectangles.

Runs process-local playback; reconstructs the original CRT sequence independently
in Python. Does not enable dynamic lighting or control the desktop.
"""
import argparse
import csv
import json
import os
from pathlib import Path
import struct
import subprocess
ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('replay',type=Path)
    parser.add_argument('--name',default='base-seeds')
    args=parser.parse_args()
    assert args.name and all(c.isalnum() or c in '-_' for c in args.name)
    folder=ROOT/'artifacts/destruction'
    folder.mkdir(parents=True,exist_ok=True)
    audit=folder/(args.name+'.csv')
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env['RF_REPLAY_TERRAIN_BASE_AUDIT']=str(audit)
    run=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',
                        str(ROOT/'Installed_Game'),str(args.replay.resolve()),str(folder/(args.name+'.ppm'))],
                       cwd=ROOT,env=env,capture_output=True,text=True)
    (folder/(args.name+'.log')).write_text(run.stdout+run.stderr)
    run.check_returncode()
    texels=0
    with audit.open() as source:
        maps=list(csv.DictReader(source))
    assert maps
    for index,row in enumerate(maps):
        assert int(row['map'])==index
        state=int(row['seed'])
        count=int(row['width'])*int(row['height'])
        assert 0<count<=4096
        expected=bytearray()
        for _ in range(count):
            state=(state*214013+2531011)&0xffffffff
            gray=(((state>>16)&32767)&63)+32
            value=0x8000|((gray>>3)<<10)|((gray>>3)<<5)|(gray>>3)
            expected.extend(struct.pack('<H',value))
        assert expected==bytes.fromhex(row['packed']),index
        if index+1<len(maps):
            assert state==int(maps[index+1]['seed']),index
        texels+=count
    report=dict(maps=len(maps),texels=texels,first_seed=int(maps[0]['seed']),
                scope='Retained seeds reproduce every completed packed base rectangle; dynamic updates are not enabled')
    audit.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS:',len(maps),'retained maps,',texels,'texels and continuous original RNG state')


if __name__=='__main__':main()
