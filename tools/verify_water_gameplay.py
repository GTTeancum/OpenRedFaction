"""Authored dm03 water entry via ordinary process-local input; no host input."""
import json,os,re,subprocess,sys
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
def main():
    out=ROOT/'artifacts/water-fixture';out.mkdir(exist_ok=True)
    subprocess.run([sys.executable,str(ROOT/'tools/replay_water_fixture.py')],cwd=ROOT,check=True)
    recording=(out/'input.bin').read_bytes();rows=[]
    for frames in (65,180):
        source=out/f'wet-{frames}.bin';source.write_bytes(recording[:8+48*frames])
        env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
        env.update(RF_REPLAY_WATER_TEST='1',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_LEVEL='dm03.rfl',RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='0')
        result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(source),str(out/f'wet-{frames}.ppm')],env=env,capture_output=True,text=True)
        (out/f'wet-{frames}.log').write_text(result.stdout+result.stderr)
        assert result.returncode==0,(frames,result.stderr)
        events=[line for line in result.stdout.splitlines() if line.startswith(('ROCKET_LIQUID ','ROCKET_IMPACT '))]
        values={line.split()[0]:list(map(int,line.split()[1:])) for line in result.stdout.splitlines() if line.startswith(('RIPPLE_LIFECYCLE ','ROCKET_LIQUID_STATE '))}
        assert len([s for s in events if s.startswith('ROCKET_LIQUID ')])==1
        assert values['ROCKET_LIQUID_STATE'][:2]==[1,4]
        assert values['RIPPLE_LIFECYCLE']==[1,int(frames==180),0,0]
        assert len([s for s in events if s.startswith('ROCKET_IMPACT ')])==int(frames==180)
        Image.open(out/f'wet-{frames}.ppm').save(out/f'wet-{frames}.png')
        rows.append(dict(frames=frames,events=events,telemetry=values))
    (out/'gameplay-verification.json').write_text(json.dumps(dict(scope='PC authored water entry, subsequent solid contact and ripple lifetime; nearby blast kills player; wet Xbox/audio unverified',cases=rows),indent=2)+'\n')
    print('PASS authored water entry, solid continuation, single ripple and expiry')
if __name__=='__main__':main()
