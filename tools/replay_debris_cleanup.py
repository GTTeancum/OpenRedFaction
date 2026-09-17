"""Ordinary two-shot settled-debris cleanup, with a rejected-edit control."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]


def recording():
    return b'RFI6'+struct.pack('<I',48)+b''.join(struct.pack('<5f7I',0,0,.8 if 130<=i<306 else 0,
        -.2 if 414<=i<424 else 0,.7 if i<90 else 0,0,0,0,int(i in (316,430)),0,int(i in (10,20,30,40)),0) for i in range(530))


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--native-report',type=Path);args=parser.parse_args()
    data=recording();digest=hashlib.sha256(data).hexdigest()
    if args.native_report:
        report=json.loads(args.native_report.read_text())
        assert report['result']=='PASS' and report['disc_restored'] and report['input_sha256']==digest
        assert report['memory']=={'base-memory':67108864,'plugged-memory':0}
        for platform in ('pc','xbox'):
            assert report['checks']['DEBRIS_CLEANUP'][platform]==[2,41,37,4,1,1408358166,0,0]
        print('PASS stock64MiB ordinary settled-debris cleanup');return
    folder=ROOT/'artifacts/debris-cleanup-live';folder.mkdir(exist_ok=True)
    replay=folder/'ordinary.bin';replay.write_bytes(data);results={}
    for name,rejected in [('ordinary',False),('rejected',True)]:
        env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
        env.update(RF_REPLAY_LEVEL='glass_house.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='0')
        if rejected:env.update(RF_REPLAY_TERRAIN_MAP_LIMIT='1',RF_REPLAY_TERRAIN_MAP_LIMIT_UNTIL='700')
        log=folder/(name+'.log')
        with log.open('wb') as output:
            subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(replay),str(folder/(name+'.ppm'))],
                cwd=ROOT,env=env,stdout=output,stderr=subprocess.STDOUT,check=True,timeout=120)
        lines=log.read_text().splitlines();words=lambda label:list(map(int,next(l for l in lines if l.startswith(label+' ')).split()[1:]))
        state=words('DEBRIS_CLEANUP');rockets=words('ROCKETS')
        if rejected:
            assert state==[0]*8 and rockets[4:6]==[0,2]
            assert sum(l.startswith('GEOMOD_REJECT ') for l in lines)==2
        else:
            assert state==[2,41,37,4,1,1408358166,0,0] and rockets[4:6]==[2,0]
            subprocess.run([sys.executable,str(ROOT/'tools/probe_debris_postedit.py'),'--live-log',str(log)],cwd=ROOT,check=True)
        results[name]=dict(cleanup=state,rockets=rockets,log_sha256=hashlib.sha256(log.read_bytes()).hexdigest())
    (folder/'report.json').write_text(json.dumps(dict(result='PASS',input_sha256=digest,cases=results),indent=2)+'\n')
    print('PASS ordinary positive cleanup and two rejected-edit controls')


if __name__=='__main__':main()
