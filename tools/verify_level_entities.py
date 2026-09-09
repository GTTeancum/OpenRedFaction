"""Check entity records against installed v180 sections, using format-reference leads.

Reference: rafalh/rf-reversed rfl.ksy, entities_section/entity (GPL-3.0-or-later).
This independent byte reader validates boundaries/data, not original runtime loading.
"""
import json,struct,subprocess,collections,sys,re
from pathlib import Path
from inspect_levels import inspect
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=entities=0;classes=collections.Counter();first=[]
spawn='--spawn' in sys.argv;owned='--owned' in sys.argv or spawn;max_bytes=0
if spawn:
    import pefile
    sys.path.insert(0,str(root/'local/python'))
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
    from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
    pe=pefile.PE(str(root/'build/xbox/main.exe'));image=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
    x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(image)+4095)//4096*4096);x.mem_write(origin,image)
    base=0x30000000;x.mem_map(base,0x10000);stack=base+0xe000;stop=base+0xf000
    entry_address=int(re.search(r'_rf_level_entity_spawn_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
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
            relationships=take(13);string();string();take(29)
            labels=[string() for _ in range(7)];take(18);flags=take(17)
            assert flags[-1] in (0,1),(entry['name'],cursor,flags.hex())
            if flags[-1]:take(4)
            string();string()
            rotation=transform[24:48]+transform[12:24]
            record=uid+transform[:12]+rotation
            for text in (name,script,labels[3],labels[5]):
                assert len(text)<256 and b'\0' not in text
                record+=text.ljust(256,b'\0')
            record+=struct.pack('<II',start,cursor-start);expected.append(record+(data[start:cursor] if owned else b''))
            if spawn:expected[-1]=relationships[1:9]+struct.pack('<II',relationships[9],(2 if flags[1] else 0)|(4 if flags[15] else 0))
            if spawn:
                raw=data[start:cursor];assert len(raw)<0x8000
                item=bytearray(1088);item[:4]=uid;struct.pack_into('<II',item,1080,len(raw),base+0x1000)
                x.mem_write(base,bytes(item));x.mem_write(base+0x1000,raw);x.mem_write(base+0x9000,bytes([0xa5])*16)
                x.mem_write(stack,struct.pack('<III',stop,base,base+0x9000));x.reg_write(UC_X86_REG_ESP,stack)
                x.emu_start(entry_address,stop,count=100000)
                assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
                assert bytes(x.mem_read(base+0x9000,16))==expected[-1],('NXDK spawn',entry['name'],uid.hex())
            classes[name.decode('cp1252')]+=1
            if entry['name'].lower()=='l1s1.rfl':first.append(dict(uid=struct.unpack('<i',uid)[0],name=name.decode('cp1252'),script=script.decode('cp1252'),position=struct.unpack('<3f',transform[:12]),skin=labels[5].decode('cp1252'),relationship_words=struct.unpack('<3I',relationships[1:]),friendliness=struct.unpack_from('<I',relationships,5)[0],factory_flags=(2 if flags[1] else 0)|(4 if flags[15] else 0)))
        assert cursor==len(data),(entry['name'],cursor,len(data))
        run=subprocess.run([str(root/'build/pc/Release/rf_level_entity_probe.exe'),str(path),entry['name']]+(['--entity-spawn'] if spawn else ['--owned-entities'] if owned else []),stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True)
        actual=run.stdout
        if owned:max_bytes=max(max_bytes,int(run.stderr))
        assert actual==b''.join(expected),entry['name']
        entities+=count;levels+=1
report=dict(result='PASS',levels=levels,entities=entities,owned=owned,spawn=spawn,max_allocated_bytes=max_bytes,classes=dict(classes),first_level=first,
    scope='Installed entity-section boundaries, all exported fields and raw spans match independent byte decoding; community format lead, no original executable loader or gameplay semantics comparison')
if spawn:
    report['nxdk_cases']=entities
    report['scope']='Four recovered spawn fields match independent installed-byte decoding on PC and compiled NXDK; PC checks one-byte-short records/output preservation after archive closure. No complete original factory or gameplay integration.'
(root/('artifacts/entity-spawn-verification.json' if spawn else 'artifacts/owned-level-entities-verification.json' if owned else 'artifacts/level-entities-verification.json')).write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k not in ('classes','first_level')})
