"""Prepare an enemy-free Fusion acquisition/fire replay; --run opts into PC execution.

No RF_REPLAY_FUSION auto-loadout flag. Two real shoulder-cannon item grants
supply ownership and one additional shell. Installed items.tbl has no dedicated
Fusion ammunition class; ordinary rocket_launcher_ammo is deliberately not used.
Original CTF06 geometry,506 props and pickups remain unchanged. Only the event
set becomes the two explicit Give_Item_To_Player requests. No checkpoint/input
state injection; RFI6 acts through ordinary selection and primary fire.
"""
import argparse,io,json,os,re,struct,subprocess
from pathlib import Path
from build_fragment_platform_fixture import read_entry,U,F,S
from inspect_levels import inspect as inspect_level
ROOT=Path(__file__).resolve().parents[1]

def discover():
    text=read_entry(ROOT/'Installed_Game/tables.vpp','items.tbl').decode('cp1252')
    blocks=re.split(r'(?=\$Class Name:)',text)
    matches=[b for b in blocks if re.search(r'\$Gives Weapon:\s*"shoulder_cannon"',b,re.I)]
    assert len(matches)==1,'Expected exactly one installed Fusion weapon item'
    name=re.search(r'\$Class Name:\s*"([^"]+)"',matches[0]).group(1)
    count=int(re.search(r'\$Count:\s*(\d+)',matches[0]).group(1))
    ammo=[b for b in blocks if re.search(r'\$Ammo For:\s*"shoulder_cannon"',b,re.I)]
    assert name=='shoulder cannon' and count==1 and not ammo
    return name

def prepare(folder):
    item=discover();game=folder/'game';game.mkdir(parents=True,exist_ok=True)
    original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    meta=inspect_level(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    events=[]
    for uid in (910400,910401):
        e=U(uid)+S(b'Give_Item_To_Player')+F(-2.75,-.4,2.5)+S(b'fusion_item_supply')
        e+=bytes([0])+F(0)+bytes([1,0])+U(0,0)+F(0,0)+S(item.encode())+S(b'')+U(0)+bytes([255]*4);events.append(e)
    data=bytearray(original[:meta['sections'][0]['offset']]);offsets={};replaced=False
    for section in meta['sections']:
        kind=int(section['type'],16);payload=original[section['offset']+8:section['offset']+8+section['size']]
        if kind==0x600:payload=U(2)+b''.join(events);replaced=True
        offsets[kind]=len(data);data+=U(kind,len(payload))+payload
    assert replaced
    struct.pack_into('<II',data,12,offsets[0x70000],offsets[0x1000000])
    inspect_level(io.BytesIO(data),dict(offset=0,size=len(data),name='ctf06.rfl'))
    size=4096+((len(data)+2047)&~2047);archive=bytearray(size);struct.pack_into('<4I',archive,0,0x51890ace,1,1,size)
    archive[2048:2057]=b'ctf06.rfl';struct.pack_into('<I',archive,2108,len(data));archive[4096:4096+len(data)]=data
    dest=game/'levelsm.vpp';assert not dest.exists() or dest.stat().st_nlink==1;dest.write_bytes(archive)
    for source in [*(ROOT/'Installed_Game').glob('*.vpp'),ROOT/'Installed_Game/bluebeard.bty']:
        if source.name.lower()=='levelsm.vpp':continue
        dest=game/source.name
        if dest.exists():assert os.path.samefile(source,dest)
        else:os.link(source,dest)
    jobs=[]
    for phase,frames in [('acquired',120),('launched',123)]:
        rows=[struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(phase=='launched' and f==120),0,int(f in range(12,53,4)),0) for f in range(frames)]
        base=folder/phase;base.with_suffix('.bin').write_bytes(b'RFI6'+U(48)+b''.join(rows))
        env=dict(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_SETUP_UID='910400,910401')
        jobs.append(dict(name=phase,frames=frames,env=env,command=[str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(game),str(base.with_suffix('.bin')),str(base.with_suffix('.ppm'))]))
    report=dict(status='PREPARED_NOT_RUN',item_class=item,weapon='shoulder_cannon',ammo_source='Second actual shoulder cannon grant; no dedicated installed ammo item.',jobs=jobs,schedule='Give_Item events0/60; eleven normal cycle edges12..52 selectslot12; first shot120; stopafterframe122 before nearby blast dominates test.',scope='Authored item ownership/shell supply, sparse resources, normal selection and one finite-ammo Fusion launch. Explosion/GeoMod coverage is separate.',limitations='Timing assumes normal DEV base11slots and demand-added Fusion12. Parent must implement resource demand before run. Actual firing frame/render must be inspected; this preparation is not a pass.')
    (folder/'recipe.json').write_text(json.dumps(report,indent=2)+'\n');return report

def words(text,key):
    rows=[list(map(int,l.split()[1:])) for l in text.splitlines() if l.startswith(key+' ')];assert rows,key;return rows[-1]

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');args=parser.parse_args()
    folder=ROOT/'artifacts/fusion-pickup-live';report=prepare(folder)
    if not args.run:print(folder/'recipe.json');return
    (folder/'report.json').unlink(missing_ok=True)
    clean={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))};states={}
    for job in report['jobs']:
        name=job['name'];base=folder/name
        for suffix in ('.ppm','.png'):base.with_suffix(suffix).unlink(missing_ok=True)
        with base.with_suffix('.log').open('wb') as log:
            result=subprocess.run(job['command'],cwd=ROOT,env=dict(clean,**job['env']),stdout=log,stderr=subprocess.STDOUT,timeout=240)
        assert result.returncode==0,f'Inspect {base}.log'
        text=base.with_suffix('.log').read_text(errors='replace')
        grant=words(text,'SCRIPT_GRANTS');selection=words(text,'WEAPON_SELECTION');ammo=words(text,'PLAYER_AMMO');fusion=words(text,'FUSION_PROJECTILES')
        assert grant[0]==2 and grant[7]==0 and selection[0]==12 and ammo[7]==0,(grant,selection,ammo)
        assert ammo[1:3]==([1,1] if name=='acquired' else [1,0]),ammo
        assert fusion[0]==int(name=='launched') and fusion[4]==0,fusion
        if name=='launched':assert ammo[0]==states['acquired']['ammo'][0]
        from PIL import Image
        Image.open(base.with_suffix('.ppm')).save(base.with_suffix('.png'))
        states[name]=dict(grants=grant,selection=selection,ammo=ammo,fusion=fusion);print(name,ammo[:3],fusion,flush=True)
    report.update(status='STATE_PASS_VISUALS_UNVERIFIED',states=states);(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
