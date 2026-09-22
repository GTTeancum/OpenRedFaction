"""Prepare non-DEV flamethrower/Napalm acquisition, stream and canister use.

No forced weapon, DEV supply, camera override or checkpoint. CTF06 retains its
original player-start section, geometry,506 props and pickups; its event set is
replaced by one real Give_Item_To_Player event. Default prepares only.
"""
import argparse,io,json,os,re,struct,subprocess
from pathlib import Path
from build_fragment_platform_fixture import read_entry,U,F,S
from inspect_levels import inspect as inspect_level
ROOT=Path(__file__).resolve().parents[1]
PROFILES={'flame':('flamethrower','flamethrower',10)}

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
        napalm=next(b for b in blocks if re.match(r'\$Class Name:\s*"Napalm"',b,re.I))
        assert re.search(r'\$Ammo For:\s*"flamethrower"',napalm,re.I)
        events=[]
        for uid,name in [(910540,item),(910541,'Napalm')]:
            event=U(uid)+S(b'Give_Item_To_Player')+F(*spawn)+S(b'flame_supply')
            event+=bytes([0])+F(0)+bytes([1,0])+U(0,0)+F(0,0)+S(name.encode())+S(b'')+U(0)+bytes([255]*4)
            events.append(event)
        level=bytearray(original[:meta['sections'][0]['offset']]);offsets={};replaced=False
        for section in meta['sections']:
            typ=int(section['type'],16);payload=original[section['offset']+8:section['offset']+8+section['size']]
            if typ==0x600:payload=U(2)+b''.join(events);replaced=True
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
        for phase in ('acquired','stream','canister'):
            base=folder/kind/phase
            frames=360 if phase=='canister' else 180
            rows=[struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(phase=='stream' and 120<=f<180),0,int(f in range(30,30+4*cycles,4)),int(phase=='canister' and f==120)) for f in range(frames)]
            base.with_suffix('.bin').write_bytes(b'RFI6'+U(48)+b''.join(rows))
            jobs.append(dict(kind=kind,phase=phase,item=item,quantity=quantity,slot=slot,frames=frames,env=dict(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_SETUP_UID='910540,910541'),command=[str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(game),str(base.with_suffix('.bin')),str(base.with_suffix('.ppm'))]))
    report=dict(status='PREPARED_NOT_RUN',jobs=jobs,spawn=spawn,cycles=cycles,schedule='Weapon grant0,Napalm60; ordinary cycle30; streamhold120..179 with6tick ignition; canisteralternate120/release228 after108ticks,finish359.',scope='Non-DEV actual flamethrower/Napalm grants, finitegas stream, thrown canister and reserve-tank replacement; preserve originalspawn/fullgeometry506props/pickups.',limitations='PC --spawn-replay calls set_campaign_spawn using original player section because DEV flag absent. Assumes only default handgun and granted target owned: onecycle selects10. Nearby original pickups could alter cycling or supply; --cycles allows explicit adjustment after live evidence. Existing item metadata can demand additional views without granting ownership. Installed ignition0.1s and alternate release1.8s; canister contact-explodes or expiresafter10s, so360framephase provesrelease but only reports explosion if observed. Alternate replaces thrown100unit tank from reserve rather than subtracting one unit. Resources and actual rendering require parent validation.')
    (folder/'recipe.json').write_text(json.dumps(report,indent=2)+'\n');return report

def words(text,key):
    rows=[list(map(int,l.split()[1:])) for l in text.splitlines() if l.startswith(key+' ')];assert rows,key;return rows[-1]

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');parser.add_argument('--kind',choices=PROFILES);parser.add_argument('--cycles',type=int,default=1);args=parser.parse_args()
    assert 1<=args.cycles<=16
    folder=ROOT/'artifacts/flame-pickup-live';folder.mkdir(parents=True,exist_ok=True);report=prepare(folder,args.cycles)
    if not args.run:print(folder/'recipe.json');return
    (folder/'report.json').unlink(missing_ok=True);states={};clean={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    for job in report['jobs']:
        kind,phase=job['kind'],job['phase']
        if args.kind and args.kind!=kind:continue
        base=folder/kind/phase
        for suffix in ('.ppm','.png'):base.with_suffix(suffix).unlink(missing_ok=True)
        with base.with_suffix('.log').open('wb') as log:run=subprocess.run(job['command'],cwd=ROOT,env=dict(clean,**job['env']),stdout=log,stderr=subprocess.STDOUT,timeout=240)
        assert run.returncode==0,f'Inspect {base}.log'
        text=base.with_suffix('.log').read_text(errors='replace');grant=words(text,'SCRIPT_GRANTS');selection=words(text,'WEAPON_SELECTION');ammo=words(text,'PLAYER_AMMO');combat=words(text,'COMBAT');visual=words(text,'FLAME_VISUAL');canister=words(text,'FLAME_CANISTER')
        assert grant[0]==2 and grant[7]==0 and selection[0]==job['slot'] and ammo[7]==0,(grant,selection,ammo)
        assert visual[5]==0 and canister[4]==0,(visual,canister)
        if phase=='acquired':
            assert ammo[1:3]==[100,100] and combat[0]==0,(ammo,combat)
            states[kind]={'acquired':dict(ammo=ammo,combat=combat)}
        else:
            baseline=states[kind]['acquired']['ammo'];assert ammo[0]==baseline[0]
            if phase=='stream':
                assert ammo[1]==baseline[1] and 0<ammo[2]<baseline[2] and combat[0]>0,(ammo,combat)
                assert visual[0]>0 and visual[1]>0 and canister[0:2]==[0,0],(visual,canister)
            else:
                assert ammo[1]==0 and ammo[2]==100 and canister[0:2]==[1,1],(ammo,canister)
            states[kind][phase]=dict(ammo=ammo,combat=combat,flame_visual=visual,canister=canister,canister_explosion_observed=canister[2]>0)
        from PIL import Image
        Image.open(base.with_suffix('.ppm')).save(base.with_suffix('.png'));print(kind,phase,ammo[:3],flush=True)
    report.update(status='STATE_PASS_VISUALS_UNVERIFIED',states=states);(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
