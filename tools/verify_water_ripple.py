"""Reconstructed DEV ripple rendering/lifetime regression; no original screenshots.
The synthetic contact tests VFX rendering only, not authored liquid collision.
"""
import json
import os
from pathlib import Path
import struct
import subprocess
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'artifacts/ripple-live'

def run(frames,enabled):
    name=f'{frames}-'+('ripple' if enabled else 'control')
    source=OUT/f'{frames}.bin'
    source.write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(48*frames))
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    if enabled:env['RF_REPLAY_RIPPLE_TEST']='1'
    result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',
        str(ROOT/'Installed_Game'),str(source),str(OUT/(name+'.ppm'))],env=env,capture_output=True,text=True)
    (OUT/(name+'.log')).write_text(result.stdout+result.stderr)
    assert result.returncode==0,(name,result.returncode)
    values={line.split()[0]:list(map(int,line.split()[1:])) for line in result.stdout.splitlines()
        if line.startswith(('RIPPLE_VISUAL ','RIPPLE_LIFECYCLE ','ROCKET_LIQUID_STATE '))}
    img=Image.open(OUT/(name+'.ppm')).convert('RGB');img.save(OUT/(name+'.png'))
    return values,img.tobytes()

def main():
    OUT.mkdir(parents=True,exist_ok=True);rows=[]
    for frames in (24,96,97):
        actual,pixels=run(frames,True);control,base=run(frames,False)
        assert actual['ROCKET_LIQUID_STATE']==[0]*4,'render fixture must not fabricate liquid contacts'
        assert actual['RIPPLE_LIFECYCLE']==[1,int(frames>=97),0,1]
        assert actual['RIPPLE_VISUAL'][1]==int(frames<97)
        assert actual['RIPPLE_VISUAL'][6]==0
        changed=sum(a!=b for a,b in zip(pixels,base));darkened=sum(a<b for a,b in zip(pixels,base))
        if frames==24:assert changed>0 and darkened==0,'additive ripple must brighten visible pixels'
        if frames==97:assert pixels==base,'expired effect must leave no pixels/depth residue'
        rows.append(dict(frames=frames,telemetry=actual,changed_channels=changed,darkened_channels=darkened))
    report=dict(scope='Synthetic contact VFX rendering and lifetime, not wet gameplay or lighting parity',cases=rows)
    (OUT/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS ripple additive pixels, bounded lifetime and expiry; authored wet entry remains separate')
if __name__=='__main__':main()
