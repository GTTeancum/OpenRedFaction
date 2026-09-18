"""Read-only final-pose floor-plane audit for actual connected destruction saves.
Does not establish full polygon contact, penetration resolution, or retail parity.
"""
import json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'artifacts/fragment-support-audit';OUT.mkdir(exist_ok=True)
report={}
for uid in (92,108):
    source=ROOT/'artifacts'/('side-group'+str(uid))
    saved=OUT/(str(uid)+'.rfcp');saved.unlink(missing_ok=True)
    env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',
        RF_REPLAY_AUTHORED_SOURCE=str(uid),RF_REPLAY_AUTHORED_SOURCES='3',RF_REPLAY_PLAYER_CHECKPOINT='1',
        RF_REPLAY_FRAGMENT_SUPPORT_AUDIT='1',RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(source/'second.rfcp'),
        RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(saved))
    log=OUT/(str(uid)+'.log')
    with log.open('w') as stream:
        result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),
            str(source/'resume.bin'),str(OUT/(str(uid)+'.ppm'))],cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
    assert result.returncode==0,(uid,result.returncode)
    assert saved.read_bytes()==(source/'second-reload.rfcp').read_bytes(),'Audit changed simulation/save state'
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
    expected=json.loads((source/'report.json').read_text())['second_pieces'][0]
    assert len(rows)==expected and rows,(uid,len(rows),expected)
    report[str(uid)]=dict(state_preserved=True,pieces=rows)
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
