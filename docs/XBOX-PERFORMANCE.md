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

Detailed run replay-20260914-090636 passes. Presentation subphase means:
NPC51.192ms, clutter84.702ms, world weapons12.260ms, pickups10.404ms,
first-person weapon16.404ms, platform sink37.942ms, state export0.029ms.
Absolute timings vary between runs; model-side work clearly warrants attention.

Source inspection found full384KiB scratch poisoning per model batch in four
paths. The candidate now poisons only the batch's addressable vertices in each
cache/clip/second/output array, preserving0xa5 initialization and capacity checks.
It changes neither allocation size nor geometry selection. The same PC crossing
has byte-identical final pixels and matching NPC/clutter/world-weapon/first-person
draw summaries;37 tests pass. Evidence: artifacts/scratch-active/report.json.
Native timing/state/capture validation is running in artifacts/performance-scratch-xemu.log;
no native speedup is claimed before it completes.

Scratch validation correction: the initial PC build failed, but the shell
continued into tests/replay using the previous executable. Those initial claims
are withdrawn. Native run replay-20260914-091131 stopped at compilation; no
emulator performance result exists for it. Fixed the world-weapon batch variable
reference and allowed NULL secondary scratch for static geometry. Build commands
now check exit status before testing.

Fresh corrected PC build/replay passes with SHA25612ad5c30891264b7a3309516769520f9d566ebd42e919727d44a22316508d55e,
distinct from baseline1b4f82f8d2b61c2be7cc081a6bfd2e29d8cf1d5ea380ec4fea7ca90b96bccb81.
Final pixels match byte-for-byte and final four model-draw summaries match;
NPC/clutter/world-weapon final counts are zero in this camera, so these alone
are not proof for all visible models. The37-test suite also passes. Repeatable
comparison: tools/replay_model_scratch.py. Corrected native run is now active
in artifacts/performance-scratch-fixed-xemu.log. Speedup remains unverified.

Visible-model differential coverage now passes via tools/replay_scratch_differential.py.
The same freshly built PC binary renders actor8323 with either active-range or
full-capacity diagnostic initialization. Final pixels and all four draw summaries
match: NPC7446 vertices, clutter1200, world weapons972, first-person weapon756.
The active capture was inspected and shows the guard/HUD. This closes the
zero-visible-actor weakness of the crossing's final-frame comparison, but is
still one camera/level. No GitHub image was uploaded. All37 tests pass.
The full-fill switch is process-local PC headless test input only; normal builds
retain active-range initialization. Native performance remains pending in
artifacts/performance-scratch-fixed-xemu.log.

Native scratch optimization verified: replay-20260914-091453 passes the same
180-frame stock64MiB crossing, native/PC state checks and framebuffer validation.
The native capture was inspected: subtitle, weapon and HUD remain correct.
No image was uploaded. Harness restoration built the latest source successfully.

Detailed baseline -> active-range scratch phase means (ms):
NPC51.192->7.913; clutter84.702->2.500; world weapons12.260->0.885;
pickups10.404->1.010; first-person weapon16.404->4.115;
platform sink37.942->36.712; state export0.029->0.029.
Scene totals294.365->125.864ms (57.2% lower in this comparison). The relatively
stable platform timing supports a model-side improvement; host variation still
limits general FPS claims. This is approximately8FPS-equivalent measured work.

Next priorities from the optimized run: camera/visibility/world rebuild41.894ms,
platform sink36.712ms, physics13.288ms. Break down world rebuild before changes.
Evidence: artifacts/performance-scratch.json, artifacts/scratch-differential/report.json.

World-phase profiling verified in replay-20260914-092055: stock64MiB native
180-frame crossing passes with 104 destination samples. World geometry
rebuild averages39.587ms; visibility traversal0.269ms, audio0.173ms,
camera setup/room location0.240ms. Total scene work123.634ms confirms
the prior scratch improvement; this instrumentation adds no new speedup.
Next optimization target: world geometry rebuild, then platform submission.
PC build and37 tests pass; Xbox build/harness restoration pass. Evidence:
artifacts/performance-world-detail.json and artifacts/xemu/replay-20260914-092055.

Camera-transform cache candidate: each geometry draw uses a64-entry,1024-byte
temporary cache keyed by vertex index; UV/lightmap UV remain face-corner data.
The cache resets for every geometry/camera/mover pose, preserving arithmetic
and avoiding persistent allocation. XBE stack reserve is65536 bytes.
All37 tests pass. tools/replay_world_cache.py compares a preserved executable
against the new build across five walking transitions plus visible actor8323:
all final pixels and selected world/model/player summaries match. Native
stock64MiB timing validation completed; measured results follow.

World camera cache verified in replay-20260914-092818: stock64MiB180-frame
crossing/state checks pass. World rebuild39.587->25.135ms
(36.5% lower); scene total123.634->88.875ms, about11FPS-equivalent work.
Platform time also falls36.471->27.260ms, indicating host/run variation;
do not attribute the whole scene gain to the cache or claim hardware FPS.
Both builds,37 tests, five PC transition comparisons and the visible-actor
comparison pass. Cache uses1024 transient bytes, with no persistent allocation
or geometry/texture reduction. Evidence: artifacts/world-cache/report.json,
artifacts/world-cache/performance.json and artifacts/world-cache/xemu.log.
Next: reduce remaining world projection and render submission costs.

Validation correction: replay-20260914-092055 and092818 omitted --capture;
their PASS proves state/timing checks, not framebuffer validation. The earlier
091453 capture is present. Vertex-upload validation now explicitly uses
--capture and will compare native output with that preserved framebuffer.

Vertex-upload experiment rejected after replay-20260914-093224. Preparing
colors in a local vertex before writing GPU storage passes stock64MiB replay
state and selected HUD pixels, but upload time5.144->6.212ms shows no measured
XEMU gain (scene88.875->88.336ms is effectively unchanged). Restored the
previous renderer rather than retaining an unproven optimization.
Full native640x480 framebuffer exactly matches replay-20260914-091453,
also closing final-image coverage for the intervening world camera cache.
Evidence: artifacts/vertex-upload/performance.json and native-image-comparison.json
in artifacts/xemu/replay-20260914-093224. Repeatable tool:
tools/compare_native_replay_images.py. This verifies one final frame only.
Next: world projection/clipping remains the measured CPU target (~25ms).

World clip-code cache candidate: reuse each transformed vertex's six-plane
outside mask across face fans, preserving the existing full clipping order
for crossing triangles. Cache grows1024->1280 transient bytes. PC build
and37 tests pass; five walking transitions plus visible actor8323 match
baseline final pixels and world/model/player summaries. Native --capture
timing/state replay completed; verified results follow.

Clip-code cache verified in replay-20260914-093743:180-frame stock64MiB
crossing/state/HUD checks pass; complete640x480 native final framebuffer
matches091453 exactly. Both builds and37 tests pass; six PC comparisons
match final pixels and world/model/player summaries. Transient cache1280B.
Measured world rebuild25.135->24.317ms; scene88.875->83.482ms. Platform
27.260->24.212ms also changes, so the overall gain is not attributable solely
to this cache. Treat as a modest reduction, roughly11-12FPS-equivalent
frame work, not hardware FPS. Next: larger world projection/submission costs.
Evidence: artifacts/world-clip-codes/pc-report.json, performance.json,
and artifacts/xemu/replay-20260914-093743/native-image-comparison.json.
