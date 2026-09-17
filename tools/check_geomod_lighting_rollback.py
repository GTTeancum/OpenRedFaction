"""Force live map exhaustion after a saved cut, then allow a later blast."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir',type=Path,default=ROOT/'artifacts/geomod-lighting-rollback')
    args=parser.parse_args();folder=args.output_dir.resolve();folder.mkdir(parents=True,exist_ok=True)
    exe=ROOT/'build/pc-expanded/Release/rf_pc_play.exe'
    report=dict(result='FAIL',binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),cases={})
    def run(name,frames,shots,checkpoint=None,limited=False,walk=False):
        out=folder/name;out.mkdir(exist_ok=True)
        (out/'state.rfds').unlink(missing_ok=True)
        data=b'RFI6'+struct.pack('<I',48)+b''.join(struct.pack('<5f7I',0,0,int(walk and 350<=i<710),0,.7 if i<90 else 0,
            0,0,0,int(i in shots),0,int(i in (10,20,30,40)),0) for i in range(frames))
        (out/'input.bin').write_bytes(data)
        env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
        env.update(RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='0',RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(out/'state.rfds'),
            RF_REPLAY_TERRAIN_PHYSICAL_SNAPSHOT=str(out/'physical.mesh'),RF_REPLAY_TERRAIN_BASE_AUDIT=str(out/'atlas.csv'))
        if checkpoint:env['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(checkpoint)
        if limited:
            # Permit one additional map before exhaustion, exercising partial
            # private preparation rather than rejecting the first new mapping.
            limit=len((checkpoint.parent/'atlas.csv').read_text().splitlines())
            env.update(RF_REPLAY_TERRAIN_MAP_LIMIT=str(limit),RF_REPLAY_TERRAIN_MAP_LIMIT_UNTIL='300')
        with (out/'run.log').open('wb') as log:
            result=subprocess.run([str(exe),'--dev-room-replay',str(ROOT/'Installed_Game'),str(out/'input.bin'),str(out/'frame.ppm')],
                cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
        assert result.returncode==0,name+' failed'
        lines=(out/'run.log').read_text().splitlines()
        def words(label):return list(map(int,next(l for l in lines if l.startswith(label+' ')).split()[1:]))
        body=words('PC_PLAY_BODY')
        entry=dict(geomod=words('GEOMOD'),rockets=words('ROCKETS'),body_position_orientation=body[22:37],
            position=struct.unpack('<3f',struct.pack('<3I',*body[22:25])),
            rejection=[l for l in lines if l.startswith('GEOMOD_REJECT ')])
        report['cases'][name]=entry
        return out,entry
    try:
        seed,a=run('seed',320,{110});assert a['geomod'][1]==1
        control,b=run('control',320,set(),seed/'state.rfds')
        rejected,c=run('rejected',320,{110},seed/'state.rfds',True)
        assert b['geomod'][1]==c['geomod'][1]==1 and c['rejection']
        assert c['rockets'][4:6]==[0,1]
        for name in ('physical.mesh','atlas.csv'):
            assert (control/name).read_bytes()==(rejected/name).read_bytes(),name+' changed after rejection'
        continued,d=run('continued',750,{110,440},seed/'state.rfds',True)
        assert d['geomod'][1]==2 and len(d['rejection'])==1 and d['rockets'][4:6]==[1,1]
        _,e=run('walk_control',850,set(),seed/'state.rfds',walk=True)
        _,f=run('walk_rejected',850,{110},seed/'state.rfds',True,True)
        assert e['body_position_orientation']==f['body_position_orientation'] and f['position'][0]<-16
        report['result']='PASS'
        report['scope']='Real live map-capacity failure preserves prior mesh and settled atlas, and a later ordinary blast commits. Admission/RNG intentionally continue; full checkpoint bytes are not expected unchanged. No Xbox or audio acceptance.'
    finally:
        (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
