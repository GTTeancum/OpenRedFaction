"""Per-second step rate, per-phase milliseconds and walk-in-place from a crash run.

Usage: python tools/crash_profile.py RUN_DIR
Profile rows are cumulative (count, total ms lo/hi, max); this prints the
average milliseconds per game step each phase used between log samples.
"""
import glob
import json
import sys

PHASES = {
    'rf_scene_profile': ['-', 'input>world', 'world', 'world>view', 'view', 'present', 'present>step', 'step'],
    'rf_scene_step_profile': ['particles', 'physics', 'lights', 'events', 'fx', 'npc', 'teleport', 'views'],
    'rf_scene_world_profile': ['w:camera-fx', 'w:ambient', 'w:camera', 'w:locate', 'w:visibility', 'w:summary', 'w:mesh-build', '-'],
    'rf_scene_world_build_profile': ['b:retained', 'b:project', 'b:terrain', 'b:capture'],
    'rf_scene_presentation_profile': ['p:npcs', 'p:clutter', 'p:weapons', 'p:pickups', 'p:viewmodel', 'p:gpu-frame', 'p:post', 'p:vehicles/projectiles/debris'],
    'rf_scene_npc_step_profile': ['ai', 'medic/masako', 'rooms/liquid', 'glare/clutter', 'attach', 'attach-fx', 'glare-rooms', '-'],
}


def total(row):
    return (row[2] << 32) + row[1]


rows = {}
for f in sorted(glob.glob(sys.argv[1] + '/ram_poll_*.txt')):
    for line in open(f):
        if line.startswith('RFLOG '):
            try:
                s = json.loads(line[6:])
            except ValueError:
                continue
            fr = s.get('rf_xbox_frame_rate')
            if fr:
                rows[fr[0]] = s
samples = [rows[k] for k in sorted(rows)]
for a, b in zip(samples, samples[1:]):
    fa, fb = a['rf_xbox_frame_rate'], b['rf_xbox_frame_rate']
    dt = (fb[0] - fa[0]) / 1000
    steps = fb[2] - fa[2]
    if dt <= 0 or steps <= 0:
        continue
    parts = []
    for name, labels in PHASES.items():
        if name not in b:
            continue
        ra, rb = a[name], b[name]
        for i, label in enumerate(labels):
            ms = total(rb[i * 4:i * 4 + 3]) - total(ra[i * 4:i * 4 + 3])
            if label != '-' and ms / steps >= 1:
                parts.append('%s %.1f' % (label, ms / steps))
    wa, wb = a.get('rf_xbox_gpu_wait'), b.get('rf_xbox_gpu_wait')
    if wa and wb:
        names = ['particle-reset', 'hud-reset', 'hud-end', 'stream-start', 'lightmap', 'skybox', 'audit', 'error', 'swap-queue', 'vblank']
        drawn = max(1, wb[10] - wa[10])
        waits = ['%s %.1f' % (n, (wb[i] - wa[i]) * 1024 / 733000 / drawn) for i, n in enumerate(names) if (wb[i] - wa[i]) * 1024 / 733000 / drawn >= .3]
        parts.append('| gpu-wait ms/drawn: ' + (', '.join(waits) or '-'))
    walk = b.get('rf_scene_npc_walk_in_place', [0] * 8)
    print('t%5.0f steps/s %4.1f drawn/s %4.1f | ms/step: %s | walk-in-place windows %d npcs %d last %d' % (
        fb[0] / 1000, steps / dt, (fb[3] - fa[3]) / dt, ', '.join(parts), walk[1], walk[2], walk[3]))
