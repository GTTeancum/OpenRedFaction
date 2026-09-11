"""Authored replacement-force fixture in the single-player runtime, no host input."""
import argparse,json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/force-replace-replay';folder.mkdir(exist_ok=True)
env=dict(os.environ)
for key in ('RF_REPLAY_REGION_START','RF_REPLAY_DOOR_START','RF_REPLAY_LIFT_START'):env.pop(key,None)
env.update(RF_REPLAY_LEVEL='ctf01.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_FORCE_UID='11113')
parser=argparse.ArgumentParser();parser.add_argument('--cycle',action='store_true');args=parser.parse_args()
results=[]
for count in ((2,8,30,120,180,240,360) if args.cycle else (2,8,30,120)):
    source=folder/('cycle-inputs.bin' if count==360 else 'inputs.bin' if count==120 else f'input-{count}.bin')
    source.write_bytes(b'RFI3'+struct.pack('<I',32)+bytes(count*32))
    run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/f'final-{count}.ppm')],env=env,capture_output=True,text=True,check=True)
    (folder/f'pc-{count}.txt').write_text(run.stdout)
    def words(label):return list(map(int,next(l for l in run.stdout.splitlines() if l.startswith(label+' ')).split()[1:]))
    ticks=words('FORCE_TICKS');body=words('PC_PLAY_BODY')
    real=lambda v:struct.unpack('<f',struct.pack('<I',v))[0]
    velocity=list(map(real,body[46:49]));position=list(map(real,body[22:25]))
    assert ticks[0]==count-1 and ticks[4]>0 and ticks[7]==1 and ticks[11]==0,ticks
    assert words('LIVE_AUDIO')[4:6]==[1,0],'Sound request failed or repeated'
    assert real(ticks[10])==6.,'Unexpected authored class speed cap'
    if count==2:
        assert velocity[1]>9 and body[68]&0x200000 and position[1]>-7.75,(velocity,position)
    results.append(dict(frames=count,force_ticks=ticks,position=position,velocity=velocity,body_flags=body[68]))
assert results[1]['force_ticks'][4]>results[0]['force_ticks'][4]
if args.cycle:
    assert results[1]['force_ticks'][4]==results[2]['force_ticks'][4]==results[3]['force_ticks'][4]
    assert results[-1]['force_ticks'][4]>results[3]['force_ticks'][4],results
    assert results[3]['velocity'][1]<0 and results[-1]['velocity'][1]>0
    assert all(r['body_flags']&0x200000 for r in results)
report=dict(result='PASS',cycle=args.cycle,level='ctf01.rfl',archive='levelsm.vpp',staged_uid=11113,results=results,
 scope='Authored replacement region in existing single-player player runtime, zero command input. Checkpoints verify launch, active cap and one sound request over repeated in-region updates. Cycle mode checks a later return increases force count after a no-force interval, while the sound remains single. No multiplayer implementation, grounded re-entry, original full trajectory or native Xbox claim.')
(folder/('cycle-report.json' if args.cycle else 'report.json')).write_text(json.dumps(report,indent=2));print(report)
