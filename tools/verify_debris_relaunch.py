"""Check a retained stock-memory two-blast run for reactivated old debris."""
import argparse
import json
from pathlib import Path


def verify(report):
    assert report['result'] == 'PASS'
    assert report['level'] == 'glass_house.rfl' and report['archive'] == 'levelsm.vpp'
    assert report['memory'] == {'base-memory': 67108864, 'plugged-memory': 0}
    checks = report['checks']
    for platform in ('pc', 'xbox'):
        state = checks['DEBRIS_RELAUNCH_STATE'][platform]
        assert state[0] >= 2 and state[1] > 0 and state[2] >= state[3] > 0, (platform, state)
        assert state[4] != 0 and state[5] != state[6], (platform, state)
        assert checks['ROCKETS'][platform][:3] == [2, 2, 0]
    assert all(check['equal'] for check in checks.values())
    return dict(result='PASS', frames=report['frames'],
                relaunch=checks['DEBRIS_RELAUNCH_STATE']['xbox'],
                scope='Two impacts and at least one previously settled fragment assigned '
                      'nonzero velocity before new fragment spawning; no animation-quality claim')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(json.loads(args.report.read_text())), indent=2))
