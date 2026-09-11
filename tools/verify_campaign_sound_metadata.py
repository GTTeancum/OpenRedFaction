"""Independent metadata bytes and authored registration order in PC campaign."""
import json,struct
import verify_sound_metadata_reader as reader
root=reader.root
inventory=json.loads((root/'artifacts/inventory.json').read_text())
available={e['name'].lower() for a in inventory['files'] if a['path']=='audio.vpp' for e in a['vpp']['entries']}
globals=json.loads((root/'artifacts/sound-table-inventory.json').read_text())['rows']
ambients=json.loads((root/'artifacts/ambient-records.json').read_text())['results']
groups=json.loads((root/'artifacts/moving-groups.json').read_text())['results']
lookup=json.loads((root/'artifacts/sound-metadata-lookup.json').read_text())
order=b''.join(struct.pack('<H',(tag-1)&65535) for tag in lookup['sorted_source_tags'])
full_hash=2166136261
for byte in bytes(reader.expected)+order:full_hash=((full_hash^byte)*16777619)&0xffffffff
results=[]
for archive,name in [('levels1.vpp','L1S1.rfl'),('levels1.vpp','L1S3.rfl'),('levels1.vpp','L2S1.rfl'),('levelsm.vpp','ctf01.rfl')]:
    names={r['name'].lower():None for r in globals}
    for level in ambients:
        if level['archive']==archive and level['file']==name:
            for r in level['records']:
                if r['name'].lower() in available:names.setdefault(r['name'].lower(),None)
    for level in groups:
        if level['archive']==archive and level['file']==name:
            for r in level['records']:
                for sound in r['sounds']:
                    if sound['name'].lower() in available:names.setdefault(sound['name'].lower(),None)
    matched=loops=missing=0;registration_hash=2166136261
    for sample in names:
        metadata=lookup['selected'].get(sample)
        if metadata:
            a,b=struct.unpack_from('<II',reader.expected,metadata['source_index']*128+120)
            matched+=1;loops+=(a>>30)&1
        else:a=b=0;missing+=1
        for value in (a,b):registration_hash=((registration_hash^value)*16777619)&0xffffffff
    expected=[2712,355344,263,matched,loops,missing,registration_hash,full_hash]
    text=(root/'artifacts/ambient-campaign'/f'{name}.txt').read_text()
    actual=list(map(int,next(line for line in text.splitlines() if line.startswith('SOUND_METADATA ')).split()[1:]))
    assert actual==expected,(name,actual,expected)
    results.append(dict(level=name,state=actual))
report=dict(result='PASS',results=results,scope='PC campaign logs from verify_ambient_campaign.py, independent packed reader fields, '
            'original sort permutation and authored global/ambient/controller registration order. Missing metadata is counted explicitly; no fallback or playback claim.')
(root/'artifacts/campaign-sound-metadata.json').write_text(json.dumps(report,indent=2));print(report)
