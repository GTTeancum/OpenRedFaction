"""Generate process-local post reset controls from the verified two-cut recording."""
import json,struct,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def row(forward=0,crouch=0,use=0,fire=0,alt=0):
    return struct.pack('<5f7I',0,0,forward,0,0,crouch,0,use,fire,0,0,alt)
def main():
    folder=ROOT/'artifacts/authored-post-live'
    base=(folder/'two-shot.bin').read_bytes()
    assert base[:8]==b'RFI6'+struct.pack('<I',48) and len(base)==8+550*48
    gesture=row(crouch=1,use=1,alt=1)
    safe=base+row()*30+gesture+row()*59
    blocked=base+row(.8)*118+row()*30+gesture+row()*31
    recut=safe+row()*20+row(fire=1)+row()*179
    restored=safe+row(.8)*135+row()*30
    manifest={}
    for name,data in [('reset-safe',safe),('reset-blocked',blocked),('reset-recut',recut),('reset-restored',restored)]:
        (folder/(name+'.bin')).write_bytes(data)
        manifest[name]={'frames':(len(data)-8)//48,'sha256':hashlib.sha256(data).hexdigest()}
    manifest['scope']='Generated candidates; actual reset acceptance/rejection and restored collision require runtime verification.'
    (folder/'reset-recipe.json').write_text(json.dumps(manifest,indent=2))
    print(json.dumps(manifest,indent=2))
if __name__=='__main__':main()
