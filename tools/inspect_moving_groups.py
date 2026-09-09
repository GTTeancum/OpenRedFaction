"""Bounded v180 moving-group inventory from original 463820 reader sequence."""
import json,struct,math
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def inspect(data):
 at=0
 def take(n):
  nonlocal at
  if n<0 or n>len(data)-at:raise ValueError((at,n,len(data)))
  v=data[at:at+n];at+=n;return v
 def number():return struct.unpack('<I',take(4))[0]
 def string():return take(struct.unpack('<H',take(2))[0]).decode('cp1252')
 def floats(n):
  v=list(struct.unpack('<'+'f'*n,take(n*4)));assert all(math.isfinite(x) for x in v);return v
 records=[]
 for _ in range(number()):
  begin=at;name=string();header=list(take(2));keys=[]
  for _ in range(number()):
   start=at;uid=number();position=floats(3);orientation=floats(9);label=string();flag=take(1)[0];timing=floats(5);links=[number() for _ in range(3)];rotation=floats(1)[0]
   keys.append(dict(offset=start,bytes=at-start,uid=uid,position=position,orientation_disk=orientation,label=label,flag=flag,timing=timing,links=links,rotation=rotation))
  legacy=[]
  for _ in range(number()):legacy.append(dict(uid=number(),position=floats(3),orientation_disk=floats(9)))
  flags=list(take(6));mode=number();unknown=number();sounds=[]
  for _ in range(4):sounds.append(dict(name=string(),value=floats(1)[0]))
  ids1=[number() for _ in range(number())];ids2=[number() for _ in range(number())]
  records.append(dict(offset=begin,bytes=at-begin,name=name,header=header,keys=keys,legacy=legacy,flags=flags,mode=mode,unknown=unknown,sounds=sounds,ids1=ids1,ids2=ids2))
 assert at==len(data),(at,len(data));return records

def main():
 inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text());results=[]
 for level in levels:
  section=next((s for s in level['sections'] if s['type']=='0x3000'),None)
  if not section:continue
  archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
  with (root/'Installed_Game'/level['archive']).open('rb') as f:f.seek(entry['offset']+section['offset']+8);data=f.read(section['size'])
  try:records=inspect(data)
  except Exception as e:raise ValueError(level['file']) from e
  results.append(dict(file=level['file'],archive=level['archive'],bytes=len(data),records=records))
 report=dict(result='PASS',levels=len(results),groups=sum(len(l['records']) for l in results),keys=sum(len(g['keys']) for l in results for g in l['records']),scope='Python v180 section 3000 inventory from original 463820 call sequence; exact section exhaustion, finite floats. Field semantics provisional; not a C reader, original parser execution or runtime movement.',results=results)
 (root/'artifacts/moving-groups.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
if __name__=='__main__':main()
