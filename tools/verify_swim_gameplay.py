"""Process-local authored swimming: entry, ascent, surface exit, re-entry and descent."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]


def main():
    subprocess.run([sys.executable,str(ROOT/'tools/replay_swim_fixture.py')],cwd=ROOT,check=True)
    folder=ROOT/'artifacts/swim-fixture'
    source=(folder/'input.bin').read_bytes()
    exe=ROOT/'build/pc/Release/rf_pc_play.exe'
    digest=hashlib.sha256(exe.read_bytes()).hexdigest()
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_SWIM_TEST='1',RF_REPLAY_LEVEL='L2S3.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
    cases={str(n):source[:8+n*48] for n in (30,90,120,150,240)}
    for name in ('idle','down'):
        data=bytearray(source[:8]+bytes(90*48))
        if name=='down':
            for frame in range(30,90):struct.pack_into('<I',data,8+frame*48+20,1)
        cases[name]=bytes(data)
    results={}
    for name,data in cases.items():
        path=folder/('verify-'+name+'.bin');path.write_bytes(data)
        run=subprocess.run([str(exe),'--spawn-replay',str(ROOT/'Installed_Game'),str(path),
                            str(folder/('verify-'+name+'.ppm'))],env=env,cwd=ROOT,capture_output=True,text=True)
        (folder/('verify-'+name+'.log')).write_text(run.stdout+run.stderr)
        run.check_returncode()
        state=list(map(int,next(s for s in run.stdout.splitlines() if s.startswith('PLAYER_SWIM ')).split()[1:]))
        enemy=list(map(int,next(s for s in run.stdout.splitlines() if s.startswith('ENEMY_COMBAT ')).split()[1:]))
        assert enemy[1:4]==[0,0,0], 'Swimming fixture must have no enemy attacks'
        liquid=list(map(int,next(s for s in run.stdout.splitlines() if s.startswith('LIQUID_DAMAGE ')).split()[1:]))
        assert liquid[0]>0 and liquid[1]==0 and liquid[7]==0, 'Ordinary water must not dispatch liquid damage'
        height,body,eye=[struct.unpack('<f',struct.pack('<I',v))[0] for v in state[8:11]]
        assert state[11]==0 and state[7]==1956 and state[0]==0
        results[name]=dict(state=state,height=height,body=body,eye=eye)
    assert results['30']['state'][1:6]==[1,1,4,1,0]
    assert results['90']['body']>results['30']['body']+1
    assert results['90']['state'][3]==3 and results['90']['state'][5]>=1
    assert results['240']['state'][1:4]==[1,1,4] and results['240']['state'][4]>=2
    assert results['down']['body']<results['idle']['body']-.2, results
    assert hashlib.sha256(exe.read_bytes()).hexdigest()==digest
    (folder/'gameplay-verification.json').write_text(json.dumps(dict(result='PASS',pc_sha256=digest,cases=results,
        scope='Authored L2S3 movement and held-input behavior; oxygen, swimming audio and visual parity unverified'),indent=2))
    print('PASS swimming entry/ascent/surface exit/re-entry and descent versus idle')


if __name__=='__main__':main()
