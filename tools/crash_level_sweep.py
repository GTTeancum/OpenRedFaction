"""Load every campaign level once through the crash harness and record outcomes.

Usage: python tools/crash_level_sweep.py OUT.json [harness args...]
Each level starts at its authored player start with the broad exercise input.
Results append per level so an interrupted sweep resumes where it stopped.
"""
import json
import os
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def campaign_levels():
    levels = []
    for archive in ('levels1.vpp', 'levels2.vpp', 'levels3.vpp'):
        data = (ROOT / 'Installed_Game' / archive).read_bytes()
        count, = struct.unpack_from('<I', data, 8)
        for i in range(count):
            name = data[2048 + i * 64:2048 + i * 64 + 60].split(b'\0')[0].decode()
            if name.lower().endswith('.rfl'):
                levels.append((archive, name))
    return levels


def main():
    out = Path(sys.argv[1])
    extra = sys.argv[2:]
    results = json.loads(out.read_text()) if out.exists() else []
    done = {(r['archive'], r['level']) for r in results}
    for archive, level in campaign_levels():
        if (archive, level) in done:
            continue
        started = time.time()
        proc = subprocess.run([sys.executable, str(ROOT / 'tools/xemu_crash_hunt.py'), '--level', level,
                               '--archive', archive, '--input', 'exercise'] + extra,
                              cwd=ROOT, capture_output=True, text=True)
        last = (proc.stdout.strip().splitlines() or [''])[-1]
        try:
            summary = json.loads(last)
        except ValueError:
            summary = {'status': 'HARNESS_ERROR', 'raw': (proc.stdout + proc.stderr)[-1500:]}
        results.append({'archive': archive, 'level': level, 'seconds': round(time.time() - started),
                        'summary': summary})
        out.write_text(json.dumps(results, indent=1))
        print(level, summary.get('status'), flush=True)
        time.sleep(1.2)


if __name__ == '__main__':
    main()
