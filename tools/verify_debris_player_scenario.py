"""Require actual health loss and suppression controls, not merely equal zero counters."""
import json
from pathlib import Path
import sys

def verify(report):
    checks=report['checks']
    for platform in ('pc','xbox'):
        test=checks['DEBRIS_PLAYER_TEST'][platform]
        # Start health100; speed5/radius.5 -> damage1.25; armor absorbs.65.
        # Controls reject wrong room, prior suppression, then next-frame repeat.
        assert test==[1120403456,1120324813,1120318259,2,1,1,32768,1],(platform,test)
        live=checks['DEBRIS_PLAYER'][platform]
        assert live==[1,1,1,1067450368,0,2,1120324813,4],(platform,live)
    return dict(result='PASS',scope='Explicit scene contact fixture: health/armor, room/flag gates, repeat suppression and player direction. Ordinary flying-fragment contact and impact particles remain unverified.')

if __name__=='__main__':
    print(json.dumps(verify(json.loads(Path(sys.argv[1]).read_text())),indent=2))
