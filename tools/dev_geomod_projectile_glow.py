"""Compare authored projectile glow with a supplied pre-integration PC build.

Uses only process-local DEV replays. The baseline executable must come from
04ea5e07 (or equivalent unlit behavior); hashes are recorded, never assumed.
This verifies two selected frames, not a full animation or Xbox pixels.
"""
import argparse,hashlib,json,os,struct,subprocess
from pathlib import Path
from PIL import Image,ImageChops
from dev_destruction_check import ROOT,recording

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',type=Path,required=True)
    parser.add_argument('--out',type=Path,default=ROOT/'artifacts/projectile-glow/pair')
    args=parser.parse_args();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    current=ROOT/'build/pc/Release/rf_pc_play.exe';baseline=args.baseline.resolve()
    full=recording('approach')+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(i==520),0,0,0) for i in range(500,900))
    results=[]
    for frames in (550,560):
        inp=out/f'flight-{frames}.bin';inp.write_bytes(full[:8+48*frames]);logs={}
        for mode,exe in [('base',baseline),('lit',current)]:
            image=out/f'{mode}-{frames}.ppm'
            env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
            env['RF_REPLAY_DEPTH_OUT']=str(image.with_suffix('.depth'))
            run=subprocess.run([str(exe),'--dev-room-replay',str(ROOT/'Installed_Game'),str(inp),str(image)],cwd=ROOT,env=env,capture_output=True,text=True)
            log=run.stdout+run.stderr;image.with_suffix('.log').write_text(log);run.check_returncode();logs[mode]=log
            Image.open(image).save(image.with_suffix('.png'))
        def words(mode,label):
            return next(line.split()[1:] for line in logs[mode].splitlines() if line.startswith(label+' '))
        for label in ('ROCKETS','GEOMOD','PLAYER_LIFE','PC_PLAY_BODY'):
            assert words('base',label)==words('lit',label),label
        rockets=list(map(int,words('lit','ROCKETS')))
        assert rockets[3]==(1 if frames==550 else 0),rockets
        depth_equal=(out/f'base-{frames}.depth').read_bytes()==(out/f'lit-{frames}.depth').read_bytes()
        assert depth_equal
        a=Image.open(out/f'base-{frames}.ppm');b=Image.open(out/f'lit-{frames}.ppm')
        diff=ImageChops.difference(a,b);box=diff.getbbox()
        pixels=list(diff.get_flattened_data());changed=sum(any(p) for p in pixels)
        if frames==550:assert changed>0 and box[3]<350,(changed,box)
        else:assert changed==0,(changed,box)
        results.append(dict(frames=frames,active_rockets=rockets[3],changed_pixels=changed,bounds=box,depth_equal=depth_equal,input_sha256=hashlib.sha256(inp.read_bytes()).hexdigest()))
    report=dict(baseline_sha256=hashlib.sha256(baseline.read_bytes()).hexdigest(),current_sha256=hashlib.sha256(current.read_bytes()).hexdigest(),frames=results,scope=__doc__)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(results,indent=2))

if __name__=='__main__':main()
