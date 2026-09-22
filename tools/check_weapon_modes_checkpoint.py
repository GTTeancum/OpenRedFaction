"""Prepare RFCP5 Machine Pistol/Undercover mode persistence; never launch by default.

MP uses real Give_Item_To_Player Machine Pistol and5.56mm_ammo events, then normal
mode toggle and reload. Undercover has no installed item class, so uses explicit
DEV firearms4 supply. Both retain CTF06 geometry and506 authored props. No saved
bytes or inventory are injected. Parent owns execution after its build is ready.
"""
import argparse, io, json, os, struct, subprocess
from pathlib import Path
from build_fragment_platform_fixture import read_entry,U,F,S
from inspect_levels import inspect as inspect_level
ROOT=Path(__file__).resolve().parents[1]

def fixture(folder,kind):
    game=folder/kind/'game';game.mkdir(parents=True,exist_ok=True)
    if kind=='machine':
        original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
        meta=inspect_level(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
        events=[]
        for uid,item in [(910300,b'Machine Pistol'),(910301,b'5.56mm_ammo')]:
            event=U(uid)+S(b'Give_Item_To_Player')+F(-2.75,-.4,2.5)+S(b'weapon_mode_supply')
            event+=bytes([0])+F(0)+bytes([1,0])+U(0,0)+F(0,0)+S(item)+S(b'')+U(0)+bytes([255]*4);events.append(event)
        level=bytearray(original[:meta['sections'][0]['offset']]);offsets={};replaced=False
        for section in meta['sections']:
            typ=int(section['type'],16);payload=original[section['offset']+8:section['offset']+8+section['size']]
            if typ==0x600:payload=U(2)+b''.join(events);replaced=True
            offsets[typ]=len(level);level+=U(typ,len(payload))+payload
        assert replaced
        struct.pack_into('<II',level,12,offsets[0x70000],offsets[0x1000000])
        inspect_level(io.BytesIO(level),dict(offset=0,size=len(level),name='ctf06.rfl'))
        size=4096+((len(level)+2047)&~2047);archive=bytearray(size);struct.pack_into('<4I',archive,0,0x51890ace,1,1,size)
        archive[2048:2057]=b'ctf06.rfl';struct.pack_into('<I',archive,2108,len(level));archive[4096:4096+len(level)]=level
        out=game/'levelsm.vpp';assert not out.exists() or out.stat().st_nlink==1;out.write_bytes(archive)
    for source in [*(ROOT/'Installed_Game').glob('*.vpp'),ROOT/'Installed_Game/bluebeard.bty']:
        if kind=='machine' and source.name.lower()=='levelsm.vpp':continue
        out=game/source.name
        if not out.exists():os.link(source,out)
        else:assert os.path.samefile(source,out)
    return game

def prepare(folder):
    jobs=[]
    for kind in ('machine','undercover'):
        game=fixture(folder,kind)
        for phase in ('saved','resumed','continued'):
            frames=600 if phase=='saved' else 180;base=folder/kind/phase;rows=[]
            for frame in range(frames):
                initial=phase=='saved'
                cycle=kind=='machine' and initial and frame in range(12,53,4)
                alternate=initial and frame==120
                reload=kind=='machine' and initial and frame==260
                fire=(80<=frame<84 or 420<=frame<424) if initial else (phase=='continued' and 40<=frame<44)
                rows.append(struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(fire),int(reload),int(cycle),int(alternate)))
            base.with_suffix('.bin').write_bytes(b'RFI6'+U(48)+b''.join(rows))
            env=dict(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1')
            if kind=='undercover':env['RF_REPLAY_FIREARMS']='4'
            if phase=='saved':
                env['RF_REPLAY_GEOMOD_CHECKPOINT_OUT']=str(base.with_suffix('.rfcp'))
                if kind=='machine':env['RF_REPLAY_SETUP_UID']='910300,910301'
            else:env['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/kind/'saved.rfcp')
            jobs.append(dict(kind=kind,phase=phase,frames=frames,env=env,command=[str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(game),str(base.with_suffix('.bin')),str(base.with_suffix('.ppm'))]))
    report=dict(status='PREPARED_NOT_RUN',jobs=jobs,scope='RFCP5 mode/selected ownership and independent magazines, live reload and continued fire; PC reload phases omit resave; the native harness separately checks load/fire/resave.',timing='MP normal cycle12..52,base fire80..83,toggle120,authored transition114ticks ends234,reload260,special fire420..423,save599; Undercover already selected16,attach120,fire420..423; reload180neutral orfire40..43.',limitations='Prepared only until parent opts into run. Undercover actual attach timing and output require inspection. MP5.56mm_ammo event dispatch60, normal DEV AR reserve also shares AP ammo. Full DEV loadout is intentional only for Undercover.')
    (folder/'recipe.json').write_text(json.dumps(report,indent=2)+'\n');return report

def words(text,key):
    values=[list(map(int,line.split()[1:])) for line in text.splitlines() if line.startswith(key+' ')];assert values,key;return values[-1]

def saved(path):
    data=path.read_bytes();header=struct.unpack_from('<4s7I',data);assert header[:3]==(b'RFCP',5,len(data)) and header[5]==544,header
    tail=data[-32:];magic,version,size,checksum,catalog,flags,rng,reserved=struct.unpack('<4s7I',tail)
    assert (magic,version,size,reserved)==(b'RFWM',1,32,0)
    computed=2166136261
    for i,value in enumerate(tail):computed=((computed^(0 if 12<=i<16 else value))*16777619)&0xffffffff
    assert checksum==computed
    player=data[32:576];assert player[:4]==b'RFPL'
    # RFWM uses composed remote-catalog identity, which also hashes remote
    # definitions; it is deliberately distinct from RFPL's weapon-supply hash.
    return dict(flags=flags,rng=rng,catalog=catalog,weapon=struct.unpack_from('<I',player,20)[0],loaded=list(struct.unpack_from('<64I',player,272)),owned=list(player[80:144]))

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');parser.add_argument('--kind',choices=('machine','undercover'));args=parser.parse_args()
    folder=ROOT/'artifacts/weapon-modes-checkpoint-live';folder.mkdir(parents=True,exist_ok=True);report=prepare(folder)
    if not args.run:print(folder/'recipe.json');return
    (folder/'report.json').unlink(missing_ok=True)
    clean={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))};states={}
    for job in report['jobs']:
        kind,phase=job['kind'],job['phase']
        if args.kind and kind!=args.kind:continue
        base=folder/kind/phase
        for suffix in ('.rfcp','.ppm','.png'):base.with_suffix(suffix).unlink(missing_ok=True)
        with base.with_suffix('.log').open('wb') as log:
            result=subprocess.run(job['command'],cwd=ROOT,env=dict(clean,**job['env']),stdout=log,stderr=subprocess.STDOUT,timeout=300)
        assert result.returncode==0,f'Inspect {base}.log'
        text=base.with_suffix('.log').read_text(errors='replace');ammo=words(text,'PLAYER_AMMO');selection=words(text,'WEAPON_SELECTION');combat=words(text,'COMBAT')
        mode=words(text,'MACHINE_MODE' if kind=='machine' else 'UNDERCOVER')
        assert selection[0]==(13 if kind=='machine' else 16) and mode[0:2]==[1,0] and mode[7]==0 and ammo[7]==0,(selection,mode,ammo)
        if phase=='saved':
            value=saved(base.with_suffix('.rfcp'));assert value['flags']==(1 if kind=='machine' else 2),value
            assert value['owned'][value['weapon']]==1 and ammo[2]==value['loaded'][ammo[0]] and combat[0]>0
            if kind=='machine':assert value['weapon']!=ammo[0] and value['loaded'][value['weapon']]>0 and mode[2]==ammo[0] and mode[5:7]==[1,1],(value,mode)
            else:assert value['weapon']==ammo[0] and mode[3:5]==[1,1],mode
            states[kind]=dict(saved=dict(**value,active_weapon=ammo[0],active_loaded=ammo[2],reserve=ammo[1],shots=combat[0]))
        else:
            assert 'PLAYER_CHECKPOINT_LOAD ' in text and not base.with_suffix('.rfcp').exists()
            previous=states[kind]['saved'];assert ammo[0]==previous['active_weapon'] and ammo[1]==previous['reserve'],ammo
            assert words(text,'SCRIPT_GRANTS')[0]==0
            assert mode[5:7]==[0,0] if kind=='machine' else mode[3:5]==[0,0],mode
            if phase=='resumed':assert ammo[2]==previous['active_loaded'] and combat[0]==0,(ammo,combat)
            else:assert 0<=ammo[2]<previous['active_loaded'] and combat[0]>0,(ammo,combat)
            states[kind][phase]=dict(active_weapon=ammo[0],loaded=ammo[2],reserve=ammo[1],mode=mode[0],shots=combat[0])
        from PIL import Image
        Image.open(base.with_suffix('.ppm')).save(base.with_suffix('.png'));print(kind,phase,ammo[:3],mode[0],flush=True)
    report.update(status='STATE_PASS_VISUALS_UNVERIFIED',states=states);(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
