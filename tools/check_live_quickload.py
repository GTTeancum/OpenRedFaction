"""One-process save/fire/load/fire, plus missing-save continuation; no host input."""
import json
import os
import struct
import subprocess
from check_precision_pickup import ROOT, prepare, words


def main():
    fixture=ROOT/'artifacts/ordinary-weapon-save'
    prepare(fixture,1)
    folder=ROOT/'artifacts/live-quickload'
    folder.mkdir(parents=True,exist_ok=True)
    rows=[struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(i in (200,330)),0,int(i==30),0) for i in range(400)]
    (folder/'session.bin').write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(rows))
    clean={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    results={}
    for missing in (False,True):
        output=folder/'missing' if missing else folder
        output.mkdir(parents=True,exist_ok=True)
        if missing:assert not list(output.glob('redfaction-save.*'))
        env=dict(clean,RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',
                 RF_REPLAY_SETUP_UID='910500',RF_REPLAY_QUICKLOAD_FRAME='260')
        if not missing:env['RF_REPLAY_QUICKSAVE_FRAME']='150'
        with (output/'session.log').open('wb') as log:
            subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(fixture/'rail/game'),
                            str(folder/'session.bin'),str(output/'session.ppm')],cwd=output,env=env,
                           stdout=log,stderr=subprocess.STDOUT,check=True,timeout=240)
        text=(output/'session.log').read_text(errors='replace')
        if missing:
            assert 'QUICK_LOAD frame260 status-3' in text and 'LEVEL_TRANSITION ' not in text
        else:
            assert text.count('QUICK_SAVE frame150 status0')==text.count('QUICK_LOAD frame260 status0')==1
            assert text.count('WORLD_SNAPSHOT_LOADED ')==text.count('LEVEL_TRANSITION ')==1
            assert words(text,'PLAYER_AMMO')[2]==0 and words(text,'COMBAT')[0]==1
        results['missing' if missing else 'reload']='PASS'
    (folder/'report.json').write_text(json.dumps(dict(results=results,native_verified=False),indent=2)+'\n')
    print(results)


if __name__=='__main__':main()
