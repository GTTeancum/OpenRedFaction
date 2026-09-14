"""Read sustained guest timing without input, pausing, capture, or auto-close."""
import argparse
import json
from pathlib import Path
import re
import time

from xemu_guest_snapshot import words
from xemu_smoke import Monitor


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('live', type=Path, help='live.json from the owned XEMU launch')
    p.add_argument('--seconds', type=int, default=45)
    p.add_argument('--load-seconds', type=int, default=240)
    p.add_argument('--out', type=Path)
    a = p.parse_args()
    if not 10 <= a.seconds <= 600 or not 1 <= a.load_seconds <= 600:
        p.error('Require10..600 measurement seconds and1..600 loading seconds')
    run = a.live.resolve().parent
    live = json.loads(a.live.read_text())
    mapping = (run / 'main.map').read_text()
    fields = [('rf_diagnostic', 58), ('rf_player_frame_clock', 8), ('scene_actor_body', 77),
              ('rf_xbox_retained_world', 8), ('rf_xbox_retained_models', 8),
              ('rf_xbox_retained_model_kinds', 6), ('rf_scene_pose_sharing', 4)]
    profiles = ['rf_scene_profile', 'rf_renderer_profile', 'rf_scene_npc_playback_profile']
    fields += [(n, 32) for n in profiles]
    addresses = {n: (int(match[1], 16), count) for n, count in fields
                 if (match := re.search('_' + n + r'\s+([0-9a-fA-F]+)', mapping))}
    monitor = Monitor(live['port'])

    def read(name):
        return words(monitor, *addresses[name])

    def sample():
        return dict(time=time.monotonic(), symbols={n: dict(words=read(n)) for n in addresses})

    try:
        memory = monitor.command('query-memory-size-summary')
        if memory['base-memory'] != 64 * 1024 * 1024:
            raise RuntimeError('This measurement requires stock64MiB')
        deadline = time.monotonic() + a.load_seconds
        while time.monotonic() < deadline:
            try:
                d = read('rf_diagnostic')
            except RuntimeError:
                time.sleep(1)
                continue
            if d[0] == 0x52464447:
                if d[2] & 0x80000000:
                    raise RuntimeError(f'Guest failure {d[2]:08x}')
                if d[2] == 5:
                    raise RuntimeError('Session has already completed; use an unlimited play session')
                if d[37] >= 64:
                    break
            time.sleep(1)
        else:
            raise TimeoutError('Session did not reach64 submitted in-level frames')
        before = sample()
        print(f'Measuring {a.seconds} seconds in level; XEMU stays running.', flush=True)
        time.sleep(a.seconds)
        after = sample()
        seconds = after['time'] - before['time']
        left, right = before['symbols'], after['symbols']
        frames = right['rf_diagnostic']['words'][37] - left['rf_diagnostic']['words'][37]
        steps = right['rf_player_frame_clock']['words'][4] - left['rf_player_frame_clock']['words'][4]
        if frames <= 0 or steps < 0 or right['rf_diagnostic']['words'][2] & 0x80000000:
            raise RuntimeError('Guest stopped progressing, failed, or reset counters during the window')
        phase_means = {}
        for name in profiles:
            old, new = left[name]['words'], right[name]['words']
            rows = []
            for i in range(8):
                count = new[i*4] - old[i*4]
                elapsed = ((new[i*4+2] << 32) + new[i*4+1]) - ((old[i*4+2] << 32) + old[i*4+1])
                rows.append(round(elapsed/count, 3) if count > 0 else None)
            phase_means[name] = rows
        summary = dict(seconds=round(seconds, 3), presented_fps=round(frames/seconds, 3),
            simulation_hz=round(steps/seconds, 3), available_mib=right['rf_diagnostic']['words'][44]/256,
            pose_sharing=right.get('rf_scene_pose_sharing', {}).get('words'), phase_mean_ms=phase_means)
        report = dict(summary=summary, before=before, after=after, memory=memory,
            scope='Non-atomic read-only live guest samples. Real user input/camera and host scheduling affect results; '
                  'not a deterministic parity check. Emulator remains running.')
        out = a.out or run / 'sustained-performance.json'
        out.write_text(json.dumps(report, indent=2))
        print(json.dumps(summary, indent=2), flush=True)
        print(out, flush=True)
    finally:
        monitor.close()


if __name__ == '__main__':
    main()
