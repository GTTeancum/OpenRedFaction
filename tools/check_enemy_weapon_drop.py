"""Prepare ordinary enemy corpse weapon acquisition; default does not run.

Uses the confirmed Slay_Object event path (real damage/death/drop publication),
then normal forward movement from actor staging to collect the physical drop.
No inventory or persistent-drop injection and no DEV flags. Guard8456's original
record receives an explicit per-instance Rocket Launcher override; all class
metadata and level geometry remain original. The weapon is then fired once to verify the acquired inventory is usable.
"""
import argparse,io,json,os,struct,subprocess
from pathlib import Path
from check_ai_projectile_ordinary import archive,records,loadout,words
from build_fragment_platform_fixture import read_entry,U,F,S
from inspect_levels import inspect as inspect_level
ROOT=Path(__file__).resolve().parents[1]

def prepare(folder,move_frames):
    game=folder/'game';game.mkdir(parents=True,exist_ok=True)
    original=read_entry(ROOT/'Installed_Game/levels1.vpp','L1S1.rfl');meta=inspect_level(io.BytesIO(original),dict(offset=0,size=len(original),name='L1S1.rfl'))
    section=next(s for s in meta['sections'] if s['type']=='0x30000');rows=records(original[section['offset']+8:section['offset']+8+section['size']]);kept=[r for r in rows if r[0] in (8456,9858)]
    assert {r[0] for r in kept}=={8456,9858}
    event=U(910600)+S(b'Slay_Object')+F(0,0,0)+S(b'corpse_drop_death')
    # Delay the real death callback past frame-zero diagnostic initialization;
    # retain the assertion that exactly one drop publication is counted.
    event+=bytes([0])+F(2/60)+bytes([1,0])+U(0,0)+F(0,0)+S(b'')+S(b'')+U(1,8456)+bytes([255]*4)
    entity_data=U(len(kept))+b''.join(loadout(r[2],'Rocket Launcher') if r[0]==8456 else r[2] for r in kept)
    level=bytearray(original[:meta['sections'][0]['offset']]);offsets={}
    for section in meta['sections']:
        kind=int(section['type'],16);data=original[section['offset']+8:section['offset']+8+section['size']]
        if kind==0x30000:data=entity_data
        elif kind==0x600:data=U(1)+event
        offsets[kind]=len(level);level+=U(kind,len(data))+data
    struct.pack_into('<II',level,12,offsets[0x70000],offsets[0x1000000]);inspect_level(io.BytesIO(level),dict(offset=0,size=len(level),name='L1S1.rfl'))
    archive(game/'levels1.vpp',[('L1S1.rfl',level)])
    for source in [*(ROOT/'Installed_Game').glob('*.vpp'),ROOT/'Installed_Game/bluebeard.bty']:
        if source.name.lower()=='levels1.vpp':continue
        dest=game/source.name
        if dest.exists():assert os.path.samefile(source,dest)
        else:os.link(source,dest)
    jobs=[]
    for phase,frames in [('emitted',90),('collected',240),('fired',300)]:
        base=folder/phase
        inputs=[struct.pack('<5f7I',0,0,float(phase!='emitted' and 90<=f<90+move_frames),0,0,0,0,0,int(phase=='fired' and f==240),0,int(phase!='emitted' and f==180),0) for f in range(frames)]
        base.with_suffix('.bin').write_bytes(b'RFI6'+U(48)+b''.join(inputs))
        env=dict(RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp',RF_REPLAY_ACTOR_UID='8456',RF_REPLAY_SETUP_UID='910600,910600')
        jobs.append(dict(phase=phase,frames=frames,env=env,command=[str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(game),str(base.with_suffix('.bin')),str(base.with_suffix('.ppm'))]))
    report=dict(status='PREPARED_NOT_RUN',jobs=jobs,scope='Controlled original guard killed via normal scripted damage/death callback, one persistent emitted weapon, ordinary movement pickup and selection; repeat Slay request must not manufacture another drop.',schedule=f'Slay requests0/60 with2tick event delay; emitted endpoint89; collection forwards90..{89+move_frames},cycle180,endpoint239; fire240,endpoint299.',limitations='Actor staging starts3units forward of guard and faces it;45forwardframes is a bounded movement assumption, collection radius2. Floor projection, collision, movement direction, weapon resource demand and actual grant must be validated live. No fake persisted record or player inventory. Scripted death tests pickup wiring, not player gun combat lethality.')
    (folder/'recipe.json').write_text(json.dumps(report,indent=2)+'\n');return report

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');parser.add_argument('--move-frames',type=int,default=45);args=parser.parse_args();assert 1<=args.move_frames<=80
    folder=ROOT/'artifacts/enemy-weapon-drop';folder.mkdir(parents=True,exist_ok=True);report=prepare(folder,args.move_frames)
    if not args.run:print(folder/'recipe.json');return
    (folder/'report.json').unlink(missing_ok=True);states={};clean={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    for job in report['jobs']:
        phase=job['phase'];base=folder/phase
        for suffix in ('.ppm','.png'):base.with_suffix(suffix).unlink(missing_ok=True)
        with base.with_suffix('.log').open('wb') as log:run=subprocess.run(job['command'],cwd=ROOT,env=dict(clean,**job['env']),stdout=log,stderr=subprocess.STDOUT,timeout=240)
        assert run.returncode==0,f'Inspect {base}.log'
        text=base.with_suffix('.log').read_text(errors='replace');drops=words(text,'WEAPON_DROPS');slay=words(text,'SCRIPT_SLAYS');ammo=words(text,'PLAYER_AMMO');selection=words(text,'WEAPON_SELECTION')
        assert slay[1]==1 and slay[2]==8456 and slay[5]==0 and drops[0]==1 and drops[7]==0,(slay,drops)
        assert words(text,'COMBAT')[0]==int(phase=='fired'),'Guard dies from script; only final phase fires the collected gun'
        if phase=='emitted':assert drops[1]==0 and drops[4]==1 and selection[0]==0,(drops,selection)
        else:assert drops[1]==1 and drops[2]>0 and drops[3]==8456 and drops[4]==0 and selection[0]==4 and ammo[2]==drops[2]-int(phase=='fired'),(drops,selection,ammo)
        if phase=='fired':assert words(text,'ROCKETS')[0]>0,words(text,'ROCKETS')
        states[phase]=dict(drops=drops,slay=slay,ammo=ammo,selection=selection)
        from PIL import Image
        Image.open(base.with_suffix('.ppm')).save(base.with_suffix('.png'));print(phase,drops,ammo[:3],flush=True)
    report.update(status='STATE_PASS_VISUALS_UNVERIFIED',states=states);(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
