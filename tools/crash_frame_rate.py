"""Per-run frame-rate summary from rf_xbox_frame_rate RAM-log samples."""
import glob, json, os, statistics, sys

for run in sys.argv[1:]:
    rows = {}
    for f in sorted(glob.glob(run + '/ram_poll_*.txt')):
        for line in open(f):
            if not line.startswith('RFLOG '):
                continue
            try:
                s = json.loads(line[6:])
            except ValueError:
                continue
            fr, a = s.get('rf_xbox_frame_rate'), s.get('rf_scene_crash_activity')
            if fr and a and a[0] + a[1] > 0:
                rows[fr[0]] = (fr, a[0] + a[1])
    rs = [rows[k] for k in sorted(rows)]
    sim, drawn = [], []
    for (f0, i0), (f1, i1) in zip(rs, rs[1:]):
        dt = (f1[0] - f0[0]) / 1000
        if dt <= 0 or i1 <= i0:
            continue
        sim.append((i1 - i0) / dt)
        if f1[3] > f0[3]:
            drawn.append((f1[3] - f0[3]) / dt)
    if sim:
        print(os.path.basename(os.path.normpath(run)), 'game steps/s median %.1f (min %.1f max %.1f)' % (
            statistics.median(sim), min(sim), max(sim)),
            ('drawn fps median %.1f' % statistics.median(drawn)) if drawn else 'drawn = one per step', '%d samples' % len(sim))
