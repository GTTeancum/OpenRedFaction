"""Quantify matched PC/native captures; this is evidence, not a parity gate."""
import argparse
from collections import Counter
import json
from pathlib import Path
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    with Image.open(args.folder / 'pc-final.ppm') as source:
        pc = source.convert('RGB')
    with Image.open(args.folder / 'framebuffer.png') as source:
        xbox = source.convert('RGB')
    if pc.size != xbox.size:
        raise ValueError('Capture dimensions differ')
    width, height = pc.size
    a, b = pc.tobytes(), xbox.tobytes()
    differences = [max(abs(a[i+j] - b[i+j]) for j in range(3))
                   for i in range(0, len(a), 3)]

    def region(left, top, right, bottom):
        counts = Counter(differences[y*width+x]
                         for y in range(top, bottom) for x in range(left, right))
        return dict(bounds=[left, top, right, bottom], pixels=sum(counts.values()),
                    different=sum(v for k, v in counts.items() if k),
                    over_one=sum(v for k, v in counts.items() if k > 1),
                    over_sixteen=sum(v for k, v in counts.items() if k > 16),
                    maximum=max(counts, default=0), histogram=dict(sorted(counts.items())))

    result = dict(size=[width, height], full=region(0, 0, width, height),
                  limitation='No registration or causal attribution; differing pixels require visual review.')
    if (width, height) == (640, 480):
        result['upper_world'] = region(0, 0, 640, 350)
        result['lower_weapon_hud'] = region(0, 350, 640, 480)
    (args.folder / 'pixel-comparison.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k: {n: v for n, v in row.items() if n != 'histogram'}
                      for k, row in result.items() if isinstance(row, dict)}, indent=2))


if __name__ == '__main__':
    main()
