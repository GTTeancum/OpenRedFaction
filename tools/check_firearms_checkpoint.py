"""Prepare real scripted HMG/Precision acquisition and checkpoint continuation.

Default prepares only. --run explicitly launches process-local PC replay; no host
input or emulator. Fixtures keep CTF06 geometry, all506 props and original pickups.
Only their event set changes to one Give_Item_To_Player event, dispatched on the
initial run alone. Reloads do not regrant or rewrite checkpoint bytes.
"""
import argparse, io, json, os, struct, subprocess
from pathlib import Path
from build_fragment_platform_fixture import read_entry, U, F, S
from inspect_levels import inspect as inspect_level
from check_clutter_checkpoint import checkpoint_sections
ROOT=Path(__file__).resolve().parents[1]
PROFILES={'hmg':('heavy machine gun',14),'precision':('scope assault rifle',15)}
EVENT_UID=910200

def fixture(folder,kind):
    game=folder/kind/'game';game.mkdir(parents=True,exist_ok=True)
    original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    meta=inspect_level(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    item,slot=PROFILES[kind]
    event=U(EVENT_UID)+S(b'Give_Item_To_Player')+F(-2.75,-.4,2.5)+S(b'checkpoint_weapon_grant')
    event+=bytes([0])+F(0)+bytes([1,0])+U(0,0)+F(0,0)+S(item.encode('ascii'))+S(b'')+U(0)+bytes([255]*4)
    level=bytearray(original[:meta['sections'][0]['offset']]);offsets={};replaced=False
    for section in meta['sections']:
        kind_id=int(section['type'],16)
        payload=original[section['offset']+8:section['offset']+8+section['size']]
        if kind_id==0x600:payload=U(1)+event;replaced=True
        offsets[kind_id]=len(level);level+=U(kind_id,len(payload))+payload
    assert replaced,'CTF06 event section expected'
    struct.pack_into('<II',level,12,offsets[0x70000],offsets[0x1000000])
    inspect_level(io.BytesIO(level),dict(offset=0,size=len(level),name='ctf06.rfl'))
    size=4096+((len(level)+2047)&~2047);archive=bytearray(size)
    struct.pack_into('<4I',archive,0,0x51890ace,1,1,size)
    archive[2048:2057]=b'ctf06.rfl';struct.pack_into('<I',archive,2108,len(level));archive[4096:4096+len(level)]=level
    out=game/'levelsm.vpp';assert not out.exists() or out.stat().st_nlink==1;out.write_bytes(archive)
    for source in [*(ROOT/'Installed_Game').glob('*.vpp'),ROOT/'Installed_Game/bluebeard.bty']:
        if source.name.lower()=='levelsm.vpp':continue
        out=game/source.name
        if not out.exists():os.link(source,out)
        else:assert os.path.samefile(source,out)
    return game

def prepare(folder):
    jobs=[]
    for kind in PROFILES:
        game=fixture(folder,kind)
        for phase in ('saved','resumed','continued'):
            base=folder/kind/phase;frames=360 if phase=='saved' else 180;rows=[]
            for frame in range(frames):
                cycle=phase=='saved' and frame in range(12,53,4)
                if phase=='saved':fire=(100<=frame<110) if kind=='hmg' else frame in (100,160)
                elif phase=='continued':fire=(40<=frame<50) if kind=='hmg' else frame in (40,100)
                else:fire=False
                rows.append(struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(fire),0,int(cycle),0))
            base.with_suffix('.bin').write_bytes(b'RFI6'+U(48)+b''.join(rows))
            env=dict(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1')
            if phase=='saved':env.update(RF_REPLAY_SETUP_UID=str(EVENT_UID),RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(base.with_suffix('.rfcp')))
            else:env['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/kind/'saved.rfcp')
            jobs.append(dict(kind=kind,phase=phase,frames=frames,env=env,command=[str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(game),str(base.with_suffix('.bin')),str(base.with_suffix('.ppm'))]))
    report=dict(status='PREPARED_NOT_RUN',jobs=jobs,scope='Real event grant, cycle selection, fire, save; fresh process neutral reload and continued fire. No resave on loaded terrain; no snapshot injection.',limitations='Parent must build HMG/Precision checkpoint support before running. Shot cadence and visual output require live validation; this recipe alone is not evidence.')
    (folder/'recipe.json').write_text(json.dumps(report,indent=2)+'\n');return report

def words(text,key):
    rows=[list(map(int,line.split()[1:])) for line in text.splitlines() if line.startswith(key+' ')]
    assert rows,f'Missing {key}'
    return rows[-1]

def saved_player(path):
    p,_=checkpoint_sections(path.read_bytes())
    weapon=struct.unpack_from('<I',p,20)[0];assert weapon<64 and p[80+weapon]==1
    return dict(weapon=weapon,loaded=struct.unpack_from('<I',p,272+4*weapon)[0])

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');parser.add_argument('--kind',choices=PROFILES);args=parser.parse_args()
    folder=ROOT/'artifacts/firearms-checkpoint-live';folder.mkdir(parents=True,exist_ok=True);report=prepare(folder)
    if not args.run:print(folder/'recipe.json');return
    (folder/'report.json').unlink(missing_ok=True)
    clean={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))};states={}
    for job in report['jobs']:
        kind,phase=job['kind'],job['phase']
        if args.kind and kind!=args.kind:continue
        base=folder/kind/phase
        for suffix in ('.rfcp','.ppm','.png'):base.with_suffix(suffix).unlink(missing_ok=True)
        with base.with_suffix('.log').open('wb') as log:
            result=subprocess.run(job['command'],cwd=ROOT,env=dict(clean,**job['env']),stdout=log,stderr=subprocess.STDOUT,timeout=240)
        assert result.returncode==0,f'Inspect {base}.log'
        text=base.with_suffix('.log').read_text(errors='replace');ammo=words(text,'PLAYER_AMMO');selection=words(text,'WEAPON_SELECTION');combat=words(text,'COMBAT')
        assert selection[0]==PROFILES[kind][1] and ammo[7]==0,(selection,ammo)
        if phase=='saved':
            saved=saved_player(base.with_suffix('.rfcp'));assert [saved['weapon'],saved['loaded']]==[ammo[0],ammo[2]]
            grant=words(text,'SCRIPT_GRANTS');assert grant[0]==1 and combat[0]>0,(grant,combat)
            states[kind]=dict(saved=dict(**saved,reserve=ammo[1],shots=combat[0]))
        else:
            assert 'PLAYER_CHECKPOINT_LOAD ' in text and not base.with_suffix('.rfcp').exists()
            saved=states[kind]['saved'];assert ammo[0]==saved['weapon'] and ammo[1]==saved['reserve'],ammo
            assert words(text,'SCRIPT_GRANTS')[0]==0,'Reload must not regrant ammunition'
            if phase=='resumed':assert ammo[2]==saved['loaded'] and combat[0]==0,(ammo,combat)
            else:assert 0<=ammo[2]<saved['loaded'] and combat[0]>0,(ammo,combat)
            states[kind][phase]=dict(weapon=ammo[0],loaded=ammo[2],reserve=ammo[1],shots=combat[0])
        from PIL import Image
        Image.open(base.with_suffix('.ppm')).save(base.with_suffix('.png'))
        print(kind,phase,ammo[:3],combat[0],flush=True)
    report.update(status='STATE_PASS_VISUALS_UNVERIFIED',states=states);(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
