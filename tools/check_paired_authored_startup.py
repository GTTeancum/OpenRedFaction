"""Opt-in paired-source live PC startup and one actual rocket, no desktop input."""
import json, os, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
folder = ROOT / 'artifacts/paired-startup'
folder.mkdir(parents=True, exist_ok=True)
env = {k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl', RF_REPLAY_ARCHIVE='levelsm.vpp',
           RF_REPLAY_DEV_ROOM='1', RF_REPLAY_AUTHORED_SOURCES='2')
recording = ROOT / 'artifacts/geomod-postedit-re/detached-live-verified/control.bin'
command = [str(ROOT/'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
           str(ROOT/'Installed_Game'), str(recording), str(folder/'final.ppm')]
with (folder/'run.log').open('wb') as log:
    result = subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
assert result.returncode == 0, 'Inspect artifacts/paired-startup/run.log'
lines = (folder/'run.log').read_text().splitlines()
def values(label):
    return list(map(int, next(line for line in lines if line.startswith(label+' ')).split()[1:]))
sources = [int(line.split()[1]) for line in lines if line.startswith('AUTHORED_SOURCE ')]
assert sources == [94, 93], sources
rockets, geomod, publication, pieces = map(values, ('ROCKETS', 'GEOMOD', 'TERRAIN_PUBLICATION', 'DETACHED_PIECES'))
assert rockets[0:2] == [1, 1] and rockets[4:6] == [1, 0], rockets
assert geomod[1:3] == [1, 1] and geomod[4] <= 13*1024*1024 and geomod[5] == 0, geomod
assert publication[0] > 0 and publication[2:4] == [1, 1], publication
assert pieces[1:3] == [1, 1] and pieces[5] == 0, pieces
report = dict(result='PASS', sources=sources, rockets=rockets, geomod=geomod,
              publication=publication, pieces=pieces,
              scope='Two retained sources; one rocket cuts selected94 and creates rubble. Source93 uncut. No multi-source save/reset or native acceptance. Inspect final.ppm for visuals.')
# A legacy single-source checkpoint must never silently omit the second owner.
save_path = folder/'unsupported.rfcp'
save_path.unlink(missing_ok=True)
save_env = dict(env, RF_REPLAY_PLAYER_CHECKPOINT='1', RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(save_path))
save_command = command[:-1] + [str(folder/'save-rejected.ppm')]
with (folder/'save-rejected.log').open('wb') as log:
    rejected = subprocess.run(save_command, cwd=ROOT, env=save_env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
assert rejected.returncode != 0 and not save_path.exists(), 'Paired state must not use single-source saves'
assert 'DETACHED_SAVE_WRITER -4' in (folder/'save-rejected.log').read_text()
report['single_source_save_rejected'] = True
(folder/'report.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
