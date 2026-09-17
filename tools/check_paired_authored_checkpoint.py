"""Process-local paired RFCP save/load and uninterrupted continuation control."""
import json, os, struct, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'artifacts/paired-checkpoint'
folder.mkdir(parents=True,exist_ok=True)
base=(ROOT/'artifacts/paired-two-shot/inputs.bin').read_bytes()
assert base[:8]==b'RFI6'+struct.pack('<I',48) and len(base)==8+850*48
(folder/'save.bin').write_bytes(base)
(folder/'resume.bin').write_bytes(base[:8]+bytes(200*48))
# A replay of N records executes N-1 updates.
(folder/'control.bin').write_bytes(base+bytes(199*48))
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',
           RF_REPLAY_AUTHORED_SOURCES='2',RF_REPLAY_PLAYER_CHECKPOINT='1')
def run(name,load=None):
    output=folder/(name+'.rfcp');output.unlink(missing_ok=True)
    local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(output))
    if load:local['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(load)
    with (folder/(name+'.log')).open('wb') as log:
        result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),
            str(folder/(name+'.bin')),str(folder/(name+'.ppm'))],cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT,timeout=180)
    assert result.returncode==0 and output.exists(),f'Inspect {name}.log'
    data=output.read_bytes()
    assert data[:4]==b'RFCP' and struct.unpack_from('<I',data,16)[0]==3
    assert data[576:580]==b'RFDS' and struct.unpack_from('<I',data,580)[0]==3
    assert data[992:996]==b'RFAS' and struct.unpack_from('<I',data,1004)[0]==2
    return data
saved=run('save')
resumed=run('resume',folder/'save.rfcp')
control=run('control')
assert resumed==control, 'Paired continuation differs from uninterrupted state'
log=(folder/'resume.log').read_text()
assert 'PLAYER_CHECKPOINT_LOAD ' in log and 'AUTHORED_SOURCE_CUTS 94 1 93 1 0 0 0 0' in log
report=dict(result='PASS',save_bytes=len(saved),continuation_bytes=len(resumed),
    scope='Actual two-rocket paired PC gameplay save, restored player/cores/rubble/shared atlas, and byte-identical 199-update continuation. Native reload and standing on paired rubble remain unverified.')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
