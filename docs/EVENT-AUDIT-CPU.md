# Live event diagnostic scans

Post-16:00 source-only delta. No builds, tests or XEMU runs performed.

After the real event tick, the scene scanned the whole event owner once to
build death-watch telemetry and again to build Switch telemetry. It also
scanned force owners for their telemetry. These arrays feed diagnostic output,
not event scheduling, physics, collision, save state or rendering.

The repeated snapshots now run only at frame zero, when the existing diagnostic
switch is enabled, or when the explicit death-watch fixture UID is nonzero.
The separate startup force/Switch snapshots are unchanged. Existing fixtures
still default to diagnostics enabled; the explicit watch remains observable
even with live checksums disabled. All real event polling, countdowns, delayed
actions, death/explosion effects, hit-signal clearing and tick counters remain
outside the gate and execute in their original order every frame.

No allocation, index, cached gameplay state, save-format change or scheduling
change is introduced. The16:00 event stage measured2.619ms after the earlier
contact-geometry cache, but the cost of these remaining audit scans and this
change's saving have not been measured.
