"""Run authored-post controls inside the PC process; never send host input."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
CASES = ('reset-safe', 'reset-blocked', 'reset-recut', 'reset-restored')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=CASES, action='append')
    args = parser.parse_args()
    folder = ROOT / 'artifacts/authored-post-live'
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='ctf06.rfl', RF_REPLAY_ARCHIVE='levelsm.vpp', RF_REPLAY_DEV_ROOM='1')
    failures = []
    for name in args.case or CASES:
        env['RF_REPLAY_AUTHORED_HISTORY_OUT'] = str(folder / (name + '.rgch'))
        env['RF_REPLAY_AUTHORED_PUBLICATION_OUT'] = str(folder / (name + '.rgp'))
        with (folder / (name + '.log')).open('wb') as output:
            result = subprocess.run([
                str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
                str(ROOT / 'Installed_Game'), str(folder / (name + '.bin')),
                str(folder / (name + '.ppm')),
            ], cwd=ROOT, env=env, stdout=output, stderr=subprocess.STDOUT)
        print(name, 'exit', result.returncode, flush=True)
        if result.returncode:
            failures.append(name)
    if failures:
        print('Failed:', ', '.join(failures))
        return 1
    if not args.case:
        return subprocess.call([sys.executable, str(ROOT / 'tools/verify_authored_post_controls.py')], cwd=ROOT)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
