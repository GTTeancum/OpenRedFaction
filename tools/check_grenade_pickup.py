"""Prepare non-DEV grenade event acquisition and ordinary firing.

No forced weapon, DEV supply, camera override or checkpoint. Six hundred frames
cover102tick primary windup plus300tick authored fuse;360 would end too early. CTF06 retains its
original player-start section, geometry,506 props and pickups; its event set is
replaced by one real Give_Item_To_Player event. Default prepares only.
"""
import argparse,io,json,os,re,struct,subprocess
from pathlib import Path
from build_fragment_platform_fixture import read_entry,U,F,S
from inspect_levels import inspect as inspect_level
ROOT=Path(__file__).resolve().parents[1]
PROFILES={'grenade':('grenades','Grenade',5)}

def prepare(folder,cycles):
    table=read_entry(ROOT/'Installed_Game/tables.vpp','items.tbl').decode('cp1252')
    blocks=re.split(r'(?=\$Class Name:)',table);jobs=[]
    original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    meta=inspect_level(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    entities=next((s for s in meta['sections'] if s['type']=='0x30000'),None)
    assert not entities or struct.unpack_from('<I',original,entities['offset']+8)[0]==0,'Expected enemy-free CTF06'
    start=next(s for s in meta['sections'] if s['type']=='0x70000')
    spawn=struct.unpack_from('<3f',original,start['offset']+8)
    for kind,(item,weapon,slot) in PROFILES.items():
        definition=next(b for b in blocks if re.match(r'\$Class Name:\s*"'+re.escape(item)+'"',b,re.I))
        assert re.search(r'\$Gives Weapon:\s*"'+re.escape(weapon)+'"',definition,re.I)
        quantity=int(re.search(r'\$Count:\s*(\d+)',definition).group(1))
        game=folder/kind/'game';game.mkdir(parents=True,exist_ok=True)
        event=U(910520)+S(b'Give_Item_To_Player')+F(*spawn)+S(b'grenade_supply')
        event+=bytes([0])+F(0)+bytes([1,0])+U(0,0)+F(0,0)+S(item.encode())+S(b'')+U(0)+bytes([255]*4)
        level=bytearray(original[:meta['sections'][0]['offset']]);offsets={};replaced=False
        for section in meta['sections']:
            typ=int(section['type'],16);payload=original[section['offset']+8:section['offset']+8+section['size']]
            if typ==0x600:payload=U(1)+event;replaced=True
            offsets[typ]=len(level);level+=U(typ,len(payload))+payload
        assert replaced;struct.pack_into('<II',level,12,offsets[0x70000],offsets[0x1000000])
        inspect_level(io.BytesIO(level),dict(offset=0,size=len(level),name='ctf06.rfl'))
        size=4096+((len(level)+2047)&~2047);archive=bytearray(size);struct.pack_into('<4I',archive,0,0x51890ace,1,1,size)
        archive[2048:2057]=b'ctf06.rfl';struct.pack_into('<I',archive,2108,len(level));archive[4096:4096+len(level)]=level
        out=game/'levelsm.vpp';assert not out.exists() or out.stat().st_nlink==1;out.write_bytes(archive)
        for source in [*(ROOT/'Installed_Game').glob('*.vpp'),ROOT/'Installed_Game/bluebeard.bty']:
            if source.name.lower()=='levelsm.vpp':continue
            out=game/source.name
            if out.exists():assert os.path.samefile(source,out)
            else:os.link(source,out)
        for phase in ('acquired','fired'):
            base=folder/kind/phase
            rows=[struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(phase=='fired' and f==120),0,int(f in range(30,30+4*cycles,4)),0) for f in range(600)]
            base.with_suffix('.bin').write_bytes(b'RFI6'+U(48)+b''.join(rows))
            jobs.append(dict(kind=kind,phase=phase,item=item,quantity=quantity,slot=slot,env=dict(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_SETUP_UID='910520'),command=[str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(game),str(base.with_suffix('.bin')),str(base.with_suffix('.ppm'))]))
    report=dict(status='PREPARED_NOT_RUN',jobs=jobs,spawn=spawn,cycles=cycles,schedule='Give_Item0; ordinary cycle edges from30 every4frames; one primary request120, primary release after102ticks at222, authored5second fuse throughapproximately522; finish599.',scope='Non-DEV real grenades item grant, finite supply, primary throw/release and timed detonation; preserve original spawn/pickups/fullgeometry506props.',limitations='PC --spawn-replay calls set_campaign_spawn using original player section because DEV flag absent. Assumes only default handgun and granted target owned: onecycle selects5. Nearby original pickups could alter cycling or supply; --cycles allows explicit adjustment after live evidence. Existing item metadata can demand additional views without granting ownership. Resources and actual rendering require parent validation.')
    (folder/'recipe.json').write_text(json.dumps(report,indent=2)+'\n');return report

def words(text,key):
    rows=[list(map(int,l.split()[1:])) for l in text.splitlines() if l.startswith(key+' ')];assert rows,key;return rows[-1]

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');parser.add_argument('--kind',choices=PROFILES);parser.add_argument('--cycles',type=int,default=1);args=parser.parse_args()
    assert 1<=args.cycles<=16
    folder=ROOT/'artifacts/grenade-pickup-live';folder.mkdir(parents=True,exist_ok=True);report=prepare(folder,args.cycles)
    if not args.run:print(folder/'recipe.json');return
    (folder/'report.json').unlink(missing_ok=True);states={};clean={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    for job in report['jobs']:
        kind,phase=job['kind'],job['phase']
        if args.kind and args.kind!=kind:continue
        base=folder/kind/phase
        for suffix in ('.ppm','.png'):base.with_suffix(suffix).unlink(missing_ok=True)
        with base.with_suffix('.log').open('wb') as log:run=subprocess.run(job['command'],cwd=ROOT,env=dict(clean,**job['env']),stdout=log,stderr=subprocess.STDOUT,timeout=240)
        assert run.returncode==0,f'Inspect {base}.log'
        text=base.with_suffix('.log').read_text(errors='replace');grant=words(text,'SCRIPT_GRANTS');selection=words(text,'WEAPON_SELECTION');ammo=words(text,'PLAYER_AMMO');combat=words(text,'COMBAT');grenades=words(text,'GRENADES')
        assert grant[0]==1 and grant[7]==0 and selection[0]==job['slot'] and ammo[7]==0,(grant,selection,ammo)
        assert grenades[5]==0 and grenades[7]==0,grenades
        if phase=='acquired':
            assert ammo[2]>0 and combat[0]==0 and grenades[0]==0,(ammo,combat,grenades)
            states[kind]={'acquired':dict(ammo=ammo,combat=combat,grenades=grenades)}
        else:
            baseline=states[kind]['acquired']['ammo'];assert ammo[0]==baseline[0] and ammo[1]==baseline[1]-1 and ammo[2]==baseline[2]-1 and combat[0]==1,(ammo,combat)
            assert grenades[0:2]==[1,1] and grenades[3]==1 and grenades[4]==0,grenades
            states[kind]['fired']=dict(ammo=ammo,combat=combat,grenades=grenades,detonation_observed=grenades[3]==1)
        from PIL import Image
        Image.open(base.with_suffix('.ppm')).save(base.with_suffix('.png'));print(kind,phase,ammo[:3],flush=True)
    report.update(status='STATE_PASS_VISUALS_UNVERIFIED',states=states);(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
