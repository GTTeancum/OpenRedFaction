"""PC process-local lamp damage/checkpoint replay, using a relocated authored prop.
No host input. Only one lamp position changes; original classes, meshes and all506 props remain.
"""
import argparse, io, json, math, os, re, struct, subprocess
from pathlib import Path
from build_fragment_platform_fixture import read_entry
from inspect_levels import inspect as inspect_level
from inspect_clutter_records import inspect as inspect_clutter
ROOT=Path(__file__).resolve().parents[1]
TARGET=13025

def prepare(folder):
    game=folder/'game';game.mkdir(parents=True,exist_ok=True)
    original=read_entry(ROOT/'artifacts/clutter-visibility-live/game/levelsm.vpp','ctf06.rfl')
    level=bytearray(original)
    meta=inspect_level(io.BytesIO(level),dict(offset=0,size=len(level),name='ctf06.rfl'))
    section=next(s for s in meta['sections'] if s['type']=='0x50000')
    records=inspect_clutter(level[section['offset']+8:section['offset']+8+section['size']])
    row=next(r for r in records if r['uid']==TARGET)
    assert len(records)==506 and row['class_name']==b'lantern_box'
    pos=(-4.25,.267,2.5)
    struct.pack_into('<3f',level,section['offset']+8+row['offset']+6+len(row['class_name']),*pos)
    size=4096+((len(level)+2047)&~2047);archive=bytearray(size)
    struct.pack_into('<4I',archive,0,0x51890ace,1,1,size)
    archive[2048:2057]=b'ctf06.rfl';struct.pack_into('<I',archive,2108,len(level));archive[4096:4096+len(level)]=level
    output=game/'levelsm.vpp';assert not output.exists() or output.stat().st_nlink==1;output.write_bytes(archive)
    for source in [*(ROOT/'Installed_Game').glob('*.vpp'),ROOT/'Installed_Game/bluebeard.bty']:
        if source.name.lower()=='levelsm.vpp':continue
        out=game/source.name
        if not out.exists():os.link(source,out)
        else:assert os.path.samefile(source,out)
    jobs=[]
    for name,shots,load in [('baseline',[],None),('damaged',[40],None),('dead',[40,72],None),('damaged-resumed',[],'damaged'),('dead-resumed',[],'dead')]:
        data=[]
        # Standing eye from post fixture: body-.6184783 + eye.7854025.
        look=math.atan2(pos[1]-(-.6184783+.7854025),1.5)*60/12
        for frame in range(180):
            data.append(struct.pack('<5f7I',0,0,0,look if not load and 12<=frame<24 else 0,0,0,0,0,int(frame in shots),0,0,0))
        (folder/(name+'.bin')).write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(data))
        env=dict(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1')
        if load:env['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/(load+'.rfcp'))
        else:env['RF_REPLAY_GEOMOD_CHECKPOINT_OUT']=str(folder/(name+'.rfcp'))
        jobs.append(dict(name=name,env=env,command=[str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(game),str(folder/(name+'.bin')),str(folder/(name+'.ppm'))]))
    report=dict(status='PREPARED',target=TARGET,fixture='Only lamp UID13025 relocated to DEV spawn; no checkpoint mutation or synthetic damage injection.',original_position=struct.unpack('<3f',row['position']),position=pos,jobs=jobs)
    (folder/'recipe.json').write_text(json.dumps(report,indent=2)+'\n')
    return report

def checkpoint(path):
    data=path.read_bytes();header=struct.unpack_from('<4s7I',data)
    assert header[:3]==(b'RFCP',4,len(data)),header
    off=32+header[3]+header[5]+header[6]+header[7]
    assert data[off:off+4]==b'RFPC';count=struct.unpack_from('<I',data,off+16)[0]
    rows=[struct.unpack_from('<IIfIii',data,off+64+i*24) for i in range(count)]
    return next(row for row in rows if row[0]==TARGET)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');parser.add_argument('--phase');args=parser.parse_args()
    folder=ROOT/'artifacts/clutter-checkpoint-live';report=prepare(folder)
    if not args.run:return
    clean={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    states={}
    for job in report['jobs']:
        name=job['name']
        if args.phase and name!=args.phase:continue
        path=folder/(name+'.rfcp');path.unlink(missing_ok=True)
        for suffix in ('.ppm','.png'):(folder/(name+suffix)).unlink(missing_ok=True)
        with (folder/(name+'.log')).open('wb') as log:
            result=subprocess.run(job['command'],cwd=ROOT,env=dict(clean,**job['env']),stdout=log,stderr=subprocess.STDOUT,timeout=240)
        print(name,result.returncode,flush=True)
        if result.returncode:raise RuntimeError(f'Inspect {name}.log')
        text=(folder/(name+'.log')).read_text(errors='replace')
        if name.endswith('resumed'):
            assert not path.exists(), 'This phase verifies reload only; native harness verifies resaving'
            incoming=Path(job['env']['RF_REPLAY_GEOMOD_CHECKPOINT_IN']);saved=checkpoint(incoming)
            matches=re.findall(r'CLUTTER_CHECKPOINT_RESTORE uid(\d+) health([^ ]+) flags(\d+) cooldown(-?\d+)',text)
            restored=[(int(uid),float(health),int(flags),int(cooldown)) for uid,health,flags,cooldown in matches if int(uid)==TARGET]
            assert restored==[(TARGET,saved[2],saved[3],saved[5])], restored
            assert 'PLAYER_CHECKPOINT_LOAD ' in text
            assert 'CLUTTER_BREAK uid' not in text and 'CLUTTER_BREAK_IMPACT_START ' not in text
            damage=[list(map(int,line.split()[1:])) for line in text.splitlines() if line.startswith('CLUTTER_DAMAGE ')]
            assert damage and damage[-1][:4]==[0,0,0,0],damage
            states[name]=saved
        else:
            assert path.exists(),f'Missing fresh checkpoint: {name}'
            states[name]=checkpoint(path)
        from PIL import Image
        Image.open(folder/(name+'.ppm')).save(folder/(name+'.png'))
        expected=80 if name=='baseline' else 40 if name.startswith('damaged') else 0
        assert states[name][2]==expected,states[name]
    report.update(status='STATE_PASS_VISUALS_UNVERIFIED',states=states);(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(states)
if __name__=='__main__':main()
