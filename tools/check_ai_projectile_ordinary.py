"""Prepare controlled non-DEV NPC projectile encounters; no launch by default.

This is a synthetic encounter on original L1S1 geometry: original guard8456 and
player9858 records remain, other NPC records/events are removed, and only the
guard record's documented primary/secondary strings are overridden. Original
tables.vpp remains hardlinked and unchanged. Installed inputs are never modified. No DEV flags,
forced firing, inventory injection, original executable or desktop control.
"""
import argparse,io,json,os,re,struct,subprocess
from pathlib import Path
from build_fragment_platform_fixture import read_entry,U,S
from inspect_levels import inspect as inspect_level
ROOT=Path(__file__).resolve().parents[1]

def archive(path,rows):
    size=2048+((len(rows)*64+2047)&~2047)+sum((len(data)+2047)&~2047 for _,data in rows)
    raw=bytearray(size);struct.pack_into('<4I',raw,0,0x51890ace,1,len(rows),size);at=2048+((len(rows)*64+2047)&~2047)
    for i,(name,data) in enumerate(rows):
        label=name.encode();assert len(label)<60;row=2048+i*64;raw[row:row+len(label)]=label;struct.pack_into('<I',raw,row+60,len(data));raw[at:at+len(data)]=data;at+=(len(data)+2047)&~2047
    assert not path.exists() or path.stat().st_nlink==1;path.write_bytes(raw)

def records(data):
    # Boundary traversal matches src/core/level.c and verify_level_entities.py.
    at=4;out=[]
    def take(n):
        nonlocal at
        assert 0<=n<=len(data)-at
        value=data[at:at+n];at+=n;return value
    def string():return take(struct.unpack('<H',take(2))[0])
    for _ in range(struct.unpack_from('<I',data)[0]):
        begin=at;uid=struct.unpack('<I',take(4))[0];name=string();take(48);string();take(13);string();string();take(29)
        for _ in range(7):string()
        take(18);flags=take(17);assert flags[16] in (0,1)
        if flags[16]:take(4)
        string();string();out.append((uid,name.decode('cp1252'),data[begin:at]))
    assert at==len(data);return out

def loadout(raw,primary):
    # Original464010: first two strings of the seven-string block are
    # primary_weapon/secondary_weapon. Empty inherits; literal none clears.
    at=4
    def string():
        nonlocal at
        size=struct.unpack_from('<H',raw,at)[0];at+=2+size;assert at<=len(raw)
    string();at+=48;string();at+=13;string();string();at+=29
    start=at;string();string();end=at
    return raw[:start]+S(primary.encode('ascii'))+S(b'none')+raw[end:]

def prepare(folder,frames):
    original=read_entry(ROOT/'Installed_Game/levels1.vpp','L1S1.rfl')
    meta=inspect_level(io.BytesIO(original),dict(offset=0,size=len(original),name='L1S1.rfl'))
    sec=next(s for s in meta['sections'] if s['type']=='0x30000');allrows=records(original[sec['offset']+8:sec['offset']+8+sec['size']])
    kept=[r for r in allrows if r[0] in (8456,9858)];assert {r[0] for r in kept}=={8456,9858}
    guard=next(r[1] for r in kept if r[0]==8456)
    jobs=[]
    for kind,weapon in [('rocket','Rocket Launcher'),('grenade','Grenade')]:
        game=folder/kind/'game';game.mkdir(parents=True,exist_ok=True)
        chosen=[loadout(r[2],weapon) if r[0]==8456 else r[2] for r in kept]
        entity_data=U(len(chosen))+b''.join(chosen)
        assert [r[0] for r in records(entity_data)]==[r[0] for r in kept]
        level=bytearray(original[:meta['sections'][0]['offset']]);offsets={}
        for section in meta['sections']:
            typ=int(section['type'],16);data=original[section['offset']+8:section['offset']+8+section['size']]
            if typ==0x30000:data=entity_data
            elif typ==0x600:data=U(0)
            offsets[typ]=len(level);level+=U(typ,len(data))+data
        struct.pack_into('<II',level,12,offsets[0x70000],offsets[0x1000000]);inspect_level(io.BytesIO(level),dict(offset=0,size=len(level),name='L1S1.rfl'))
        archive(game/'levels1.vpp',[('L1S1.rfl',level)])
        for source in [*(ROOT/'Installed_Game').glob('*.vpp'),ROOT/'Installed_Game/bluebeard.bty']:
            if source.name.lower()=='levels1.vpp':continue
            dest=game/source.name
            if dest.exists():assert os.path.samefile(source,dest),f'Remove legacy copied fixture file before preparing: {dest}'
            else:os.link(source,dest)
        base=folder/kind/'idle';base.with_suffix('.bin').write_bytes(b'RFI6'+U(48)+bytes(frames*48))
        jobs.append(dict(kind=kind,weapon=weapon,guard_class=guard,frames=frames,env=dict(RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp',RF_REPLAY_ACTOR_UID='8456'),command=[str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(game),str(base.with_suffix('.bin')),str(base.with_suffix('.ppm'))]))
    report=dict(status='PREPARED_NOT_RUN',jobs=jobs,scope='One original hostile guard with controlled per-instance projectile loadout, ordinary startup/targeting/fire and neutral player. Synthetic guard record override is explicit, not an original authored encounter claim.',limitations='Original464010 seven-string indices0/1 set primary to selected projectile and secondary to literal none; all class tables remain original. Requires new per-instance parser/startup binding integration. Default650frames covers grenade5second fuse if launched before350. Player death may stop later shots. Ordinary startup and launch code use finite owned inventory; current aggregate logs do not expose remaining NPC rounds, so these runs cannot prove exact depletion/exhaustion. Missing motion binding, delayed hostility or blocked geometry must fail/report rather than force firing. Visual/audio inspection remains separate.')
    (folder/'recipe.json').write_text(json.dumps(report,indent=2)+'\n');return report

def words(text,key):
    rows=[list(map(int,l.split()[1:])) for l in text.splitlines() if l.startswith(key+' ')];assert rows,key;return rows[-1]

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');parser.add_argument('--kind',choices=('rocket','grenade'));parser.add_argument('--frames',type=int,default=650);args=parser.parse_args();assert 360<=args.frames<=1200
    folder=ROOT/'artifacts/ai-projectile-ordinary';folder.mkdir(parents=True,exist_ok=True);report=prepare(folder,args.frames)
    if not args.run:print(folder/'recipe.json');return
    (folder/'report.json').unlink(missing_ok=True);states={};env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    for job in report['jobs']:
        kind=job['kind']
        if args.kind and kind!=args.kind:continue
        base=folder/kind/'idle'
        for suffix in ('.ppm','.png'):base.with_suffix(suffix).unlink(missing_ok=True)
        with base.with_suffix('.log').open('wb') as log:run=subprocess.run(job['command'],cwd=ROOT,env=dict(env,**job['env']),stdout=log,stderr=subprocess.STDOUT,timeout=300)
        assert run.returncode==0,f'Inspect {base}.log'
        text=base.with_suffix('.log').read_text(errors='replace');ai=words(text,'AI_ROCKETS' if kind=='rocket' else 'AI_GRENADES')
        assert words(text,'COMBAT')[0]==0,'Neutral player must not fire'
        assert ai[0]>0 and ai[4]==0,ai
        assert ai[1 if kind=='rocket' else 2]>0,'No observed rocket impact/grenade detonation within bounded replay'
        states[kind]=dict(counters=ai,finite_inventory='Ordinary startup/consume path; exact remaining rounds unobservable in current log')
        from PIL import Image
        Image.open(base.with_suffix('.ppm')).save(base.with_suffix('.png'));print(kind,ai,flush=True)
    report.update(status='PROJECTILE_STATE_PASS_VISUALS_AND_EXHAUSTION_UNVERIFIED',states=states);(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
