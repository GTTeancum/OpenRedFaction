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

Nested renderer phases total43.539ms, including28.442ms for world/particles/HUD
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
