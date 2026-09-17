"""Verify settled-fragment orientation retention and resumed rotation in a live trace."""
import argparse
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path


def verify(path):
    previous={};pending={};accepted=[]
    for line in path.read_text().splitlines():
        if line.startswith('DEBRIS_RELAUNCH_ROTATION '):
            slot,old_count,new_count,*words=map(int,line.split()[1:])
            assert len(words)==18 and words[:9]==words[9:],line
            if old_count==0:
                assert new_count>0 and slot in previous,line
                assert previous[slot][1]==words[:9],('settled basis changed',slot)
                pending[slot]=(previous[slot][0],words[:9])
        elif line.startswith('DEBRIS_ROTATION_SAMPLE '):
            frame,slot,*words=map(int,line.split()[1:]);assert len(words)==23
            if slot in pending:
                last_frame,basis=pending.pop(slot)
                assert frame>last_frame+1,('no observed resting interval',slot)
                assert words[:9]==basis,('resumed from different orientation',slot)
                assert words[14:]!=basis,('rotation did not resume',slot)
                accepted.append(dict(slot=slot,last_moving_frame=last_frame,resumed_frame=frame,basis=basis))
            previous[slot]=(frame,words[14:])
    assert accepted and not pending,'No complete settled-fragment rotation resumption'
    return dict(result='PASS',log_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),resumptions=accepted,
                scope='Live trace confirms preserved orientation across rest/relaunch and the next moving update; no visual animation claim.')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path,nargs='?')
    parser.add_argument('--run',action='store_true',help='Generate and execute the ordinary two-shot PC replay first')
    args=parser.parse_args()
    if args.run:
        root=Path(__file__).resolve().parents[1]
        subprocess.run([sys.executable,str(root/'tools/replay_debris_relaunch.py')],cwd=root,check=True,capture_output=True)
        folder=root/'artifacts/debris-relaunch';args.log=folder/'rotation.log'
        env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
        env.update(RF_REPLAY_LEVEL='glass_house.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',
                   RF_REPLAY_DEV_ROOM='1',RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='0')
        with args.log.open('wb') as output:
            subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',
                            str(root/'Installed_Game'),str(folder/'inputs.bin'),str(folder/'rotation.ppm')],
                           cwd=root,env=env,stdout=output,stderr=subprocess.STDOUT,check=True,timeout=120)
    if args.log is None:parser.error('provide a log or --run')
    print(json.dumps(verify(args.log),indent=2))
