"""Read-only final-pose floor-plane audit for actual connected destruction saves.
Does not establish full polygon contact, penetration resolution, or retail parity.
"""
import argparse,json,math,os,subprocess
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--max-penetration',type=float,help='Fail after reporting if any actual vertex/floor gap is below this negative tolerance')
parser.add_argument('--max-clearance',type=float,help='Optional maximum positive gap for these known floor-resting fixtures')
parser.add_argument('--first-cut',action='store_true',help='Audit the first-junction save instead of both junctions')
args=parser.parse_args()
if args.max_penetration is not None and (not math.isfinite(args.max_penetration) or args.max_penetration<0):
    parser.error('--max-penetration must be finite and nonnegative')
if args.max_clearance is not None and (not math.isfinite(args.max_clearance) or args.max_clearance<0):
    parser.error('--max-clearance must be finite and nonnegative')
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'artifacts'/('fragment-support-audit-first' if args.first_cut else 'fragment-support-audit');OUT.mkdir(exist_ok=True)
report={}
for uid in (92,108):
    source=ROOT/'artifacts'/('side-group'+str(uid))
    saved=OUT/(str(uid)+'.rfcp');saved.unlink(missing_ok=True)
    env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',
        RF_REPLAY_AUTHORED_SOURCE=str(uid),RF_REPLAY_AUTHORED_SOURCES='3',RF_REPLAY_PLAYER_CHECKPOINT='1',
        RF_REPLAY_FRAGMENT_SUPPORT_AUDIT='1',RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(source/('shot.rfcp' if args.first_cut else 'second.rfcp')),
        RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(saved))
    log=OUT/(str(uid)+'.log')
    with log.open('w') as stream:
        result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),
            str(source/'resume.bin'),str(OUT/(str(uid)+'.ppm'))],cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
    assert result.returncode==0,(uid,result.returncode)
    assert saved.read_bytes()==(source/('resume.rfcp' if args.first_cut else 'second-reload.rfcp')).read_bytes(),'Audit changed simulation/save state'
    rows=[]
    for line in log.read_text().splitlines():
        if not line.startswith('DETACHED_SUPPORT_AUDIT '):continue
        words=line.split()[1:];assert len(words)==19
        ids=list(map(int,words[:8]));values=list(map(float,words[8:]))
        row=dict(source_slot=ids[0],batch=ids[1],piece=ids[2],flags=ids[3],vertices=ids[4],spheres=ids[5],
            floor_found=ids[6],floor_face=ids[7],mesh_min_y=values[0],sphere_min_y=values[1],
            closest_sphere_gap=values[2],mesh_plane_gap=values[3],plane=values[4:8],minimum_sphere_plane_gap=values[8],actual_vertex_floor_gap=values[9],actual_vertex_floor_face=int(values[10]))
        assert row['floor_found'] and not row['flags']&0x80000000,row
        rows.append(row)
    expected=json.loads((source/'report.json').read_text())['pieces' if args.first_cut else 'second_pieces'][0]
    assert len(rows)==expected and rows,(uid,len(rows),expected)
    report[str(uid)]=dict(state_preserved=True,pieces=rows)
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

if args.max_penetration is not None or args.max_clearance is not None:
    failures=[dict(group=uid,batch=p['batch'],piece=p['piece'],gap=p['actual_vertex_floor_gap'])
        for uid,case in report.items() for p in case['pieces']
        if p['actual_vertex_floor_face']==0xffffffff
        or (args.max_penetration is not None and p['actual_vertex_floor_gap'] < -args.max_penetration)
        or (args.max_clearance is not None and p['actual_vertex_floor_gap'] > args.max_clearance)]
    if failures:
        print('CONTACT_GATE_FAIL '+json.dumps(failures));raise SystemExit(1)
    print('CONTACT_GATE_PASS')
