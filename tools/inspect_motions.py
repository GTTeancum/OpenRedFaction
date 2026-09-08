"""Audit installed RFA envelopes, preserving unclassified trailing regions.

This records observed boundaries, not recovered animation sampling semantics.
Header words retain offsets as names until their original use is established.
"""
import json
import struct
from collections import Counter
from pathlib import Path

def inspect(data):
    if len(data) < 80: raise ValueError('truncated header')
    words = struct.unpack_from('<20I', data)
    if words[0] != 0x46564d56 or words[1] not in (7, 8): raise ValueError('unsupported header')
    count = words[6]
    directory_end = 80 + count * 4
    if directory_end > len(data): raise ValueError('offset table exceeds file')
    offsets = list(struct.unpack_from('<' + 'I' * count, data, 80))
    end_a, end_b = words[18:20]
    if not directory_end <= end_a <= end_b <= len(data): raise ValueError('invalid region boundaries')
    if offsets and offsets[0] != directory_end: raise ValueError('unexpected first record offset')
    if offsets != sorted(offsets) or any(not directory_end <= x < end_a for x in offsets):
        raise ValueError('invalid record offsets')
    ranges = [dict(offset=a, bytes=b-a) for a,b in zip(offsets, offsets[1:] + [end_a])]
    return dict(version=words[1], bytes=len(data), header_words=list(words),
                candidate_track_count=count, candidate_tracks=ranges,
                region_48=dict(offset=end_a, bytes=end_b-end_a),
                region_4c=dict(offset=end_b, bytes=len(data)-end_b))

def main():
    root = Path(__file__).resolve().parents[1]
    inventory = json.loads((root / 'artifacts/inventory.json').read_text())
    records = []
    for archive in inventory['files']:
        for entry in archive.get('vpp', {}).get('entries', []):
            if not entry['name'].lower().endswith('.rfa'): continue
            with (root / 'Installed_Game' / archive['path']).open('rb') as stream:
                stream.seek(entry['offset']); data = stream.read(entry['size'])
            if len(data) != entry['size']: raise ValueError('truncated archive read')
            records.append(dict(archive=archive['path'], motion=entry['name'], **inspect(data)))
    if not records: raise ValueError('no motion files')
    versions = dict(Counter(r['version'] for r in records))
    extra = sum(bool(r['region_48']['bytes'] or r['region_4c']['bytes']) for r in records)
    report = dict(files=len(records), versions=versions, files_with_additional_regions=extra,
                  scope='Observed RFA envelopes; trailing-region and keyframe semantics unclassified', motions=records)
    (root / 'artifacts/motion-structure.json').write_text(json.dumps(report, indent=2))
    print({k:v for k,v in report.items() if k != 'motions'})

if __name__ == '__main__': main()
