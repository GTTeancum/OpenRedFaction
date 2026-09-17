# Moving debris water crossings

The live debris loop now tests the recovered retained-room liquid crossing after
a solid miss. A strict downward crossing requests the existing ripple visual at
the historical reverse-interpolated point, then commits the proposed movement
endpoint. It does not move the fragment onto that unusual ripple point, decrement
bounces or alter velocity for a water entry. Existing gravity proceeds afterward.
Solid contacts still take precedence. Birth placement and underwater displacement
remain separate operations.

Original 48f99e calls 48fc10. Its wet result has a null face; 48f9b6..48f9d0
requests effect global8568b0 with retained room, null direction, wet point,
size0.2, selector-1,0,1. Then 48f9d8 commits the proposed endpoint.
The reconstructed path submits the shared bounded ripple visual. It does not
implement the full generic effect/audio dispatcher. The request's size0.2 is
recorded; it is not used to invent a VFX mesh scale (the generic VFX setup uses
unit animation scale). Broader effect branches remain outside this change.

## Executed original evidence

Run `python tools/check_live_wet_debris.py`, followed by
`python tools/check_live_debris_crossing.py` against the current PC build.
The first produces the two-blast trace; the second executes original 48f900 for
every accepted live crossing, including real 4ce080, vector helpers and48fc10.
The original executable SHA256 is checked. Only the solid-world query is supplied
as a miss after the reconstructed world reports a miss, and effect submission is
recorded. No original game process, screenshots or desktop input are used.

All 35 actual crossing inputs produce bit-identical proposed endpoints and ripple
request positions. Original request size/room/flags are checked, and original
velocity and room remain unchanged. This proves the local post-solid-miss branch,
not equivalence of the two engines' world collision queries. Results are in
`artifacts/debris-motion/live-crossing.json` with the trace and executable hashes.

PC telemetry is `[2699,35,3,2369043722,1045220557,0,0,0]`: solid misses, accepted
entries, last room, last ripple point hash, size bits and status. The bounded
ripple pool reports35 starts,16 expirations and3 replacements, with16 active at
the550-frame endpoint. Audio and every-frame visual correctness remain unverified.

## Native acceptance and precision fixes

Instrumented run `artifacts/xemu/render-20260917-002832` passes all62 selected
PC/Xbox comparisons on stock64MiB, including35 crossings, debris audio and the
ripple geometry hash. Its192 emitted vertices (2688 words) match exactly, as do
camera, all16 ripple source records, captured preprojection triangles, local
animation samples and floating-point state. `tools/compare_ripple_capture.py`
compares these fields without tolerance acceptance or dropping a hash check.

Two separate precision issues were isolated. Liquid-height integer conversion
changed the native mixed x87/SSE arithmetic path; e8387dd1 removes that temporary
mode change while preserving mathematical truncation. See
GEOMOD-LIQUID-ROUNDING-ISOLATION-20260917.md for the first-motion-step evidence.
The remaining ripple discrepancy was world placement: before the fix,24 captured
world-input words differed despite identical cameras, effect positions and ages.
Explicit double addition with a final float store now makes local samples,
world-space triangles and every emitted vertex word agree. The sample data and
animation implementation were not replaced; no precision threshold was relaxed.

`--capture-ripple` captures ordinary gameplay effects without enabling the
separate render-only fixture. Captures append240 local-space floats after the
existing floating-point-state block. Temporary motion and audio ledgers used to
isolate the divergence have been removed from the scene and harness. Only the
small crossing counters and ripple-source capture remain.

Remaining scope: audible entry effects, full generic effect dispatch, wider liquid
rooms/trajectories, every-frame visual inspection and physical hardware testing.
Endpoint/state captures do not prove those requirements.

Clean-build acceptance: all115 CTests pass after a full rebuild. Run
`artifacts/xemu/render-20260917-003132`, with the temporary ledgers removed,
again passes62 state comparisons and every ripple capture word exactly.
The stock64MiB endpoint has4159 free pages (16.246 MiB), restores the staged disc
and exits. The3884-byte checkpoint remains unchanged with SHA256
`7e6900b99a39ce38dc0154b70e95b601b9bd8816fe837cf1198ccb4857d5fb94`.
The inspected framebuffer retains the damaged post, scattered fragments, room,
weapon and HUD. Individual transient ripple visibility remains outside this
endpoint-only inspection; no screenshot was uploaded to GitHub.
