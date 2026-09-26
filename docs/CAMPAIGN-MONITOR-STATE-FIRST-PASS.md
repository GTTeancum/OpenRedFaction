# Campaign Monitor_State first pass

The installed 68 campaign levels contain 65 `Monitor_State` events. Every
authored link in those events resolves to one of 56 distinct linked `camera2`
entities or 59 distinct linked screen/mirror props (`smallscreen`, `Mirror01`,
`Screen03`).
Some events link only a camera and therefore have no monitor prop to configure.
This inventory comes from the installed VPP/RFL records, not a screenshot.

In original `RF.exe` SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`,
the type-49 factory at `0x4B69D0` creates the class whose ON vtable entry is
`0x4BA980` and OFF entry is the base `0x4B9F80` no-op for this type. `0x4BA980`
visits linked monitor props, applies monitor mode and refresh through `0x412600`,
then visits linked camera actors through `0x4126A0`. The latter retains the
first camera handle for an unbound monitor. `0x412600` replaces a nonpositive
refresh interval with 8 seconds. Other effects on the linked camera actor and
the rendered-image cache remain unimplemented.

The shared event dispatcher now sends ON actions to the scene monitor owner.
That owner keeps a bounded screen-to-first-camera association and refresh
interval per level; OFF remains a no-op. Raw secondary and header fields are
retained without invented labels. The table resets before startup events in
each section. A focused installed-level test binds L6S3 Mirror01 UID 6179 to
camera2 UID 5881, and smallscreen UID 3799 to camera2 UID 6581; camera-only
links leave the table unchanged. An 80-frame PC replay submits 12 bindings
with 12 camera owners. Forcing event 6177 adds one ON update; the final table
hash remains identical because that event was already applied at startup.
The same 80-frame `--no-images` run passes on stock 64 MiB XEMU: all five
`MONITOR_BINDINGS` words match PC, and 5,183 physical pages (20.25 MiB) remain
available. The run produced no visual image files.

The ordinary environment checkpoint now writes `RFEN5`: it adds the bounded
screen/camera association table, raw authored fields, refresh intervals and
monitor event counters after the existing player-form record. Restore checks
that each screen and camera UID belongs to the loaded section, rejects
duplicate screens and invalid intervals, and publishes the table only after
the other checkpoint components validate. `RFEN1` through `RFEN4` remain
readable. Queued type-49 events now retain their timers through the existing
event checkpoint; other unsupported pending effects remain blocked.

The focused L6S3 fixture restores two real authored bindings and rejects a
tampered camera UID. An image-free 80-frame live L6S3 save on PC and stock
64 MiB XEMU produced byte-identical 65,924-byte world snapshots, including a
464-byte environment section; XEMU retained 5,183 physical pages (20.25 MiB).
Loading the saved PC snapshot restores all five `MONITOR_BINDINGS` words.
The run is recorded at `artifacts/xemu/render-20260926-175911/report.json`.
No visual image files were created.

This is event and association state, not live monitor imagery. Camera actor
side effects, texture render/cache and monitor material submission remain
open. A queued type-49 event's post-load firing still needs a live campaign
fixture beyond the event-codec and binding-restore checks.
