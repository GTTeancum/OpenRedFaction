"""Pack verified local factory data for the standalone PC/Xbox runtime.

Run inspect_geomod_template.py first. Original assets remain ignored; this
does not execute or distribute RF.exe as part of the reconstructed runtime.
"""
import hashlib
import json
from pathlib import Path
import struct
from extract_geomod_template import SHA
ROOT=Path(__file__).resolve().parents[1]

def run():
    assert hashlib.sha256((ROOT/'Installed_Game/RF.exe').read_bytes()).hexdigest()==SHA
    data=json.loads((ROOT/'artifacts/geomod-holey01-original.json').read_text())
    assert data['exe_sha256']==SHA and data['bit_exact_match']
    assert len(data['vertex_words'])==10 and len(data['faces'])==16
    radius=data['bounds_radius_center'][6]
    result=struct.pack('<4sIIf3f',b'RFCT',1,16,radius,0,0,0)
    for face,uv in zip(data['faces'],data['uv_words']):
        assert len(face)==len(uv)==3
        for index,corner in reversed(list(zip(face,uv))):
            result+=struct.pack('<5I',*data['vertex_words'][index],*corner)
    path=ROOT/'build/data/geomod-template.bin';path.parent.mkdir(parents=True,exist_ok=True)
    if not path.exists() or path.read_bytes()!=result:path.write_bytes(result)
    print('Packed',len(result),'bytes:',path)

if __name__=='__main__':run()
