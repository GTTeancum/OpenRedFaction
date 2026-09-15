"""Compare final rendered depth for identical DEV cameras with/without cuts.

RFD1 is little-endian width,height followed by float32 raster depth values.
This audits the PC raster, not original-game visual parity. Quantized depth
noise and uncovered pixels are reported separately, never counted as a cavity.
"""
from array import array
import json,math,os,struct,subprocess,sys
from dev_destruction_check import ROOT,recording

def main():
    folder=ROOT/'artifacts/destruction/depth-audit';folder.mkdir(parents=True,exist_ok=True)
    original=recording('approach')+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(i==520),0,0,0) for i in range(500,900))
    depths={};cameras={};dimensions=None
    for kind in ('cut','intact'):
        data=bytearray(original)
        if kind=='intact':
            for i in range((len(data)-8)//48):struct.pack_into('<I',data,8+i*48+32,0)
        path=folder/(kind+'.bin');path.write_bytes(data)
        env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')};env['RF_REPLAY_DEPTH_OUT']=str(folder/(kind+'.depth'))
        env['RF_REPLAY_MESH_OUT']=str(folder/(kind+'.mesh'))
        r=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),str(path),str(folder/(kind+'.ppm'))],cwd=ROOT,env=env,capture_output=True,text=True)
        (folder/(kind+'.log')).write_text(r.stdout+r.stderr);r.check_returncode()
        row=lambda label:next(l.split()[1:] for l in r.stdout.splitlines() if l.startswith(label+' '))
        cameras[kind]=list(map(int,row('DEPTH_CAMERA')))
        assert int(row('GEOMOD')[1])==(3 if kind=='cut' else 0)
        assert int(row('DEBRIS')[1])==0 and int(row('PLAYER_LIFE')[0])==0
        raw=(folder/(kind+'.depth')).read_bytes();assert raw[:4]==b'RFD1'
        w,h=struct.unpack_from('<II',raw,4);assert len(raw)==12+w*h*4
        assert dimensions is None or dimensions==(w,h);dimensions=(w,h)
        values=array('f');values.frombytes(raw[12:])
        if sys.byteorder!='little':values.byteswap()
        assert (w,h)==(640,480) and all(math.isfinite(v) for v in values)
        depths[kind]=values
    assert cameras['cut']==cameras['intact'],'Camera mismatch invalidates depth comparison'
    # Exclude first-person weapon/HUD at the bottom. Report all remaining
    # pixels;128 encoded depth units separate substantial depth changes from
    # differently triangulated coplanar surfaces on the1/16-pixel grid.
    deltas=[];farther=[];nearer=[];new_clear=[]
    for i in range(w*350):
        a=depths['intact'][i];b=depths['cut'][i];d=b-a;deltas.append(d)
        if b==16777216 and a<16777216:new_clear.append(i)
        elif b<16777216 and d>128:farther.append(i)
        elif d< -128:nearer.append(i)
    bounds=lambda ids:[min(i%w for i in ids),min(i//w for i in ids),max(i%w for i in ids),max(i//w for i in ids)] if ids else None
    report=dict(camera_words=cameras['cut'],dimensions=dimensions,rows_checked=350,depth_tolerance=128,
        min_raw_delta=min(deltas),max_raw_delta=max(deltas),substantially_farther_pixels=len(farther),farther_bounds=bounds(farther),
        substantially_nearer_pixels=len(nearer),new_uncovered_pixels=len(new_clear),uncovered_coordinates=[[i%w,i//w] for i in new_clear],
        limitation='Coplanar projection noise remains; uncovered pixels are an open seam issue. No lighting or original-game parity claim.')
    (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    assert len(farther)>10000 and not nearer,report
    print('PASS: identical camera bits;',len(farther),'recessed solid pixels,',len(nearer),'substantially nearer pixels')
    print('OPEN:',len(new_clear),'new uncovered pixels; minimum raw depth delta',min(deltas))
if __name__=='__main__':main()
