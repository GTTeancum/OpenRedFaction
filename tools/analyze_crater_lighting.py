"""Summarize an opt-in completed live crater light audit, without image claims."""
import argparse
import collections
import csv
import json
import math
from pathlib import Path
import statistics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    args = parser.parse_args()
    with args.csv.open() as source:
        metadata = source.readline().strip()
        remaining = list(source)
        lights = [json.loads(line[len('# source='):]) for line in remaining if line.startswith('# source=')]
        rows = list(csv.DictReader(line for line in remaining if not line.startswith('#')))
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
    if lights:
        result['admitted_lights'] = lights
        for light in lights:
            if light['type'] != 2:
                continue
            distances = [math.dist(light['position'], [float(row['p'+axis]) for axis in 'xyz'])
                         for row in rows]
            light['sample_distances'] = dict(minimum=min(distances), maximum=max(distances),
                                            inside_radius=sum(d < light['radius'] for d in distances),
                                            total=len(distances),
                                            scope='Geometric point-light radius only; does not include profile, normal or shadows')
    if 'blocked_authored' in rows[0]:
        fields = ('blocked_authored', 'blocked_generated', 'excluded_flags_portal',
                  'excluded_alpha', 'excluded_coplanar')
        for row in rows:
            assert int(row['blocked_authored'])+int(row['blocked_generated']) == int(row['blocked'])
        result['first_accepted_blockers'] = {field:sum(int(row[field]) for row in rows) for field in fields}
        result['blocker_limitation'] = ('Traversal-first accepted hit, not necessarily nearest. Only listed eligibility exclusions are checked; '
                                       'mapping ownership, bounds, shadow-volume clipping and projected raster coverage are not assessed.')
    if 'projected_r' in rows[0]:
        result['projected_comparison'] = dict(
            mean_red=statistics.mean(float(r['projected_r']) for r in rows),
            brighter_red=sum(float(r['projected_r'])>float(r['shadow_r'])+1e-6 for r in rows),
            darker_red=sum(float(r['projected_r'])<float(r['shadow_r'])-1e-6 for r in rows),
            limitation='Recovered projection/cull/raster helpers on port per-face grids; zero receiver-area threshold. Original grouping, density and traversal ownership unverified. No live rendering change.')
    toward_room = [r for r in rows if float(r['nx'])>.9]
    if toward_room: result['normal_x_over_point_nine'] = summary(toward_room)
    args.csv.with_suffix('.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    assert not mismatches, 'Stored atlas differs from recomputed shadow lighting'


if __name__ == '__main__': main()
