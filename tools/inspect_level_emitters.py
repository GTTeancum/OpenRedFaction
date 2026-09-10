"""Bounded v180 particle-emitter inventory from original 45fcf0 reader sequence."""
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
  begin=at;uid=number();name=string();position=floats(3);orientation=floats(9);script=string()
  header_byte=take(1)[0];header_word=number();spawn_radius=floats(1)[0];unknown_floats=floats(2);bitmap=string()
  delay=floats(2);speed=floats(2);acceleration=floats(1)[0];life=floats(2);radius=floats(2)
  growth=floats(1)[0];gravity=floats(1)[0];angle=floats(1)[0]
  color=list(take(4));destination=list(take(4));emitter_flags=number();particle_flags=number();enabled=take(1)[0]
  cycle=floats(4);finish_age=floats(1)[0]
  records.append(dict(offset=begin,bytes=at-begin,uid=uid,name=name,position=position,orientation_disk=orientation,script=script,
   header_byte=header_byte,header_word=header_word,spawn_radius=spawn_radius,unknown_floats=unknown_floats,bitmap=bitmap,
   delay=delay,speed=speed,acceleration=acceleration,life=life,radius=radius,growth=growth,gravity_scale=gravity,
   cone_angle=angle,color=color,color_destination=destination,emitter_flags=emitter_flags,particle_flags=particle_flags,
   enabled=enabled,cycle=cycle,finish_age=finish_age))
 assert at==len(data),(at,len(data));return records

def main():
 inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text());results=[]
 for level in levels:
  section=next((s for s in level['sections'] if s['type']=='0xa00'),None)
  if not section:continue
  archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
  with (root/'Installed_Game'/level['archive']).open('rb') as f:f.seek(entry['offset']+section['offset']+8);data=f.read(section['size'])
  try:records=inspect(data)
  except Exception as e:raise ValueError(level['file']) from e
  results.append(dict(file=level['file'],archive=level['archive'],bytes=len(data),records=records))
 report=dict(result='PASS',levels=len(results),emitters=sum(len(l['records']) for l in results),
  scope='Python v180 section A00 inventory following original 45fcf0 reader call sequence; bounded reads, finite floats, exact section exhaustion. Semantics provisional; not original reader execution or shared runtime binding.',results=results)
 (root/'artifacts/level-emitters.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='results'})
if __name__=='__main__':main()
