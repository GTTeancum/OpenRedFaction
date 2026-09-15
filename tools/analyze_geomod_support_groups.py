"""Summarize supporting-plane pairs without claiming geometric edge closure."""
import argparse
import csv
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    rows = list(csv.DictReader(args.csv.open()))
    groups, mixed = {}, set()
    for row in rows:
        face, edge = int(row['face_support']), int(row['edge_support'])
        if face == 65535:
            mixed.add(int(row['face']))
            continue
        groups.setdefault(tuple(sorted((face, edge))), []).append(row)
    result = dict(vertices=len(rows), faces=len({r['face'] for r in rows}),
        mixed_support_faces=sorted(mixed), support_pairs=len(groups),
        pairs=[dict(supports=pair, edges=[[int(r['face']), int(r['corner'])] for r in group])
               for pair, group in sorted(groups.items())],
        scope='Shared supporting lines only; membership does not prove interval overlap, opposite winding, or closure.')
    args.out.write_text(json.dumps(result, indent=2)+'\n')
    print('vertices', result['vertices'], 'faces', result['faces'],
          'support pairs', result['support_pairs'], 'mixed faces', len(mixed))


if __name__ == '__main__':
    main()
