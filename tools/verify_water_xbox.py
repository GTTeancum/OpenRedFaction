"""Assert the authored 180-frame water scenario from retained native evidence."""
import argparse
import json
from pathlib import Path


def verify(report):
    assert report['frames'] == 180, 'Water lifecycle scenario requires 180 frames'
    assert report['level'] == 'dm03.rfl' and report['archive'] == 'levelsm.vpp'
    assert report['memory'] == {'base-memory': 67108864, 'plugged-memory': 0}
    checks = report['checks']
    for platform in ('pc', 'xbox'):
        assert checks['ROCKET_LIQUID_STATE'][platform][:2] == [1, 4], platform
        assert checks['RIPPLE_LIFECYCLE'][platform] == [1, 1, 0, 0], platform
        assert checks['ROCKETS'][platform][:3] == [1, 1, 0], platform
        assert checks['ROCKET_BLAST'][platform][0] == 1, platform
        assert checks['RIPPLE_VISUAL'][platform][:4] == [180, 0, 0, 0], platform
        assert checks['RIPPLE_VISUAL'][platform][6] == 0, platform
    assert all(check['equal'] for check in checks.values()), 'PC/Xbox mismatch'
    return dict(result='PASS', scope='One liquid entry, subsequent solid impact/blast, '
                'one non-fixture ripple and expiry on stock64MiB; '
                'endpoint does not establish active ripple visual parity or audio')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    args = parser.parse_args()
    report = json.loads(args.report.read_text())
    assert report['result'] == 'PASS'
    print(json.dumps(verify(report), indent=2))
