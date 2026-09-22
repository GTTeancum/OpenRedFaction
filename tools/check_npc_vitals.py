"""Prepare non-DEV authored NPC vitals and real handgun damage checks.

Guard8456's documented raw health/armor fields alone change. Same original
position, model, loadout and geometry, ordinary actor staging and player fire.
No damage injection, DEV flags, original executable or runtime NPC relocation.
Default prepares only; --run opts into reconstructed PC replay.
"""
import argparse,io,json,os,struct,subprocess
from pathlib import Path
from check_ai_projectile_ordinary import archive,records,words
from build_fragment_platform_fixture import read_entry,U
from inspect_levels import inspect as inspect_level
ROOT=Path(__file__).resolve().parents[1]

def vitals(raw,health,armor):
    data=bytearray(raw);at=4
    def string():
        nonlocal at
        n=struct.unpack_from('<H',data,at)[0];at+=2+n;assert at<=len(data)
    string();at+=48;string();at+=13;string();string()
    # Confirmed464010:29byteblock sixbytes,twoangles,threebytes,health,armor,FOV.
    # health17/armor21 arefloat32; -1inherits, otherfinitevaluesclamp[0,classmax].
    struct.pack_into('<ff',data,at+17,health,armor);return bytes(data)

def prepare(folder):
    source=read_entry(ROOT/'Installed_Game/levels1.vpp','L1S1.rfl');meta=inspect_level(io.BytesIO(source),dict(offset=0,size=len(source),name='L1S1.rfl'))
    sec=next(s for s in meta['sections'] if s['type']=='0x30000');kept=[r for r in records(source[sec['offset']+8:sec['offset']+8+sec['size']]) if r[0] in (8456,9858)];assert {r[0] for r in kept}=={8456,9858}
    jobs=[]
    for name,health,armor in [('fragile',20,0),('healthy',80,0),('armored',80,40)]:
        game=folder/name/'game';game.mkdir(parents=True,exist_ok=True)
        entity=U(len(kept))+b''.join(vitals(r[2],health,armor) if r[0]==8456 else r[2] for r in kept)
        level=bytearray(source[:meta['sections'][0]['offset']]);offsets={}
        for sec in meta['sections']:
            kind=int(sec['type'],16);data=source[sec['offset']+8:sec['offset']+8+sec['size']]
            if kind==0x30000:data=entity
            elif kind==0x600:data=U(0)
            offsets[kind]=len(level);level+=U(kind,len(data))+data
        struct.pack_into('<II',level,12,offsets[0x70000],offsets[0x1000000]);inspect_level(io.BytesIO(level),dict(offset=0,size=len(level),name='L1S1.rfl'));archive(game/'levels1.vpp',[('L1S1.rfl',level)])
        for src in [*(ROOT/'Installed_Game').glob('*.vpp'),ROOT/'Installed_Game/bluebeard.bty']:
            if src.name.lower()=='levels1.vpp':continue
            dst=game/src.name
            if dst.exists():assert os.path.samefile(src,dst)
            else:os.link(src,dst)
        base=folder/name/'shot';rows=[struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(f==30),0,0,0) for f in range(90)];base.with_suffix('.bin').write_bytes(b'RFI6'+U(48)+b''.join(rows))
        jobs.append(dict(name=name,health=health,armor=armor,env=dict(RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp',RF_REPLAY_ACTOR_UID='8456'),command=[str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(game),str(base.with_suffix('.bin')),str(base.with_suffix('.ppm'))]))
    report=dict(status='PREPARED_NOT_RUN',jobs=jobs,scope='Sameguard actual singlehandgunhit compares lowhealthdeath, higherhealthsurvival and armorabsorption, exercising authoredspawnvitals plus live damage.',limitations='NPC creation applies authored vitals before persistence restoration. Fixture guard class capacities are 100 health and 100 armor. COMBAT[1] actualactorhits andCOMBAT[4] lastvictimhealth suffice fordurability withoneNPC; no additional telemetry required for thiscomparison. Startingarmor exactvalue is notobserved; minimumoptionaltelemetry wouldbeUID,createdhealth,createdarmor aftervitalassignment ifdirectspawnreadbackneeded. Aim and survivalthroughframe89 require livecheck; misses fail, never substituted by directdamage.')
    (folder/'recipe.json').write_text(json.dumps(report,indent=2)+'\n');return report

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');args=parser.parse_args();folder=ROOT/'artifacts/npc-vitals-live';folder.mkdir(parents=True,exist_ok=True);report=prepare(folder)
    if not args.run:print(folder/'recipe.json');return
    (folder/'report.json').unlink(missing_ok=True);states={};clean={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    for job in report['jobs']:
        name=job['name'];base=folder/name/'shot'
        for suffix in ('.ppm','.png'):base.with_suffix(suffix).unlink(missing_ok=True)
        with base.with_suffix('.log').open('wb') as log:run=subprocess.run(job['command'],cwd=ROOT,env=dict(clean,**job['env']),stdout=log,stderr=subprocess.STDOUT,timeout=240)
        assert run.returncode==0,f'Inspect {base}.log'
        text=base.with_suffix('.log').read_text(errors='replace');combat=words(text,'COMBAT');assert combat[0:2]==[1,1] and combat[3]!=0xffffffff,combat
        health=struct.unpack('<f',struct.pack('<I',combat[4]))[0];states[name]=dict(health_after_hit=health,combat=combat)
        from PIL import Image
        Image.open(base.with_suffix('.ppm')).save(base.with_suffix('.png'));print(name,health,flush=True)
    assert states['fragile']['health_after_hit']<=0
    assert 0<states['healthy']['health_after_hit']<80
    assert states['healthy']['health_after_hit']<states['armored']['health_after_hit']<=80
    report.update(status='DURABILITY_PASS_VISUALS_UNVERIFIED',states=states);(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
