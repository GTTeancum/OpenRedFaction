"""Check entity records against installed v180 sections, using format-reference leads.

Reference: rafalh/rf-reversed rfl.ksy, entities_section/entity (GPL-3.0-or-later).
This independent byte reader validates boundaries/data, not original runtime loading.
"""
import json,struct,subprocess,collections
from pathlib import Path
from inspect_levels import inspect
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=entities=0;classes=collections.Counter();first=[]
for archive in inventory['files']:
    for entry in archive.get('vpp',{}).get('entries',[]):
        if not entry['name'].lower().endswith('.rfl'):continue
        path=root/'Installed_Game'/archive['path']
        with path.open('rb') as stream:
            level=inspect(stream,entry);section=next((s for s in level['sections'] if s['type']=='0x30000'),None)
            if not section:continue
            stream.seek(entry['offset']+section['offset']+8);data=stream.read(section['size'])
        cursor=0
        def take(n):
            global cursor
            assert 0<=n<=len(data)-cursor,(entry['name'],cursor,n,len(data))
            result=data[cursor:cursor+n];cursor+=n;return result
        def string():return take(struct.unpack('<H',take(2))[0])
        count=struct.unpack('<I',take(4))[0];expected=[]
        for _ in range(count):
            start=cursor;uid=take(4);name=string();transform=take(48);script=string()
            take(13);string();string();take(29)
            labels=[string() for _ in range(7)];take(18);flags=take(17)
            assert flags[-1] in (0,1),(entry['name'],cursor,flags.hex())
            if flags[-1]:take(4)
            string();string()
            rotation=transform[24:48]+transform[12:24]
            record=uid+transform[:12]+rotation
            for text in (name,script,labels[3],labels[5]):
                assert len(text)<256 and b'\0' not in text
                record+=text.ljust(256,b'\0')
            record+=struct.pack('<II',start,cursor-start);expected.append(record)
            classes[name.decode('cp1252')]+=1
            if entry['name'].lower()=='l1s1.rfl':first.append(dict(uid=struct.unpack('<i',uid)[0],name=name.decode('cp1252'),script=script.decode('cp1252'),position=struct.unpack('<3f',transform[:12]),skin=labels[5].decode('cp1252')))
        assert cursor==len(data),(entry['name'],cursor,len(data))
        actual=subprocess.check_output([str(root/'build/pc/Release/rf_level_entity_probe.exe'),str(path),entry['name']])
        assert actual==b''.join(expected),entry['name']
        entities+=count;levels+=1
report=dict(result='PASS',levels=levels,entities=entities,classes=dict(classes),first_level=first,
    scope='Installed entity-section boundaries, all exported fields and raw spans match independent byte decoding; community format lead, no original executable loader or gameplay semantics comparison')
(root/'artifacts/level-entities-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k not in ('classes','first_level')})
