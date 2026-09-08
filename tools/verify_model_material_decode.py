"""Check installed material conversion against the documented 53ae5f field map."""
import json,struct,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1];records=[]
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
    for entry in archive.get('vpp',{}).get('entries',[]):
        if not entry['name'].lower().endswith('.v3c'):continue
        with (root/'Installed_Game'/archive['path']).open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
        for s in inspect(raw)['sections']:
            if s['type']!='0x5355424d':continue
            records.extend(raw[s['material_offset']+i*84:s['material_offset']+(i+1)*84] for i in range(s['materials']))
cases=[];expected=[]
for raw in records:
    assert raw[0] and b'\0' in raw[:32] and b'\0' in raw[48:80]
    for transparent in (0,1):
        primary,secondary=101,202
        cases.append(raw+struct.pack('<iiII',primary,secondary,transparent,4096))
        dst=bytearray(200)
        for offset in (0x44,):struct.pack_into('<i',dst,offset,-1)
        dst[9:13]=b'\xff'*4;struct.pack_into('<I',dst,0x78,15)
        flags,=struct.unpack_from('<I',raw,80)
        struct.pack_into('<I',dst,4,1 | (8 if transparent or flags&2 else 0) | (16 if flags&1 else 0));dst[8]=bool(flags&2)
        for source,target in ((0,0x14),(48,0x90)):
            name=raw[source:source+32].split(b'\0')[0]+b'\0';dst[target:target+len(name)]=name
        struct.pack_into('<i',dst,0x10,primary);struct.pack_into('<i',dst,0xb4,secondary if raw[48] else -1)
        struct.pack_into('<I',dst,0xb8,1);dst[0x84:0x90]=raw[36:48]
        expected.append(struct.pack('<i',0)+dst+raw[32:36])
out=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--material-disk'],input=b''.join(cases))
assert out==b''.join(expected)
for offset in (0,48):
    malformed=bytearray(records[0]);malformed[offset:offset+32]=b'x'*32
    actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--material-disk'],input=malformed+struct.pack('<iiII',1,2,0,4096))
    assert actual==struct.pack('<i',-2)+bytes(204)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--material-disk'],input=records[0]+struct.pack('<iiII',1,2,0,0))
assert actual==struct.pack('<i',-4)+bytes(204)
report=dict(result='PASS',materials=len(records),cases=len(cases),rejections=3,scope='All installed records against documented 53ae5f mapping with both resolved transparency values; scalar owned independently; synthetic texture handles, not complete original loader execution')
(root/'artifacts/model-material-decode-verification.json').write_text(json.dumps(report,indent=2));print(report)
