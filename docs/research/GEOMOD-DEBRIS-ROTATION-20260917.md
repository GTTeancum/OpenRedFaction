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
