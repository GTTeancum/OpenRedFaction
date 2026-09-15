"""Summarize an opt-in completed live crater light audit, without image claims."""
import argparse
import collections
import csv
import json
from pathlib import Path
import statistics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    args = parser.parse_args()
    with args.csv.open() as source:
        metadata = source.readline().strip()
        rows = list(csv.DictReader(source))
    assert rows and metadata.startswith('# generation=')
    mismatches = []
    for row in rows:
        rgb = [max(0, int(float(row['shadow_'+c])*255)) for c in 'rgb']
        peak = max(rgb)
        if peak > 255: rgb = [v*255//peak for v in rgb]
        expected = 0x8000 | max(4, rgb[0]>>3)<<10 | max(4, rgb[1]>>3)<<5 | max(4, rgb[2]>>3)
        if expected != int(row['packed']): mismatches.append([row['face'],row['x'],row['y']])

    def summary(samples):
        return dict(samples=len(samples), faces=len({r['face'] for r in samples}),
                    blocked_lights=dict(collections.Counter(r['blocked'] for r in samples)),
                    minimum_packed=sum(int(r['packed'])==0x9084 for r in samples),
                    shadow_reduces_red=sum(float(r['clear_r'])-float(r['shadow_r'])>1e-6 for r in samples),
                    mean_clear_red=statistics.mean(float(r['clear_r']) for r in samples),
                    mean_shadow_red=statistics.mean(float(r['shadow_r']) for r in samples))

    result = dict(metadata=metadata, all=summary(rows), packing_mismatches=mismatches,
                  limitation='All generated-face grid samples, including border samples; not screen-visible pixel coverage or original shadow parity.')
    if 'blocked_authored' in rows[0]:
        fields = ('blocked_authored', 'blocked_generated', 'excluded_flags_portal',
                  'excluded_alpha', 'excluded_coplanar')
        for row in rows:
            assert int(row['blocked_authored'])+int(row['blocked_generated']) == int(row['blocked'])
        result['first_accepted_blockers'] = {field:sum(int(row[field]) for row in rows) for field in fields}
        result['blocker_limitation'] = ('Traversal-first accepted hit, not necessarily nearest. Only listed eligibility exclusions are checked; '
                                       'mapping ownership, bounds, shadow-volume clipping and projected raster coverage are not assessed.')
    toward_room = [r for r in rows if float(r['nx'])>.9]
    if toward_room: result['normal_x_over_point_nine'] = summary(toward_room)
    args.csv.with_suffix('.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    assert not mismatches, 'Stored atlas differs from recomputed shadow lighting'


if __name__ == '__main__': main()
