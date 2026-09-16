"""Inventory authored liquid rooms/faces; no generated surface or gameplay claim."""
import json,struct
from pathlib import Path
from inspect_geometry import inspect
ROOT=Path(__file__).resolve().parents[1]
def liquid(data):
 inspect(data)
 at=6
 def take(n):
  nonlocal at
  assert 0<=n<=len(data)-at
  out=data[at:at+n];at+=n;return out
 def number():return struct.unpack('<I',take(4))[0]
 def string():return take(struct.unpack('<H',take(2))[0]).decode('cp1252')
 for _ in range(number()):string()
 take(number()*12);rooms=[]
 for index in range(number()):
  raw=take(40);name=string()
  if raw[32]:
   prefix=take(8);texture=string();tail=take(37)
   rooms.append(dict(index=index,name=name,bounds=list(struct.unpack_from('<6f',raw,4)),prefix_hex=prefix.hex(),texture=texture,tail_hex=tail.hex()))
  if raw[33]:take(4)
 for _ in range(number()):number();take(number()*4)
 take(number()*32);take(number()*12);faces=[]
 for index in range(number()):
  raw=take(56);mapping=struct.unpack_from('<I',raw,20)[0];flags=struct.unpack_from('<I',raw,40)[0];room,count=struct.unpack_from('<II',raw,48)
  take(count*(12 if mapping==0xffffffff else 20))
  if flags&4:faces.append(dict(index=index,room=room,flags=flags))
 return dict(liquid_rooms=rooms,serialized_liquid_faces=faces)
def main():
 inv=json.loads((ROOT/'artifacts/inventory.json').read_text());levels=json.loads((ROOT/'artifacts/levels.json').read_text());rows=[]
 for level in levels:
  section=next(s for s in level['sections'] if s['type']=='0x100')
  archive=next(a for a in inv['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
  with (ROOT/'Installed_Game'/level['archive']).open('rb') as stream:
   stream.seek(entry['offset']+section['offset']+8);data=stream.read(section['size'])
  row=liquid(data);row.update(file=level['file'],archive=level['archive']);rows.append(row)
 for row in rows:
  room_ids={r['index'] for r in row['liquid_rooms']}
  assert all(f['room'] in room_ids for f in row['serialized_liquid_faces']),row['file']
 report=dict(water_levels=sum(bool(r['liquid_rooms']) for r in rows),levels=len(rows),rooms=sum(len(r['liquid_rooms']) for r in rows),faces=sum(len(r['serialized_liquid_faces']) for r in rows),rows=rows)
 (ROOT/'artifacts/liquid-geometry-inventory.json').write_text(json.dumps(report,indent=2))
 print({k:v for k,v in report.items() if k!='rows'})
if __name__=='__main__':main()
