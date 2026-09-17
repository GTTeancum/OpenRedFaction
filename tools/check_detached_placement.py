"""Reject a finite but overlapping fragment pose in a real authored player save."""
import os,struct,subprocess,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/geomod-postedit-re/detached-placement-negative';folder.mkdir(parents=True,exist_ok=True)
source=ROOT/'artifacts/geomod-postedit-re/detached-placement-restart'
data=bytearray((source/'saved.rfcp').read_bytes());assert data[:4]==b'RFCP' and data[32:36]==b'RFPL'
player=struct.unpack_from('<3f',data,64);rfds=576;assert data[rfds:rfds+4]==b'RFDS'
size=struct.unpack_from('<I',data,rfds+8)[0];pieces=struct.unpack_from('<I',data,rfds+12)[0]
trailer=rfds+size-pieces;assert data[trailer:trailer+4]==b'RFPB' and pieces==336
body=trailer+16+12;old=struct.unpack_from('<3f',data,body+88);delta=[player[i]-old[i] for i in range(3)]
for offset in [88,100,248,260]:
 values=struct.unpack_from('<3f',data,body+offset);struct.pack_into('<3f',data,body+offset,*[values[i]+delta[i] for i in range(3)])
checkpoint=folder/'overlap.rfcp';checkpoint.write_bytes(data)
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1',RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(checkpoint))
with (folder/'run.log').open('wb') as log:
 result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(source/'continued.bin'),str(folder/'frame.ppm')],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
text=(folder/'run.log').read_text();assert result.returncode!=0 and 'DETACHED_PLAYER_RESTORE_REJECT ' in text and 'GEOMOD_CHECKPOINT_ERROR load' in text,text[-2000:]
report=dict(result='PASS',scope='Real checkpoint rejected specifically at fragment/player clearance gate; raw RFCP has no outer transport checksum',player=player,original_piece=old)
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
