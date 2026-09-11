"""Bounded v180 force-region section1100 inventory from original462f60."""
import json,math,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def inspect(data):
    at=0
    def take(n):
        nonlocal at
        if n<0 or at+n>len(data):raise ValueError(('truncated',at,n))
        result=data[at:at+n];at+=n;return result
    def word():return struct.unpack('<I',take(4))[0]
    def string():return take(struct.unpack('<H',take(2))[0]).decode('cp1252')
    def floats(n):
        values=list(struct.unpack('<'+'f'*n,take(4*n)))
        if not all(math.isfinite(v) for v in values):raise ValueError('nonfinite')
        return values
    records=[]
    for _ in range(word()):
        start=at;uid=word();name=string();position=floats(3);orientation=floats(9);label=string();byte=take(1)[0];shape=word()
        if shape not in (1,2,3):raise ValueError(('shape',shape))
        extent=floats(1 if shape==1 else 3);strength=floats(1)[0];flags=word()
        records.append(dict(uid=uid,name=name,position=position,orientation_disk=orientation,label=label,
                            header_byte=byte,shape=shape,extent=extent,strength=strength,flags=flags,offset=start,bytes=at-start))
    if at!=len(data):raise ValueError(('trailing',at,len(data)))
    return records
def main():
    inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text());results=[]
    for level in levels:
        section=next((s for s in level['sections'] if s['type']=='0x1100'),None)
        if not section:continue
        archive=next(a for a in inventory['files'] if a['path']==level['archive'])
        entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
        with (root/'Installed_Game'/level['archive']).open('rb') as stream:
            stream.seek(entry['offset']+section['offset']+8);data=stream.read(section['size'])
        try:records=inspect(data)
        except Exception as error:raise ValueError(level['file']) from error
        results.append(dict(file=level['file'],archive=level['archive'],records=records))
    report=dict(result='PASS',levels=len(results),regions=sum(len(l['records']) for l in results),results=results,
        scope='Bounded Python section1100 inventory from462f60 read sequence, finite values and exact section exhaustion. Names/header byte retained without inferred behavior. No original parser execution or force application.')
    (root/'artifacts/force-regions.json').write_text(json.dumps(report,indent=2))
    print({k:v for k,v in report.items() if k!='results'})
    print([(l['file'],len(l['records'])) for l in results if l['records']][:12])
if __name__=='__main__':main()
