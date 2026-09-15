"""Authored rifle alternate cadence, release and primary/alternate handoff.
Staged at actual rifle3415 in L4S5; no inventory injection.
"""
import json,os,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/rifle-alternate';folder.mkdir(parents=True,exist_ok=True)
results=[]
for name in (sys.argv[1:] or ['held','release','from_burst','to_burst','primary_priority']):
    frames=120 if name=='held' else 180
    def record(i):
        primary=(i==60 if name=='from_burst' else i==90 if name=='to_burst' else 60<=i<120 if name=='primary_priority' else False)
        alt=(65<=i<120 if name=='from_burst' else 60<=i<90 if name=='to_burst' else 60<=i<120)
        return struct.pack('<5f7I',0,0,float(10<=i<25),0,0,0,0,0,int(primary),0,int(i==40),int(alt))
    source=folder/(name+'.bin');source.write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(record(i) for i in range(frames)))
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L4S5.rfl',RF_REPLAY_ITEM_UID='3415',RF_REPLAY_TRACE='1')
    run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(source.with_suffix('.ppm'))],cwd=root,env=env,capture_output=True,text=True)
    source.with_suffix('.log').write_text(run.stdout+run.stderr);run.check_returncode()
    def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
    alt,ammo,combat,weapon=map(words,['RIFLE_ALT','PLAYER_AMMO','COMBAT','PLAYER_WEAPON'])
    expected={'held':(10,10,0,3),'release':(10,10,0,0),'from_burst':(3,4,1,0),'to_burst':(5,8,0,0),'primary_priority':(0,6,0,0)}[name]
    shots,total,cancelled,clip=expected
    assert alt[0]==shots and alt[4]==cancelled and alt[6:]==[0,0],alt
    assert ammo[:3]==[8,0,42-total] and ammo[7]==0 and combat[0]==total,(ammo,combat)
    assert combat[2]==1 and words('PLAYER_LIFE')[0]==0
    if name in ('held','release'):
        hits=[int(l.split()[1]) for l in run.stdout.splitlines() if l.startswith('COMBAT_EVENT ') and l.split()[2]=='0']
        assert hits==[60,66,72],hits
    assert weapon[1]==clip and weapon[6]==0,weapon
    assert words('RIOT_STICK')==[0]*8 and words('SHOTGUN')==[0]*8
    if shots:assert alt[5]==struct.unpack('<I',struct.pack('<f',4))[0] and alt[3]!=0
    else:assert alt==[0]*8
    print(name,'PASS',alt,ammo,flush=True)
    results.append(dict(case=name,alt=alt,ammo=ammo,combat=combat,weapon=weapon))
(folder/('report.json' if len(sys.argv)==1 else '-'.join(sys.argv[1:])+'-report.json')).write_text(json.dumps(dict(result='PASS',cases=results),indent=2))
