"""Exercise DEV box rejection, recovery, guarded reset and fresh box creation."""
import json
import os
from pathlib import Path
import struct
import subprocess
ROOT=Path(__file__).resolve().parents[1]


def main():
    folder=ROOT/'artifacts/geomod-legacy-controls';folder.mkdir(parents=True,exist_ok=True)
    report=dict(result='FAIL',cases={})
    for name,frames,edits,limited in [('intact',180,{},False),('rejected',180,{110:1},True),
        ('control',650,{220:1,330:2,440:1},False),('recovery',650,{110:1,220:1,330:2,440:1},True)]:
        out=folder/name;out.mkdir(exist_ok=True)
        data=b'RFI6'+struct.pack('<I',48)+b''.join(struct.pack('<5f7I',0,0,0,0,.7 if i<90 else 0,
            int(edits.get(i)==2),0,int(i in edits),0,0,0,int(i in edits)) for i in range(frames))
        (out/'input.bin').write_bytes(data)
        env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
        env.update(RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='0',RF_REPLAY_TERRAIN_PHYSICAL_SNAPSHOT=str(out/'physical.mesh'),
            RF_REPLAY_TERRAIN_BASE_AUDIT=str(out/'atlas.csv'))
        if limited:env.update(RF_REPLAY_TERRAIN_MAP_LIMIT='1',RF_REPLAY_TERRAIN_MAP_LIMIT_UNTIL='200')
        with (out/'run.log').open('wb') as log:
            result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),
                str(out/'input.bin'),str(out/'frame.ppm')],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=120)
        assert result.returncode==0,name+' failed'
        lines=(out/'run.log').read_text().splitlines()
        state=list(map(int,next(l for l in lines if l.startswith('GEOMOD ')).split()[1:]))
        report['cases'][name]=dict(geomod=state,edits=[l for l in lines if l.startswith('GEOMOD_EDIT ')])
    for a,b in [('intact','rejected'),('control','recovery')]:
        for artifact in ('physical.mesh','atlas.csv'):
            assert (folder/a/artifact).read_bytes()==(folder/b/artifact).read_bytes(),artifact+' differs'
    assert report['cases']['rejected']['geomod'][1:3]==[0,1]
    assert report['cases']['rejected']['geomod'][6:]==[1,0]
    assert report['cases']['recovery']['geomod'][1:3]==[1,4]
    assert report['cases']['recovery']['geomod'][6:]==[4,3]
    assert len(report['cases']['recovery']['edits'])==4
    report['result']='PASS'
    (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
