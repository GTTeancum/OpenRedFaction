"""Compare native guest framebuffer with the shared-mesh PC rasterizer.

This checks the untextured diagnostic view, not original-game/PS2 fidelity.
Allow minor channel rounding and a small number of rasterization edge pixels.
"""
import argparse
import json
from pathlib import Path
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference', type=Path)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    with Image.open(args.reference) as source:
        reference = source.convert('RGB')
    with Image.open(args.capture) as source:
        capture = source.convert('RGB')
    if reference.size != (640, 480) or capture.size != reference.size:
        raise SystemExit('Expected two 640x480 diagnostic frames')
    a, b = reference.tobytes(), capture.tobytes()
    errors = [max(abs(a[i+c]-b[i+c]) for c in range(3))
              for i in range(0, len(a), 3)]
    bad = sum(error > 3 for error in errors)
    # Up to 0.1% edge pixels; the old W-buffer regression exceeds this bound.
    passed = bad / len(errors) <= 0.001
    report = dict(reference=str(args.reference), capture=str(args.capture),
                  pixels=len(errors), pixels_over_3=bad,
                  maximum_channel_error=max(errors),
                  mean_maximum_channel_error=sum(errors)/len(errors),
                  result='PASS' if passed else 'FAIL')
    args.report.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()
