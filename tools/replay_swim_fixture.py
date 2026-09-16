"""Prepare authored L2S3 room0 swim input; never launch a game or send host input."""
import hashlib,json,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    folder=ROOT/'artifacts/swim-fixture';folder.mkdir(parents=True,exist_ok=True)
    # RFI6: moveXYZ/lookXY then crouch,jump,use,fire,reload,cycle,altfire.
    frames=[struct.pack('<5f7I',0,0,0,0,0,int(150<=i<240),int(30<=i<120),0,0,0,0,0) for i in range(240)]
    assert struct.unpack_from('<7I',frames[30],20)==(0,1,0,0,0,0,0)
    assert struct.unpack_from('<7I',frames[150],20)==(1,0,0,0,0,0,0)
    recording=b'RFI6'+struct.pack('<I',48)+b''.join(frames)
    (folder/'input.bin').write_bytes(recording)
    report=dict(level='L2S3.rfl',archive='levels1.vpp',body_position=[100,1,60],basis=[1,0,0,0,1,0,0,0,1],
        authored_water_surface=2.4499001502990723,frames=240,sha256=hashlib.sha256(recording).hexdigest(),
        actions={'0-29':'idle','30-119':'held jump/ascent','120-149':'idle','150-239':'held crouch/descent'},
        pc_environment={'RF_REPLAY_SWIM_TEST':'1','RF_REPLAY_ARCHIVE':'levels1.vpp','RF_REPLAY_LEVEL':'L2S3.rfl'},
        xbox_flags=['swim-test.flag'],
        scope='Preparation only. Original4e1630 confirms room0 alongY.5..3 atx100,z60; no actor isolation or geometry edits. set_campaign_spawn copies body origin unchanged, authored eye tags determine camera. Validate PLAYER_SWIM mode4 and actual ascent/descent before acceptance.')
    (folder/'recipe.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Prepared240-frame L2S3 swimming recording; no runtime launched')
if __name__=='__main__':main()
