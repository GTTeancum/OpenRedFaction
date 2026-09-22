"""Prepare real pistol-hit -> When_Hit -> Goal_Set, without runtime shortcuts.
Originalguard8456 staysat authoredpose/vitals/loadout onL1S1 geometry. Goal_Create
initializeshit_seen0; observer links guardandGoal_Set. No event is manuallyfired.
Default preparesonly; parent mayrun --run againstreconstructedPC.
"""
import argparse,io,json,os,struct,subprocess
from pathlib import Path
from check_ai_projectile_ordinary import archive,records,words
from build_fragment_platform_fixture import read_entry,U,F,S
from inspect_levels import inspect as inspect_level
ROOT=Path(__file__).resolve().parents[1]

def event(uid,kind,name,links=(),text='',flags=(0,0)):
    data=U(uid)+S(kind.encode())+F(0,0,0)+S(name.encode())
    return data+bytes([0])+F(0)+bytes(flags)+U(0,0)+F(0,0)+S(text.encode())+S(b'')+U(len(links),*links)+bytes([255]*4)

def prepare(folder):
    game=folder/'game';game.mkdir(parents=True,exist_ok=True)
    raw=read_entry(ROOT/'Installed_Game/levels1.vpp','L1S1.rfl');meta=inspect_level(io.BytesIO(raw),dict(offset=0,size=len(raw),name='L1S1.rfl'))
    sec=next(s for s in meta['sections'] if s['type']=='0x30000');kept=[r for r in records(raw[sec['offset']+8:sec['offset']+8+sec['size']]) if r[0] in (8456,9858)];assert {r[0] for r in kept}=={8456,9858}
    events=[event(910800,'Goal_Create','hit_seen'),event(910801,'Goal_Set','hit_output',text='hit_seen'),event(910802,'When_Hit','guard_hit',(8456,910801))]
    level=bytearray(raw[:meta['sections'][0]['offset']]);offsets={}
    for sec in meta['sections']:
        kind=int(sec['type'],16);data=raw[sec['offset']+8:sec['offset']+8+sec['size']]
        if kind==0x30000:data=U(len(kept))+b''.join(r[2] for r in kept)
        elif kind==0x600:data=U(3)+b''.join(events)
        offsets[kind]=len(level);level+=U(kind,len(data))+data
    struct.pack_into('<II',level,12,offsets[0x70000],offsets[0x1000000]);inspect_level(io.BytesIO(level),dict(offset=0,size=len(level),name='L1S1.rfl'));archive(game/'levels1.vpp',[('L1S1.rfl',level)])
    for src in [*(ROOT/'Installed_Game').glob('*.vpp'),ROOT/'Installed_Game/bluebeard.bty']:
        if src.name.lower()=='levels1.vpp':continue
        dst=game/src.name
        if dst.exists():assert os.path.samefile(src,dst)
        else:os.link(src,dst)
    jobs=[]
    for name in ('baseline','hit'):
        base=folder/name;data=[struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(name=='hit' and f==30),0,0,0) for f in range(90)]
        base.with_suffix('.bin').write_bytes(b'RFI6'+U(48)+b''.join(data));env=dict(RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp',RF_REPLAY_ACTOR_UID='8456')
        jobs.append(dict(name=name,env=env,command=[str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(game),str(base.with_suffix('.bin')),str(base.with_suffix('.ppm'))]))
    report=dict(status='PREPARED_NOT_RUN',jobs=jobs,scope='Confirmed runtimeWhen_Hit nonconsumingactorhitflag poll andmixedactor/outputlinks; Goal_Set incrementsdeclaredhit_seen. Actualpistolproducer, noflaginjection or directactivation.',expected='Baseline COMBATshots/hits0 andMISSION_GOALhit_seen0; hitrunoneactualshot/hit, nonfatalvictimhealth andMISSION_GOALhit_seen1. Remainingneutralframeschecknofurtherpulses.',limitations='Synthetic eventgraph only; originalguard50health50armor preserved. Exactaimandlivingvictim mustbeverified; missedshotsfail. Native/render/audio verification separate.')
    (folder/'recipe.json').write_text(json.dumps(report,indent=2)+'\n');return report

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');args=parser.parse_args();folder=ROOT/'artifacts/hit-event-live';folder.mkdir(parents=True,exist_ok=True);report=prepare(folder)
    if not args.run:print(folder/'recipe.json');return
    (folder/'report.json').unlink(missing_ok=True);states={};clean={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    for job in report['jobs']:
        name=job['name'];base=folder/name
        for suffix in ('.ppm','.png'):base.with_suffix(suffix).unlink(missing_ok=True)
        with base.with_suffix('.log').open('wb') as log:run=subprocess.run(job['command'],cwd=ROOT,env=dict(clean,**job['env']),stdout=log,stderr=subprocess.STDOUT,timeout=240)
        assert run.returncode==0,f'Inspect {base}.log'
        text=base.with_suffix('.log').read_text(errors='replace');combat=words(text,'COMBAT');expected=int(name=='hit')
        assert combat[:2]==[expected,expected],combat
        goal=[l.split()[2:] for l in text.splitlines() if l.startswith('MISSION_GOAL hit_seen ')]
        assert goal==[[str(expected),'0']],goal
        if expected:assert combat[3]!=0xffffffff and struct.unpack('<f',struct.pack('<I',combat[4]))[0]>0,combat
        states[name]=dict(combat=combat,goal=int(goal[0][0]))
        from PIL import Image
        Image.open(base.with_suffix('.ppm')).save(base.with_suffix('.png'));print(name,combat,goal,flush=True)
    report.update(status='HIT_CHAIN_PASS_VISUALS_UNVERIFIED',states=states);(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
