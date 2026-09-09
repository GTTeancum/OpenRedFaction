"""Bounded v180 event inventory from original 462150 and type table 5a1a3c."""
import hashlib
import json
import math
import struct
from pathlib import Path
import pefile

root = Path(__file__).resolve().parents[1]


def inspect(data, types):
    at = 0
    def take(size):
        nonlocal at
        if size < 0 or size > len(data)-at:
            raise ValueError((at,size,len(data)))
        value=data[at:at+size]; at+=size
        return value
    def number(): return struct.unpack('<I',take(4))[0]
    def string(): return take(struct.unpack('<H',take(2))[0]).decode('cp1252')
    def floats(n):
        value=list(struct.unpack('<'+'f'*n,take(n*4)))
        if not all(math.isfinite(x) for x in value): raise ValueError(('nonfinite',at))
        return value
    records=[]
    for _ in range(number()):
        start=at
        uid,kind,position,name=number(),string(),floats(3),string()
        header=take(1)[0]; delay=floats(1)[0]; flags=list(take(2))
        words=[number(),number()]; values=floats(2); texts=[string(),string()]
        count=number()
        if count > (len(data)-at)//4: raise ValueError(('links',at,count))
        links=[number() for _ in range(count)]
        index=next((i for i,t in enumerate(types) if t.lower()==kind.lower()),-1)
        if index<0: raise ValueError(('unknown event type',kind))
        matrix=floats(9) if index in (4,63,70,46) else None
        color=list(take(4)) # 52d170, version threshold 176 at 462370.
        records.append(dict(uid=uid,type=kind,type_index=index,name=name,position=position,
            header_byte=header,delay=delay,flags=flags,words=words,values=values,texts=texts,
            links=links,orientation_disk=matrix,color_bytes=color,offset=start,bytes=at-start))
    if at!=len(data): raise ValueError(('trailing',at,len(data)))
    return records


def main():
    executable=root/'Installed_Game/RF.exe'
    if hashlib.sha256(executable.read_bytes()).hexdigest()!='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836':
        raise ValueError('Event table requires the verified original executable')
    pe=pefile.PE(str(executable))
    image=pe.get_memory_mapped_image(); base=pe.OPTIONAL_HEADER.ImageBase
    types=[]
    for address in range(0x5a1a3c,0x5a1ba4,4):
        offset=struct.unpack_from('<I',image,address-base)[0]-base
        types.append(image[offset:image.index(b'\0',offset)].decode('cp1252'))
    inventory=json.loads((root/'artifacts/inventory.json').read_text())
    results=[]
    for level in json.loads((root/'artifacts/levels.json').read_text()):
        section=next((s for s in level['sections'] if s['type']=='0x600'),None)
        if not section: continue
        if level['version']!=180: raise ValueError(('version',level['version']))
        archive=next(a for a in inventory['files'] if a['path']==level['archive'])
        entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
        with (root/'Installed_Game'/level['archive']).open('rb') as stream:
            stream.seek(entry['offset']+section['offset']+8)
            data=stream.read(section['size'])
        try: records=inspect(data,types)
        except Exception as error: raise ValueError(level['file']) from error
        results.append(dict(file=level['file'],archive=level['archive'],records=records))
    report=dict(result='PASS',levels=len(results),events=sum(len(x['records']) for x in results),
        links=sum(len(r['links']) for x in results for r in x['records']),types=types,results=results)
    (root/'artifacts/events.json').write_text(json.dumps(report,indent=2))
    print({k:v for k,v in report.items() if k not in ('types','results')})
    for level in results:
        if level['file'].lower()=='l1s1.rfl':
            print(json.dumps([r for r in level['records'] if r['uid'] in (9826,8694,8695)],indent=2))

if __name__=='__main__': main()
