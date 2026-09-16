"""Prepare bounded RFCP semantic-rejection cases; does not launch/build anything."""
import argparse,hashlib,json,struct
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args()
b=a.source.read_bytes();assert b[:4]==b'RFCP' and struct.unpack_from('<I',b,4)[0]==1 and struct.unpack_from('<I',b,8)[0]==len(b)<=110524
assert struct.unpack_from('<I',b,20)[0]==544 and b[32:36]==b'RFPL' and b[576:580]==b'RFDS'
a.out.mkdir(parents=True,exist_ok=True)
cases={}
def change(name,offset,data):
 v=bytearray(b);v[offset:offset+len(data)]=data;cases[name]=bytes(v)
change('wrong-profile',16,struct.pack('<I',struct.unpack_from('<I',b,16)[0]^1))
change('dead-player',32+24,struct.pack('<f',0))
change('nan-position',32+32,struct.pack('<I',0x7fc00000))
change('unsupported-air-standing',32+32,struct.pack('<3f',10,-8,0))
change('far-rock',32+32,struct.pack('<3f',10000,10000,10000))
change('unchanged-bar',32+32,struct.pack('<3f',3.875,-5.375,1))
change('unsupported-weapon',32+20,struct.pack('<I',64))
change('wrong-level',576+16,b'wrong.rfl\0')
cases['legacy-rfds-in-player-mode']=b[576:]
rows=[]
for name,data in cases.items():
 target=a.out/(name+'.bin');target.write_bytes(data);rows.append(dict(name=name,path=str(target),bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),expect='Reject before first frame/publication'))
(a.out/'manifest.json').write_text(json.dumps(dict(source=str(a.source),sha256=hashlib.sha256(b).hexdigest(),cases=rows,scope='Unchecksummed RFCP input; outer RFSG must be regenerated when testing native semantic fallback. Existing default RFDS mode remains a separate compatibility test.'),indent=2)+'\n')
print(f'Prepared {len(rows)} rejection cases')
