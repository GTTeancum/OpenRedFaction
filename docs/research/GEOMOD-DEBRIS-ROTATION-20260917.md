# Incremental debris orientation

The previous renderer reapplied an accumulated angle around the current spin
axis. A bounce changes that axis, so it also reinterpreted all prior rotation.
Chunks now retain a nine-float orientation matrix, initialized to identity at
birth and preserved on relaunch. Each moving update composes an incremental
rotation after collision and gravity; a terminal collision freezes orientation.
Rendering transforms local vertices using the retained matrix.

## Binary evidence

`tools/probe_debris_rotation.py` executes RF.exe `48f64a..48f678`, with real
`4fbf30`, `40ea80` and `40a3b0` helpers. Input register setup supplies the chunk
and the preceding gravity span's EAX/ECX values. The executable SHA-256 is
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

The angle is float(dt * spin). Quaternion intermediates and matrix composition
preserve recovered rounding and summation order. In row-major storage the
composition is increment * prior basis. This routine does not normalize axes.

The generated fixture covers 180 combinations of bases, axes, spin and timestep,
plus 90 accumulated updates with changing axes. All 270 results match the shared
C implementation exactly, including in-place output.

`--live-log artifacts/debris-player-live/ordinary.log` checks every traced matrix
against the original span. All 1,353 updates match, with two ordinary flying
fragment contacts still occurring at frames 287 and 291 and final health
42.393387. This probe verifies rotation for supplied live inputs; it does not
execute original world collision or rendering.

## Resource and validation scope

Replacing one angle with nine floats costs 2,560 bytes across the fixed 80-chunk
pool. Four telemetry words record update count, last matrix hash, ordered matrix
hash and status for native comparison. Detailed input/output traces are opt-in.

Numerical agreement establishes orientation behavior, not all-frame visual
acceptance. Broader bounce/relaunch visual coverage remains open.


Native acceptance: `artifacts/xemu/render-20260917-013419` passes all 67
comparisons over 400 frames on stock 64 MiB. Rotation telemetry is identical:
`[1353, 699426666, 119619872, 0]`. There are 4,083 free pages (15.949 MiB),
one page less than the prior run. The ordinary player-hit verifier also passes;
fixture injection remains disabled. The endpoint capture shows the approached
post, surrounding geometry, weapon and reduced health HUD; it does not prove
all intervening bounce visuals. The disc was restored and the owned emulator
exited. All 115 PC CTests pass (`rotation-full-ctest.log`).


## Repeated-blast continuation

`python tools/check_debris_relaunch_rotation.py --run` generates the ordinary
`glass_house.rfl` two-shot route, clears fixture/replay environment overrides,
and verifies the opt-in live trace. Slot 0's last moving update is frame 412;
it rests through frame 472 and resumes at frame 473. Its nine orientation words
are unchanged across relaunch and are the next update's exact input. The next
output changes, establishing resumed rotation. This is a state check, not a
claim that every frame has been visually inspected.

All 2,096 rotation samples in `artifacts/debris-relaunch/rotation.log` also
match original instructions via `tools/probe_debris_rotation.py --live-log`.
The current authored-post two-shot route does not relaunch settled fragments;
new-fragment generation alone must not be accepted for this requirement.

Stock 64 MiB run `artifacts/xemu/render-20260917-013918` passes 66 comparisons
at 550 frames, with 8,322 free pages (32.508 MiB) in this smaller test level.
Relaunch state is `[2,3,1,1,228786469,3892924275,75152948,0]` and rotation
state is `[2096,72323296,1827719996,0]` on both platforms. The independent
`tools/verify_debris_relaunch.py` positive-reactivation check passes. Input SHA:
`0ee89c4d7d321f8a7e2dba2a898f08683b6fd2652461bac9597d309944f0fcbb`.
Disc restoration succeeds and the owned emulator exits. The inspected native
endpoint shows the double-cut crater, residual smoke, weapon and intact HUD;
dark crater readability remains unresolved. This trace-only follow-up changes
no gameplay math; the preceding 115-test acceptance remains applicable.
