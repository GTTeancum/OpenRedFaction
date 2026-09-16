# Authored post reset and continuation

The ctf06 development fixture uses the actual player body to admit restoration
of the original post. Reset must reject a player standing inside the destroyed
post, restore collision when safe, and allow subsequent ordinary rocket edits.

`tools/replay_authored_post_reset.py` extends the retained two-shot recording:

| Case | Frames | Required outcome |
| --- | ---: | --- |
| reset-safe | 640 | zero cuts, publication serial3, player survives standing up |
| reset-blocked | 730 | two cuts/serial2 retained; original post overlaps body |
| reset-recut | 840 | reset then third rocket produces one cut/serial4 |
| reset-restored | 805 | same forward inputs as open-post control now stop at restored post |

`tools/run_authored_post_controls.py` launches only headless PC game processes,
clears inherited replay options, and exports histories/publications. The verifier
requires completed runs, expected edit counters, player position, health, armor,
weapon/ammunition state, and byte-identical rejected-reset history/publication.
These checks do not independently establish rendered appearance or Xbox parity.

## Exposed movement failure

The initial safe reset committed successfully at frame580. Frame581 returned
RF_NOT_FOUND from player stance. Failure-only instrumentation narrowed it to
standing clearance with flags4100 (0x1004), moving from
(4.45000124,-0.401361138,2.5) to (4.45000124,0.335426629,2.5).
The blocked reset exposed the same next-frame failure; it was not a failure of
the accepted reset transaction or a missing animation resource.

Original499ed0 constructs body+124 OR4. The body collision adapter used a room
sweep that rejects liquid query flags despite having owned liquid metadata.
The intended repair preserves the query and routes through the existing
liquid-aware room sweep when bit1000 is set. Dry queries without that bit retain
the old adapter; absent liquid metadata and unsupported alpha queries still
fail explicitly.

## Verified PC results

The geometry_liquid_overlay regression passes dry standing with1004, real water
contact and source mapping, flags4 solid contact, absent-metadata dry operation,
and atomic failure for unsupported queries. All four gameplay controls complete
their full recorded lengths and pass `controls-verification.json`.

The restored-post walk stops at X-4.147538185119629, matching the intact-post
control. The previously verified destroyed-post walk reaches X-6.346451282501221.
The rejected reset preserves both RGCH and RGP1 exports byte-for-byte against
the two-shot baseline. Safe reset ends at zero cuts/serial3; another ordinary
rocket produces one cut/serial4. Health and armor stay100 throughout.

The native-resolution PC outputs were inspected: safe reset visibly restores
the post, and recut removes an upper section while keeping adjacent beams,
floor, water, lighting, weapon and HUD. This is bounded DEV fixture evidence,
not general authored destruction coverage.

## Xbox continuation

`artifacts/xemu/render-20260916-114703/report.json` passes56 comparisons after
840frames on stock64MiB. The ordinary controls cut twice, safely reset, stand,
and cut again, ending with one cut/serial4. Endpoint free memory is4226pages
(16.5078125MiB). The native framebuffer was inspected and shows the upper post
cut with surrounding beams/floor/water, weapon and HUD intact. The harness
restored staged disc contents and exited its owned emulator. Unsafe-reset and
restored-post walking scenarios are verified on PC only so far.
