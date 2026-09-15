"""Supported enemy weapon drop: kill, collect, select and fire the rifle.
Explicit staging at authored shotgun; no inventory or health injection.
"""
import argparse,json,os,struct,subprocess
parser=argparse.ArgumentParser();parser.add_argument("--returning",action="store_true");args=parser.parse_args()
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/weapon-drops';folder.mkdir(parents=True,exist_ok=True)
source=folder/('return.bin' if args.returning else 'input.bin')
frames=260 if args.returning else 360
records=[]
for i in range(frames):
    if args.returning:
        values=(0,0,float(10<=i<25 or 191<=i<206),0,0,0,0,0,int(i==230),0,int(i in (30,210,220)),int(31<=i<59))
    else:
        values=(0,0,float(10<=i<25),0,0,0,0,0,int(i in (60,340)),0,int(i in (40,290,310)),int(160<=i<205))
    records.append(struct.pack('<5f7I',*values))
source.write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(records))
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S3.rfl',RF_REPLAY_ITEM_UID='2115',RF_REPLAY_TRACE='1')
if args.returning:env.update(RF_REPLAY_EXIT_UID='5151',RF_REPLAY_RETURN_EXIT_UID='5150',RF_REPLAY_RETURN_ITEM_UID='2115')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(source.with_suffix('.ppm'))],cwd=root,env=env,capture_output=True,text=True)
source.with_suffix('.log').write_text(run.stdout+run.stderr);run.check_returncode()
def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
drops,ammo,selection=map(words,['WEAPON_DROPS','PLAYER_AMMO','WEAPON_SELECTION'])
if args.returning:
    assert drops[:5]==[0]*5 and words('ACTOR_RETIREMENT')[1]==1,drops
    transitions=[l.split()[1:] for l in run.stdout.splitlines() if l.startswith('LEVEL_TRANSITION ')]
    assert transitions==[['L2S3.rfl','L2S2a.rfl','5151','61'],['L2S2a.rfl','L2S3.rfl','5150','181']]
else:assert drops[:5]==[1,1,42,2114,0],drops
assert drops[6:]==[49152,0],drops
assert selection[0]==1 and ammo[:3]==[8,0,39],(selection,ammo)
assert words('PLAYER_LIFE')[0]==0
report=dict(result='PASS',frames=frames,returning=args.returning,drops=drops,ammo=ammo,selection=selection,scope=('Collected weapon and ammo survive two dispatched authored exits; retired guard does not grant another drop on return.' if args.returning else 'Kill rifle guard2114 with authored shotgun, collect one rifle magazine, cycle to rifle and fire one three-round burst. Staged encounter, not full campaign traversal.'))
source.with_suffix('.json').write_text(json.dumps(report,indent=2));print(report)
