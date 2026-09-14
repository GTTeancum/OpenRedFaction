"""Process-local handoff fixture; separate scene runs, not seamless transitions."""
import json, os, struct, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/campaign-state';folder.mkdir(exist_ok=True)
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
def run(name,level,records,extra,success=True):
    source=folder/(name+'.bin');source.write_bytes(b'RFI5'+struct.pack('<I',44)+records)
    options=dict(env,RF_REPLAY_LEVEL=level,RF_REPLAY_ARCHIVE='levels1.vpp',**extra)
    result=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=options,capture_output=True,text=True)
    (folder/(name+'.log')).write_text(result.stdout+result.stderr)
    if success:result.check_returncode()
    else:assert result.returncode!=0
    return result
records=b''.join(struct.pack('<5f6I',0,0,float(10<=i<25),0,0,0,0,0,int(i==60),0,int(i==40)) for i in range(120))
run('source','L4S5.rfl',records,dict(RF_REPLAY_ITEM_UID='3415',RF_REPLAY_PLAYER_STATE_OUT=str(folder/'source.state')))
source=(folder/'source.state').read_bytes();assert len(source)==464
owned=source[:64];reserve=struct.unpack_from('<32i',source,64);loaded=struct.unpack_from('<64i',source,192)
health,armor,weapon,catalog=struct.unpack_from('<ffII',source,448)
assert owned[8]==1 and loaded[8]==39 and reserve[2]==0 and weapon==8
idle=bytes(44)
run('destination','L1S2.rfl',idle,dict(RF_REPLAY_PLAYER_STATE_IN=str(folder/'source.state'),RF_REPLAY_PLAYER_STATE_OUT=str(folder/'destination.state')))
assert (folder/'destination.state').read_bytes()==source,'handoff changed player state'
# Same loaded scene without a staged import must still start a new game normally.
run('fresh','L1S2.rfl',idle,dict(RF_REPLAY_PLAYER_STATE_OUT=str(folder/'fresh.state')))
fresh=(folder/'fresh.state').read_bytes();assert fresh[8]==0 and struct.unpack_from('<I',fresh,456)[0]==3
bad=bytearray(source);struct.pack_into('<I',bad,460,catalog^1);(folder/'bad.state').write_bytes(bad)
run('bad-catalog','L1S2.rfl',idle,dict(RF_REPLAY_PLAYER_STATE_IN=str(folder/'bad.state')),False)
report=dict(result='PASS',bytes=len(source),weapon=weapon,rifle_loaded=loaded[8],health=health,armor=armor,cases=['cross-level exact state','fresh game','catalog mismatch rejected'])
(folder/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
