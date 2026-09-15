"""Three real rocket impacts: emitter reuse, expiry and selected visual captures.
No desktop input. Captures require visual inspection; this is not full parity.
"""
import json,os,struct,subprocess
from pathlib import Path
from PIL import Image
from dev_destruction_check import ROOT,recording

def main():
    out=ROOT/'artifacts/impact-lifecycle';out.mkdir(parents=True,exist_ok=True)
    full=recording('approach')+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(i==520),0,0,0) for i in range(500,800))
    results=[]
    for frames in (560,590,650,800):
        inp=out/f'{frames}.bin';inp.write_bytes(full[:8+48*frames]);image=out/f'{frames}.ppm'
        env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
        env.update(RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='0')
        r=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),str(inp),str(image)],cwd=ROOT,env=env,capture_output=True,text=True)
        log=r.stdout+r.stderr;image.with_suffix('.log').write_text(log);r.check_returncode()
        Image.open(image).save(image.with_suffix('.png'))
        def rows(label):return [list(map(int,line.split()[1:])) for line in log.splitlines() if line.startswith(label+' ')]
        starts=rows('IMPACT_START');releases=rows('IMPACT_RELEASE');particles=rows('SCENE_PARTICLES')[0]
        assert len(starts)==3 and all(row[2]==63 for row in starts),starts
        assert starts[0][1]==starts[2][1],starts
        if frames==800:
            assert len(releases)==3 and particles[6]==0,(releases,particles)
            assert all(end[0]-start[0]==121 and end[1]==start[1] and end[2]==63 for start,end in zip(starts,releases))
        else:assert particles[6]>0,particles
        results.append(dict(frames=frames,starts=starts,releases=releases,live_particles=particles[6]))
    report=dict(result='PASS',captures=results,scope=__doc__)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
