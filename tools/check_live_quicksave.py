"""Process-local quick-save request during play, followed by fresh reload/fire."""
import json
import os
import subprocess
import struct
from check_precision_pickup import ROOT, prepare, words


def main():
    fixture=ROOT/'artifacts/ordinary-weapon-save'
    prepare(fixture,1)
    folder=ROOT/'artifacts/live-quicksave'
    folder.mkdir(parents=True,exist_ok=True)
    (fixture/'reload-fire.bin').write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(f==60),0,0,0) for f in range(180)))
    clean={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    for phase,replay,extra in (
        ('save',fixture/'rail/acquired.bin',dict(RF_REPLAY_SETUP_UID='910500',RF_REPLAY_QUICKSAVE_FRAME='150')),
        ('reload',fixture/'reload-fire.bin',dict(RF_REPLAY_WORLD_SNAPSHOT_IN=str(folder/'redfaction-save')))):
        env=dict(clean,RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',**extra)
        with (folder/(phase+'.log')).open('wb') as log:
            subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(fixture/'rail/game'),
                            str(replay),str(folder/(phase+'.ppm'))],cwd=folder,env=env,
                           stdout=log,stderr=subprocess.STDOUT,check=True,timeout=240)
    saved=(folder/'save.log').read_text(errors='replace')
    loaded=(folder/'reload.log').read_text(errors='replace')
    assert saved.count('QUICK_SAVE frame150 status0')==1
    assert 'WORLD_SNAPSHOT_LOADED ' in loaded
    assert words(saved,'PLAYER_AMMO')[2]==1 and words(loaded,'PLAYER_AMMO')[2]==0
    assert words(loaded,'COMBAT')[0]==1
    report=dict(result='PASS',save_frame=150,shots_after_reload=1,native_live_save_verified=False,
                scope='Public save-button API from process-local PC replay; no endpoint save flag or host input.')
    (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report)


if __name__=='__main__':main()
