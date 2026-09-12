"""Version180 authored clutter layout from465220/464f90; raw field names retained."""
import json
import struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def inspect(data):
    cursor=0
    def take(n):
        nonlocal cursor
        if n<0 or cursor+n>len(data):raise ValueError(('clutter truncated',cursor,n))
        value=data[cursor:cursor+n];cursor+=n;return value
    def word():return struct.unpack('<I',take(4))[0]
    def string():return take(struct.unpack('<H',take(2))[0])
    count=word()
    if count>(len(data)-4)//67:raise ValueError('Invalid clutter count')
    records=[]
    for _ in range(count):
        start=cursor
        row=dict(uid=word(),class_name=string(),position=take(12),matrix=take(36),name=string(),enabled=take(1)[0])
        common=word()
        if common>(len(data)-cursor)//6:raise ValueError('Invalid common count')
        row['common']=[(string(),word()) for _ in range(common)]
        row['resource_name']=string()
        links=word()
        if links>(len(data)-cursor)//4:raise ValueError('Invalid link count')
        row['links']=[word() for _ in range(links)]
        row['offset']=start;row['bytes']=cursor-start;records.append(row)
    if cursor!=len(data):raise ValueError(('clutter trailing',cursor,len(data)))
    return records

def sections():
    inventory=json.loads((ROOT/'artifacts/inventory.json').read_text())
    for level in json.loads((ROOT/'artifacts/levels.json').read_text()):
        assert level['version']==180
        archive=next(a for a in inventory['files'] if a['path']==level['archive'])
        entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
        for section in level['sections']:
            if section['type']!='0x50000':continue
            with (ROOT/'Installed_Game'/level['archive']).open('rb') as stream:
                stream.seek(entry['offset']+section['offset']+8);data=stream.read(section['size'])
            yield level,data

if __name__=='__main__':
    rows=[]
    for level,data in sections():
        records=inspect(data)
        rows.append(dict(file=level['file'],bytes=len(data),records=len(records),
                         common=sum(len(r['common']) for r in records),links=sum(len(r['links']) for r in records),
                         resources=sum(bool(r['resource_name']) for r in records),
                         classes=sorted(set(r['class_name'].decode('cp1252') for r in records))))
    report=dict(result='PASS',sections=len(rows),records=sum(r['records'] for r in rows),levels=rows)
    (ROOT/'artifacts/clutter-records.json').write_text(json.dumps(report,indent=2))
    print({k:v for k,v in report.items() if k!='levels'})
    print(next(r for r in rows if r['file'].lower()=='l1s1.rfl'))
