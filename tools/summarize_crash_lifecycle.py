"""Summarize existing application telemetry without connecting to an emulator.

Counts are observations from non-atomic samples, not a gameplay pass verdict.
"""
import argparse
import json
from pathlib import Path


def summarize(path):
    events = []
    intervals = []
    pending = None
    prior = None
    samples = 0
    for line in path.read_text().splitlines():
        row = json.loads(line)
        activity = row.get('rf_scene_crash_activity')
        trace = row.get('rf_scene_crash_scenario_trace')
        if not activity or not trace or not trace[0]:
            continue
        samples += 1
        key = (activity[2], activity[3])
        if key != prior:
            events.append(dict(seconds=row['elapsed_seconds'], scene_starts=key[0],
                               deaths=key[1], live_ticks=trace[2], dead_ticks=trace[3]))
            prior = key
        if trace[1] == 7:
            if pending is None:
                pending = dict(first_dead_seconds=row['elapsed_seconds'],
                               scene_at_death=activity[2], live_ticks_at_death=trace[2],
                               dead_samples=0, live_tick_changes_in_dead_samples=0)
            pending['dead_samples'] += 1
            pending['live_tick_changes_in_dead_samples'] += trace[2] != pending['live_ticks_at_death']
        elif pending is not None and trace[2] > pending['live_ticks_at_death']:
            pending.update(resumed_seconds=row['elapsed_seconds'],
                           scene_after_recovery=activity[2],
                           live_ticks_after_recovery=trace[2],
                           reload_observed=activity[2] > pending['scene_at_death'])
            intervals.append(pending)
            pending = None
    if pending is not None:
        pending['resumed_seconds'] = None
        intervals.append(pending)
    return dict(source=str(path), samples=samples, transitions=events,
                death_intervals=intervals,
                recovered_intervals=sum(i.get('resumed_seconds') is not None for i in intervals),
                limitation='Sampled, non-atomic application telemetry; no visual/audio or stability verdict.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('telemetry', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = json.dumps(summarize(args.telemetry), indent=2)
    if args.output:
        args.output.write_text(result + '\n')
    else:
        print(result)
