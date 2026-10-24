"""Run paced hardware recipes under bottleneck experiments and summarise speed.

Usage: python tools/perf_matrix.py OUT.json LEVEL:ARCHIVE[,..] EXPERIMENT[,..]
Each run uses the broad exercise input at the authored player start. The
summary is the median game-step rate, median drawn rate and median CPU time
per drawn frame waiting on the GPU over the live samples.
"""
import glob
import json
import statistics
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def summarise(run):
    rows = {}
    for f in sorted(glob.glob(str(Path(run) / 'ram_poll_*.txt'))):
        for line in open(f):
            if line.startswith('RFLOG '):
                try:
                    s = json.loads(line[6:])
                except ValueError:
                    continue
                if s.get('rf_xbox_frame_rate') and s.get('rf_scene_crash_activity', [0])[0]:
                    rows[s['rf_xbox_frame_rate'][0]] = s
    samples = [rows[k] for k in sorted(rows)]
    steps, drawn, wait, frame_ms = [], [], [], []
    for a, b in zip(samples, samples[1:]):
        fa, fb = a['rf_xbox_frame_rate'], b['rf_xbox_frame_rate']
        dt = (fb[0] - fa[0]) / 1000
        if dt <= 0 or fb[2] <= fa[2]:
            continue
        steps.append((fb[2] - fa[2]) / dt)
        d = fb[3] - fa[3]
        drawn.append(d / dt)
        if d > 0:
            frame_ms.append(dt * 1000 / d)
        wa, wb = a.get('rf_xbox_gpu_wait'), b.get('rf_xbox_gpu_wait')
        if wa and wb and wb[10] > wa[10]:
            wait.append(sum(wb[i] - wa[i] for i in range(10)) * 1024 / 733000 / (wb[10] - wa[10]))
    med = lambda v: round(statistics.median(v), 1) if v else None
    return {'steps_per_s': med(steps), 'drawn_per_s': med(drawn), 'ms_per_drawn': med(frame_ms),
            'gpu_wait_ms_per_drawn': med(wait), 'slowest_steps_per_s': round(min(steps), 1) if steps else None,
            'samples': len(steps)}


def main():
    out = Path(sys.argv[1])
    levels = [x.split(':') for x in sys.argv[2].split(',')]
    experiments = [int(x) for x in sys.argv[3].split(',')]
    extra = sys.argv[4:]
    results = json.loads(out.read_text()) if out.exists() else []
    for level, archive in levels:
        for experiment in experiments:
            proc = subprocess.run([sys.executable, str(ROOT / 'tools/xemu_crash_hunt.py'), '--target', 'hardware',
                                   '--level', level, '--archive', archive, '--input', 'exercise', '--aim', '--paced',
                                   '--frames', '1800', '--seconds', '300', '--experiment', str(experiment)] + extra,
                                  cwd=ROOT, capture_output=True, text=True)
            last = (proc.stdout.strip().splitlines() or [''])[-1]
            try:
                summary = json.loads(last)
            except ValueError:
                summary = {'status': 'HARNESS_ERROR', 'raw': (proc.stdout + proc.stderr)[-800:]}
            row = {'level': level, 'experiment': experiment, 'status': summary.get('status'), 'run': summary.get('run')}
            if summary.get('run'):
                row.update(summarise(summary['run']))
            results.append(row)
            out.write_text(json.dumps(results, indent=1))
            print(json.dumps(row), flush=True)
            time.sleep(1.2)


if __name__ == '__main__':
    if len(sys.argv) == 2 and Path(sys.argv[1]).is_dir():
        print(json.dumps(summarise(sys.argv[1])))
    else:
        main()
