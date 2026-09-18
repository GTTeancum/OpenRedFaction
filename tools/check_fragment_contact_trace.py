"""Measure finite-floor gaps before/after translation and accepted rotation.
Uses existing side-group fixtures; tracing must preserve the full checkpoint.
"""
import json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'artifacts/fragment-contact-trace';OUT.mkdir(exist_ok=True)
report={}
for uid in (92,108):
    source=ROOT/'artifacts'/('side-group'+str(uid))
    for name in ('shot','second'):
        stem=OUT/f'{uid}-{name}';save=stem.with_suffix('.rfcp');save.unlink(missing_ok=True)
        env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
        env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',
            RF_REPLAY_AUTHORED_SOURCE=str(uid),RF_REPLAY_AUTHORED_SOURCES='3',RF_REPLAY_PLAYER_CHECKPOINT='1',
            RF_REPLAY_FRAGMENT_CONTACT_TRACE='1',RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(save))
        if name=='second':env['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(source/'shot.rfcp')
        with stem.with_suffix('.log').open('w') as log:
            result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',
                str(ROOT/'Installed_Game'),str(source/(name+'.bin')),str(stem.with_suffix('.ppm'))],
                cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
        assert result.returncode==0,(uid,name,result.returncode)
        assert save.read_bytes()==(source/(name+'.rfcp')).read_bytes(),(uid,name,'Tracing changed state')
        rows=[]
        for line in stem.with_suffix('.log').read_text().splitlines():
            if not line.startswith('DETACHED_CONTACT_TRACE '):continue
            w=line.split()[1:];assert len(w)==10,w
            rows.append(dict(source=int(w[0]),batch=int(w[1]),piece=int(w[2]),fraction=float(w[3]),
                hit_counts=[int(w[i]) for i in (4,6,8)],gaps=[float(w[i]) for i in (5,7,9)]))
        assert rows,(uid,name,'No contacts traced')
        # All three poses must have finite-floor samples to compare their minima.
        comparable=[r for r in rows if all(r['hit_counts'])]
        worst=min(comparable,key=lambda r:r['gaps'][2]-r['gaps'][1]) if comparable else None
        report[f'{uid}-{name}']=dict(state_preserved=True,contacts=len(rows),
            comparable=len(comparable),worst_rotation_delta=worst,rows=rows)
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:{a:b for a,b in v.items() if a!='rows'} for k,v in report.items()},indent=2))
