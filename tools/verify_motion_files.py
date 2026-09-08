"""Verify streaming C RFA metadata and every decoded key against asset bytes."""
import json,struct,subprocess
from pathlib import Path
from inspect_motions import inspect
root=Path(__file__).resolve().parents[1]
probe=root/'build/pc/Release/rf_motion_file_probe.exe'
files=tracks=rotations=positions=0
fixture=None
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
    entries=[e for e in archive.get('vpp',{}).get('entries',[]) if e['name'].endswith('.rfa')]
    if not entries: continue
    path=root/'Installed_Game'/archive['path']; expected=bytearray()
    with path.open('rb') as f:
        for e in entries:
            f.seek(e['offset']); raw=f.read(e['size']); parsed=inspect(raw)
            expected.extend(e['name'].encode().ljust(61,b'\0')+raw[:80]); files+=1
            for t in parsed['candidate_tracks']:
                off=t['offset']; n,m=struct.unpack_from('<2H',raw,off+4)
                assert t['bytes']==8+n*16+m*40
                expected.extend(struct.pack('<4I',off,t['bytes'],n,m)+raw[off:off+4]+raw[off+8:off+t['bytes']])
                tracks+=1; rotations+=n; positions+=m
            if e['name'].lower()=='ult2_stand.rfa': fixture=raw
    run=subprocess.run([str(probe),str(path)],capture_output=True,check=True)
    assert run.stdout==expected,(archive['path'],len(run.stdout),len(expected))
assert fixture is not None
folder=root/'artifacts/motion-file-tests'; folder.mkdir(parents=True,exist_ok=True)
def wrap(raw):
    data=bytearray(4096+(len(raw)+2047)//2048*2048)
    struct.pack_into('<4I',data,0,0x51890ace,1,1,len(data)); data[2048:2057]=b'test.rfa\0'
    struct.pack_into('<I',data,2108,len(raw)); data[4096:4096+len(raw)]=raw
    path=folder/'fixture.vpp'; path.write_bytes(data)
    return subprocess.run([str(probe),str(path)],capture_output=True).returncode
assert wrap(fixture)==0
bad=[fixture[:79],fixture[:-1]]
for offset,value in [(0,0),(4,9),(24,0xffffffff),(72,79),(76,len(fixture)+1),(80,0),(84,80),(36,0xffffffff)]:
    changed=bytearray(fixture); struct.pack_into('<I',changed,offset,value); bad.append(changed)
off=struct.unpack_from('<I',fixture,80)[0]
for offset,value in [(off,0x7fc00000),(off+4,0xffffffff)]:
    changed=bytearray(fixture); struct.pack_into('<I',changed,offset,value); bad.append(changed)
for i,raw in enumerate(bad): assert wrap(raw)==3,i
report=dict(result='PASS',files=files,tracks=tracks,rotation_keys=rotations,position_keys=positions,malformed_cases=len(bad),scope='Archive-backed directory and every decoded key; no skeleton integration')
(folder/'report.json').write_text(json.dumps(report,indent=2)); print(report)
