"""Prepare a disposable three-prop chain fixture using unmodified game assets."""
import io,json,os,struct
from pathlib import Path
from build_fragment_platform_fixture import read_entry,U,F,S
from inspect_levels import inspect
ROOT=Path(__file__).resolve().parents[1]
def main():
    folder=ROOT/'artifacts/clutter-chain-live';game=folder/'game';game.mkdir(parents=True,exist_ok=True)
    original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    positions=[(23.5,5.1,-159.470245),(23.5,5.1,-157.470245),(23.5,5.1,-155.470245)]
    names=[b'Oil Drum',b'gas_can',b'Oil Drum'];props=U(3)
    for index,(name,position) in enumerate(zip(names,positions)):
        props+=U(910001+index)+S(name)+F(*position)+F(1,0,0,0,1,0,0,0,1)+S(b'chain_target')+bytes([1])+U(0)+S(b'')+U(0)
    data=bytearray(original[:meta['sections'][0]['offset']]);offsets={}
    for section in meta['sections']:
        kind=int(section['type'],16);payload=original[section['offset']+8:section['offset']+8+section['size']]
        if kind==0x50000:payload=props
        elif kind==0x600:payload=U(0)
        elif kind==0x70000:payload=F(26.55,5.1,-159.470245,-1,0,0,0,0,1,0,1,0)
        offsets[kind]=len(data);data+=U(kind,len(payload))+payload
    struct.pack_into('<II',data,12,offsets[0x70000],offsets[0x1000000])
    inspect(io.BytesIO(data),dict(offset=0,size=len(data),name='ctf06.rfl'))
    size=4096+((len(data)+2047)&~2047);archive=bytearray(size);struct.pack_into('<4I',archive,0,0x51890ace,1,1,size)
    archive[2048:2057]=b'ctf06.rfl';struct.pack_into('<I',archive,2108,len(data));archive[4096:4096+len(data)]=data
    destination=game/'levelsm.vpp';assert not destination.exists() or destination.stat().st_nlink==1;destination.write_bytes(archive)
    for source in [*(ROOT/'Installed_Game').glob('*.vpp'),ROOT/'Installed_Game/bluebeard.bty']:
        if source.name.lower()=='levelsm.vpp':continue
        destination=game/source.name
        if destination.exists():assert os.path.samefile(source,destination)
        else:os.link(source,destination)
    jobs=[]
    for name in ['baseline','fire']:
        rows=[struct.pack('<5f7I',0,0,0,-.9 if 12<=frame<24 else 0,0,0,0,0,int(name=='fire' and frame==40),0,0,0) for frame in range(90)]
        inputs=folder/(name+'.bin');inputs.write_bytes(b'RFI6'+U(48)+b''.join(rows))
        jobs.append(dict(name=name,command=[str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(game),str(inputs),str(folder/(name+'.ppm'))],env=dict(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp')))
    (folder/'recipe.json').write_text(json.dumps(dict(jobs=jobs,positions=positions,classes=[x.decode() for x in names],scope='Authored class models/life/factors with isolated DEV placements; one pistol shot, then inspect contacts and covered radial chain. No effect resources are substituted.'),indent=2))
    print(folder/'recipe.json')
if __name__=='__main__':main()
