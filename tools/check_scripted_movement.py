"""Prepare a controlled non-DEV Goto while a hostile guard recognizes the player.

Preserve original guard8456/player9858 records and L1S1 geometry. Replace events
with a confirmed Goto whose position is two units along the guard's authored
right vector, linked to8456. No NPC pose changes or forced alert; ordinary player
staging faces the guard three units away. Default prepares only.
"""
import argparse,io,json,math,os,struct,subprocess
from pathlib import Path
from check_ai_projectile_ordinary import archive,records,words
from build_fragment_platform_fixture import read_entry,U,F,S
from inspect_levels import inspect as inspect_level
ROOT=Path(__file__).resolve().parents[1]

def prepare(folder,distance):
    game=folder/'game';game.mkdir(parents=True,exist_ok=True)
    raw=read_entry(ROOT/'Installed_Game/levels1.vpp','L1S1.rfl');meta=inspect_level(io.BytesIO(raw),dict(offset=0,size=len(raw),name='L1S1.rfl'))
    sec=next(s for s in meta['sections'] if s['type']=='0x30000');entities=records(raw[sec['offset']+8:sec['offset']+8+sec['size']]);kept=[r for r in entities if r[0] in (8456,9858)];assert {r[0] for r in kept}=={8456,9858}
    guard=next(r[2] for r in kept if r[0]==8456);at=6+struct.unpack_from('<H',guard,4)[0];transform=struct.unpack_from('<12f',guard,at)
    start=transform[:3];right=transform[6:9] # disk forward/right/up order, level.c rotates into rows2/0/1.
    target=[start[i]+distance*right[i] for i in range(3)];target[1]=start[1]
    event=U(910700)+S(b'Goto')+F(*target)+S(b'controlled_script_destination')
    event+=bytes([0])+F(0)+bytes([1,0])+U(0,0)+F(0,0)+S(b'')+S(b'')+U(1,8456)+bytes([255]*4)
    level=bytearray(raw[:meta['sections'][0]['offset']]);offsets={}
    for sec in meta['sections']:
        typ=int(sec['type'],16);payload=raw[sec['offset']+8:sec['offset']+8+sec['size']]
        if typ==0x30000:payload=U(len(kept))+b''.join(r[2] for r in kept)
        elif typ==0x600:payload=U(1)+event
        offsets[typ]=len(level);level+=U(typ,len(payload))+payload
    struct.pack_into('<II',level,12,offsets[0x70000],offsets[0x1000000]);inspect_level(io.BytesIO(level),dict(offset=0,size=len(level),name='L1S1.rfl'));archive(game/'levels1.vpp',[('L1S1.rfl',level)])
    for source in [*(ROOT/'Installed_Game').glob('*.vpp'),ROOT/'Installed_Game/bluebeard.bty']:
        if source.name.lower()=='levels1.vpp':continue
        dest=game/source.name
        if dest.exists():assert os.path.samefile(source,dest)
        else:os.link(source,dest)
    jobs=[]
    for name,frames in [('before',29),('moved',90)]:
        base=folder/name;base.with_suffix('.bin').write_bytes(b'RFI6'+U(48)+bytes(frames*48))
        jobs.append(dict(name=name,frames=frames,env=dict(RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp',RF_REPLAY_ACTOR_UID='8456',RF_REPLAY_GOTO_UID='910700'),command=[str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(game),str(base.with_suffix('.bin')),str(base.with_suffix('.ppm'))]))
    report=dict(status='PREPARED_NOT_RUN',jobs=jobs,start=start,target=target,scope='Confirmed Goto callback assigns event.position to8456 while ordinary enemy AI recognizes nearby player; actor moves or reports real route/blockage attempts. NoDEV, forcedAI-mode or runtime NPC relocation.',limitations='Synthetic destination may encounter original collision. Positive requests plus target match do not alone establish movement. Harness requires steps and measurableposition change and alerts; route/blockage counters retained to diagnose failure. It does not prove full campaign pathfinding or uninterrupted pursuit arbitration. Default targettwo authoredrightunits can be adjusted with --distance after evidence. Player recognition is measured by ENEMY_COMBAT alerts, not assumed from nearby spawn.')
    (folder/'recipe.json').write_text(json.dumps(report,indent=2)+'\n');return report

def vec(words):return struct.unpack('<3f',struct.pack('<3I',*words))

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');parser.add_argument('--distance',type=float,default=2);args=parser.parse_args();assert math.isfinite(args.distance) and .5<=abs(args.distance)<=5
    folder=ROOT/'artifacts/scripted-movement-live';folder.mkdir(parents=True,exist_ok=True);report=prepare(folder,args.distance)
    if not args.run:print(folder/'recipe.json');return
    (folder/'report.json').unlink(missing_ok=True);states={};clean={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    for job in report['jobs']:
        name=job['name'];base=folder/name
        for suffix in ('.ppm','.png'):base.with_suffix(suffix).unlink(missing_ok=True)
        with base.with_suffix('.log').open('wb') as log:run=subprocess.run(job['command'],cwd=ROOT,env=dict(clean,**job['env']),stdout=log,stderr=subprocess.STDOUT,timeout=240)
        assert run.returncode==0,f'Inspect {base}.log'
        text=base.with_suffix('.log').read_text(errors='replace');move=words(text,'SCRIPT_MOVE');actor=words(text,'SCRIPT_ACTOR');routes=words(text,'SCRIPT_ROUTES');enemy=words(text,'ENEMY_COMBAT')
        assert move[7]==0 and routes[6]==0 and enemy[7]==0,(move,routes,enemy)
        states[name]=dict(movement=move,actor=actor,routes=routes,enemy=enemy)
        if name=='before':assert move[0]==0,move
        else:
            assert move[0]==1 and move[5:7]==[8456,910700] and actor[0]==8456,(move,actor)
            position=vec(actor[1:4]);destination=vec(actor[4:7]);assert math.dist(destination,report['target'])<.001,(destination,report['target'])
            assert move[1]>0 and math.dist(position,report['start'])>.05,(move,position)
            assert enemy[1]>0,'No ordinary player-recognition alert observed'
            states[name].update(position=position,destination=destination)
        from PIL import Image
        Image.open(base.with_suffix('.ppm')).save(base.with_suffix('.png'));print(name,move,enemy[:3],flush=True)
    report.update(status='STATE_PASS_VISUALS_UNVERIFIED',states=states);(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
