"""Ordinary authored railgun grant, save, fresh reload and finite-ammo fire."""
import json
import os
import struct
import subprocess
from pathlib import Path
from check_precision_pickup import ROOT, prepare, words


def main():
    folder=ROOT/'artifacts/ordinary-weapon-save'
    folder.mkdir(parents=True,exist_ok=True)
    recipe=prepare(folder,1)
    job=next(j for j in recipe['jobs'] if j['kind']=='rail' and j['phase']=='acquired')
    clean={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    for base in ('source','resaved'):
        for slot in range(2):Path(str(folder/base)+'.'+str(slot)).unlink(missing_ok=True)
    env=dict(clean,**job['env'],RF_REPLAY_WORLD_SNAPSHOT_OUT=str(folder/'source'))
    with (folder/'capture.log').open('wb') as log:
        subprocess.run(job['command'],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=240)
    rows=[struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(f==60),0,0,0) for f in range(180)]
    (folder/'reload-fire.bin').write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(rows))
    env=dict(clean,RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',
             RF_REPLAY_WORLD_SNAPSHOT_IN=str(folder/'source'),RF_REPLAY_WORLD_SNAPSHOT_OUT=str(folder/'resaved'))
    with (folder/'reload-fire.log').open('wb') as log:
        subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(folder/'rail/game'),
                        str(folder/'reload-fire.bin'),str(folder/'reload-fire.ppm')],cwd=ROOT,env=env,
                       stdout=log,stderr=subprocess.STDOUT,check=True,timeout=240)
    before=(folder/'capture.log').read_text(errors='replace')
    after=(folder/'reload-fire.log').read_text(errors='replace')
    a,b=words(before,'PLAYER_AMMO'),words(after,'PLAYER_AMMO')
    combat=words(after,'COMBAT')
    assert 'WORLD_SNAPSHOT_LOADED ' in after and 'WORLD_SNAPSHOT_STORED ' in after
    assert words(after,'WEAPON_SELECTION')[0]==7 and combat[0]==1
    assert a[:2]==b[:2] and a[2]>0 and b[2]==a[2]-1,(a,b)
    result=dict(status='PASS',ammo_before=a,ammo_after=b,combat=combat,native_verified=False,
                scope='Ordinary enemy-free authored grant fixture; fresh load and one finite-ammo rail shot. No host input.')
    (folder/'report.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
