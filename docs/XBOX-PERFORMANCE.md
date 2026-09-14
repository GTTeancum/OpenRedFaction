# Xbox performance work

The user reports roughly4FPS. Gameplay performance takes priority over new
systems and visual polish. Stock64MiB and existing gameplay remain requirements.

Baseline replay-20260914-090014 passes the180-frame L1S2->L1S3 crossing.
Both scene and renderer profiles contain104 destination-section samples after
16 warm-up submissions. Guest scene phase means sum to221.913ms, excluding
input/pacing and loading. This is approximate frame work, not a measured
presentation FPS or a hardware benchmark. XEMU scheduling affects these times.

- Camera/visibility/world rebuild:32.317ms.
- Actor construction/drawing phase:6.144ms.
- Broader presentation/state-export phase:165.433ms.
- Physics/event stepping:10.798ms.
- Other measured phases total7.221ms.

Nested renderer phases total43.479ms, including28.442ms for world/particles/HUD
and GPU waits. Do not add this renderer total to scene time. The broad165ms
phase includes scene_npc_draw, scene_clutter_draw, weapon/pickup/player-weapon
drawing, the platform sink and state export; it needs finer timing before
attributing the remainder to any one system.

A bounded solid-overlay batching candidate shares shader setup across HUD
commands and drains/resets after at most64 fans. It preserves draw order and
adds no vertex allocations. Native timing/state/capture validation is running
in artifacts/performance-hud-batch-xemu.log. No speedup is claimed yet.
Next: compare that run, then instrument the broad presentation phase to find
its dominant actor/geometry/platform cost. Local baseline: artifacts/performance-baseline.json.

HUD batching validation: replay-20260914-090327 passes the same180-frame
stock64MiB crossing and native/PC state checks. Its native framebuffer was
inspected: the two-line Hendrix subtitle, crosshair and weapon HUD remain visible.
No GitHub screenshot was added. Command batches retain at most64 fans before
GPU drain/reset and reuse solid shader state within the overlay pass.

Measured scene phase means total198.077ms versus221.913ms baseline (10.7%
lower); nested renderer means total26.750ms versus43.479ms. Draw/GPU-wait
phase falls from28.442 to12.558ms. This single controlled replay comparison
suggests about5FPS-equivalent frame work, not a broad game performance guarantee.

Finer instrumentation now separates NPC/clutter/world-weapon/pickup/first-person
weapon drawing, platform sink and state export within the dominant scene phase.
Both builds and37 tests pass. Native detailed timing is running in
artifacts/performance-detail-xemu.log. Remaining CPU costs still dominate;
next changes must follow those measurements.
