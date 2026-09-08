# Initial validation — 2026-09-08

- Original executable and tables match RF 1.20 NA; detailed hashes are in PROVENANCE.md.
- Ghidra 11.3.2 headless analysis completed, yielding 7,248 candidate functions.
- PC: Visual Studio 2022, MSVC 19.44.35225, Windows SDK 10.0.26100.0, Win32 build.
- Xbox: installed NXDK commit recorded in PROVENANCE.md, MSYS2 Clang 21.1.8,
  Pentium III target, existing MinGW64 cxbe/extract-xiso host tools.
- PC CTest passes malformed archive, boundary, lookup and interleaved-read tests.
- C VPP reader matches every offset/name/size in 15 archives (8,410 entries).
- The level inspection tool scans all 94 installed RFLs. `L1S1.rfl` is Live Mines,
  version 180, with 29 payload sections plus an end marker; player-start and
  level-info offsets agree with the scanned sections and there are no trailing bytes.
- Compiled checksum matches original x86 in Unicorn 2.1.4 for 9,671 cases.
- XEMU 0.8.136, commit `fc24584ce88f0915ad7f04775bb7712c2e3f49ee`, reports
  exactly 67,108,864 bytes guest RAM; the guest independently reports 16,384 pages.
- Xbox diagnostic successfully reads disc `tables.vpp`: 29 entries, 1,294,336
  bytes, filename checksum `0x32d7cb85`.
- Success with both `xbox-4627_debug.bin` and `Complex_4627v1.03.bin`, using
  XEMU's `xemu` display backend, isolated settings, and temporary HDD writes.
- `-display none` attempts timed out before entry. Debug BIOS evidence includes
  a guest kernel page fault at `0xfff0007b`; the precise emulator cause is unresolved.
  Keep this as a harness limitation rather than a game defect.

Successful initial reports:
`artifacts/xemu/20260908-121223-572203/report.json` (Complex) and
`artifacts/xemu/20260908-121243-616063/report.json` (debug).
The XBE SHA-256 for these runs is
`da87262ff74d2e45dc311892992eaf157283536c33ffb6d050e9a8f01b8e29d9`.

This is diagnostic validation only: no campaign gameplay, 3D renderer, PS2 visual
comparison, performance measurement, or physical hardware validation is complete.

## Shared level loader and project relocation

The source now resides at `D:/Programming/GitHub/OpenRedFaction`; fresh Win32 and
NXDK builds succeeded there. Old C-drive build artifacts remain under
`local/legacy-build-c` solely as historical evidence.

- `level_bounds` and `vpp_bounds` CTest suites pass.
- `tools/verify_levels.py` verifies all 94 installed level directories and
  bit-exact spawn positions/orientation row order against independently read bytes.
- Xbox report `artifacts/xemu/20260908-122235-064382/report.json` passes with
  exactly 64 MiB RAM, RFL version 180, 29 sections, 3,572,594 level bytes,
  geometry section 1,637,649 bytes and lightmap section 1,130,684 bytes.
- Guest Live Mines spawn bits match disk: `c2ee600a 3efabdf8 426dc25d`.
- The original levels1.vpp is larger than the total Xbox RAM; direct directory
  and spawn reads succeed without an archive-sized allocation. This does not
  establish that expanded geometry, textures, AI and other game state fit.

## Resident static geometry

- `inspect_geometry.py` independently parses known fields in all 94 payloads,
  recording opaque trailing bytes rather than silently discarding them.
- `verify_geometry.py` compares C counts to those results for all 94 levels and
  verifies rejection when the allocation budget is one byte below the required
  payload/index requests. Largest requested allocation: 2,131,016 bytes.
- Nine altered original-level fixtures are rejected: oversized texture/face/corner
  counts, invalid texture/room/vertex indices, fewer than three corners, and
  nonfinite plane/position values. Existing CTest suites also pass.
- XEMU report `artifacts/xemu/20260908-122924-936004/report.json` verifies resident
  Live Mines geometry at 64 MiB: 17 textures, 54 rooms, 6,658 vertices, 7,418 faces,
  29,110 corners, 5,851 lightmap-mapping records, 1,667,605 requested bytes.
- First and last decoded vertex positions match original binary32 disk bits.
  Guest available-page telemetry is captured after allocation; it is not a
  whole-game memory peak measurement.

## Initial geometry rendering

The shared preview mesh clips and triangulates Live Mines at the stored spawn,
using a provisional 90-degree horizontal field of view and face-normal colors.
It contains 12,169 triangles (876,168 vertex bytes). The PC diagnostic rasterizer
and Xbox NV2A renderer both draw this mesh at 640x480.

`artifacts/xemu/20260908-124226-256752/report.json` verifies three frames in
64 MiB XEMU. Its framebuffer.png is decoded from the game's actual framebuffer
via QMP guest physical RAM; no desktop capture or input is used.
An initial W-buffer state override caused distant geometry to show through walls.
Restoring Z-buffer mode after each pb_target_back_buffer call fixes this capture.
`tools/compare_preview.py` compares it to artifacts/live-mines-pc.ppm: only 191 of
307,200 pixels differ by more than three channel values (0.0622%). Small edge
and color-rounding differences remain. This is a diagnostic view comparison,
not evidence of original renderer equivalence or PS2 visual parity.

Texture/lightmap rendering, visibility, collision, Geo-Mod mutation, gameplay,
and original loader behavioral equivalence remain open.

## TGA decoding

`python tools/verify_images.py` passes on 41 installed textures: all 16 Live Mines
TGAs, every installed true-color RLE image, and representatives of each other
supported header variant. All decoded RGBA pixels match Pillow with unspecified
alpha normalized to opaque. Each also rejects a budget one byte below its output.
Seventeen synthetic cases cover all four origin combinations in raw/RLE data,
mixed run/raw packets, truncated pixels, and a packet exceeding the image size.
One installed paletted TGA is recorded as unsupported in
`artifacts/image-tests/report.json`; no claim of complete image-format support.
The 16 Live Mines base images request 2,117,632 RGBA bytes; this excludes GPU
layout, mipmaps, lightmaps and allocator overhead. USERBMAP has no matching TGA.
PC and NXDK builds pass. The initial decoder-only check was followed by the
resident-material Xbox test below.

## Resident Live Mines materials

`rf_material_probe` loads the geometry's texture names through the shared C API
and searches explicitly ordered maps1/maps2/maps3/maps4/maps_en archives. It loads
16 images, retains one missing USERBMAP slot, and requests 2,118,040 bytes including
17 material records. An exact 2,118,040-byte budget succeeds; 2,118,039 rejects
and the probe verifies that failure leaves no material allocation/state behind.

`artifacts/xemu/20260908-125115-298432/report.json` verifies the same counts and
allocation in stock 64 MiB XEMU, with all original map archives on the test disc.
Schema-5 guest telemetry reports decoded pixel checksum 0xa3aeb67d, matching
independent Pillow decoding of all 16 source images (XOR of per-image FNV-1a).
This checksum is diagnostic, not cryptographic proof. Geometry and three GPU
frames still complete with the images resident. Material sampling is not yet
bound to GPU draw calls, so the framebuffer remains the untextured preview.

## Perspective UV preparation

The shared preview now interpolates source UVs at clipping intersections and
stores u/z, v/z, 1/z plus the material index. The same 36,507 vertices now occupy
1,460,280 bytes. PC optional archive arguments enable nearest/repeat base-texture
sampling, producing artifacts/live-mines-textured-pc.png. This inspected image
has no lightmaps, transparency or filtering and is not an original-game comparison.

NXDK's generated dependencies used Windows D:/ targets while make used /d/,
leaving renderer.obj stale after the vertex header changed. Explicit project
header/shader prerequisites fix this. The rebuilt run at
artifacts/xemu/20260908-125415-589445/report.json passes material checks and the
untextured frame comparison with the expanded stride. The earlier run at
20260908-125344-963757 passed telemetry only with a stale renderer object and
must not be used as evidence of vertex-layout correctness.

## Xbox base texture sampling

`artifacts/xemu/20260908-125913-947166/report.json` reaches three textured frames
in 64 MiB XEMU. GPU uploads use ARGB8 swizzled power-of-two images, border-color
source, repeat and nearest sampling. Material changes split draw batches.
Projective TEX0 uses shared clipped u/z, v/z and 1/z. Missing images use diagnostic
face color through a white texture, not recovered USERBMAP behavior.

The inspected native framebuffer shows the expected rock tunnel. The strict PC
comparison FAILS: 29,235/307,200 pixels exceed three channel values, maximum 107,
mean maximum-channel error 1.346123. Sampling/coverage differences remain open;
execution and visible texture sampling do not establish pixel equivalence.
Initial corrupt captures selected a border layout absent from the uploaded data.
GPU images request 2,117,636 bytes including the white fallback, in addition to
retained CPU images. Full gameplay memory peaks and frame rate remain unverified.

## Raster precision investigation and lightmap inventory

The optional PC diagnostic environment flag RF_PREVIEW_SNAP_1_16 floors screen
coordinates to a 1/16 grid before software rasterization. Against the unchanged
XEMU capture above it yields 1,222 pixels over three channel values, maximum 70,
mean maximum-channel error 0.060765. This still FAILS the strict comparison.
The experiment suggests subpixel precision accounts for much of the mismatch,
but does not prove NV2A hardware behavior; the default reference is unchanged.
Reports: artifacts/sample-offset-test.json and artifacts/sample-quantized-test.json.

`python tools/inspect_lightmaps.py` checks every byte boundary for all 94 level
lightmap sections and writes artifacts/lightmaps.json. Live Mines contains 23
images; its 1,130,684-byte section expands to 1,507,328 RGBA pixel bytes. Largest
installed-level RGBA expansion is 3,670,016 bytes. This validates image framing
only, not geometry-to-lightmap mapping or the original lighting equation.

## Shared C lightmap loading

`python tools/verify_lightmaps.py` verifies all 94 installed levels through the C
loader, comparing counts/dimensions to independent section inspection. Exact
budgets include RGBA pixels and Win32 image records; every nonzero one-byte-short
budget fails and the probe checks cleared state. Live Mines requests 1,507,696
bytes including 23 records. All 23 decoded pixel FNV hashes match independent
RGB-to-RGBA expansion. Report: artifacts/lightmap-validation.json.
The loader preserves stored row order and uses 1536 bytes of input scratch,
rejects impossible counts/dimensions/payload boundaries, and requires exact section
consumption. PC and NXDK compile; Xbox runtime integration and corrupt-fixture
coverage are still pending. This does not validate lightmap sampling or blending.

## Lightmap mapping and Xbox residency

The lightmap probe now loads geometry and resolves every mapping record through
the shared accessor, checking the first word against the loaded lightmap count.
All 94 levels pass. The accessor also rejects an out-of-range mapping and a zero
image count. Remaining 92 bytes per mapping are retained without interpretation.

`artifacts/xemu/20260908-130723-044370/report.json` (schema 6) confirms 23 loaded
Live Mines lightmaps and successful mapping bounds checks in 64 MiB XEMU. Geometry,
base materials and lightmaps remain resident through three rendered frames. The
native framebuffer bytes equal the earlier base-textured capture exactly. Lightmap
sampling is not enabled; GPU lighting and the known PC sampling mismatch remain
open. Available-page telemetry follows loading, not the complete GPU memory peak.

Lightmap UV propagation expands preview vertices to 56 bytes (2,044,392 requested
bytes for Live Mines). Both toolchains build. XEMU run
artifacts/xemu/20260908-131410-565786/report.json passes residency checks; native
framebuffer bytes equal the earlier base-textured capture exactly. The second
texture stage remains disabled, so this checks layout compatibility rather than
lightmap interpolation or the final lighting equation.

## First lightmapped tunnel

`artifacts/xemu/20260908-131705-063932/report.json` captures three frames with both
texture stages active in 64 MiB XEMU. Stage 0 uses linear/repeat base sampling;
stage 1 uses linear/clamp lightmaps. Doubled modulation follows the original
supported-capability path. Missing lightmaps preserve base color through a white
fallback and half input color. GPU uploads are capped at 8 MiB; current base,
lightmap and fallback pixels request 3,624,964 bytes, additional to CPU copies.

The native Xbox frame and artifacts/live-mines-lit-pc.png were inspected and show
matching large-scale baked lighting. Strict comparison FAILS: 5,248 pixels exceed
three channel values, maximum 75, mean maximum-channel error 0.608568. Report:
artifacts/lit-preview-comparison.json. Fog, alpha, dynamic lighting and original
texture conversion remain unimplemented; original-game/PS2 parity is not proven.
Final guard/fallback edits compile on both targets and existing CTest checks pass;
these checks are not additional visual evidence.

## Shared raster grid and upload memory

Both targets now receive screen positions floored to a 1/16-pixel grid by the
shared projection code. The PC-only RF_PREVIEW_SNAP_1_16 experiment is removed.
This is an explicit cross-platform precision policy, not a claim that all NV2A
or original-game rasterization semantics are reconstructed.

`artifacts/xemu/20260908-132044-184009/report.json` verifies schema-7 execution
and allocation snapshots. Comparison to the rebuilt default PC reference PASSES:
maximum channel error 1, zero pixels above three values, mean maximum-channel
error 0.030514. Report: artifacts/lit-shared-grid-comparison.json.
After uploads, 46,399,488 physical bytes (44.25 MiB) remain available; after CPU
mesh release, 48,451,584 bytes remain. GPU requests are 3,624,964 image bytes and
2,044,392 vertex bytes. This is an observed resident-scene snapshot, not a measured
whole-game high-water mark or performance benchmark.
`xemu_smoke.py --reference <PC image>` now requires the image comparison to pass
before reporting overall success; without it the harness checks execution only.
The integrated comparison run at artifacts/xemu/20260908-132159-367249/report.json
passes with the rebuilt lit PC reference and unchanged comparison thresholds.

## Initial Xbox animation runtime check (superseded profile)

The initial schema-8 diagnostic executed the shared `src/diagnostic/animation.c`
check before loading the scene. It reads miner bones/eye attachment and standing
and crouching motion tracks directly from `meshes.vpp`/`motions.vpp`, blends
two looping slots over 64 updates at 1/30 second, and tests a second cached
evaluation each frame. Root displacement is applied once; a newly queued value
must survive a same-generation cache hit. This runs through the NXDK-built
shared playback, archive sampling, pose blending and hierarchy code on Xbox.

`rf_animation_check` on PC and `tools/verify_animation_check.py` independently
produce the same sequence checksums as original `0x51ba80`, `0x51b500` and
`0x51b2e0`, with no original callee replacements:

- Bones: 25; frames: 64; bone payload: 1,404 temporary bytes.
- Evaluated bone matrices: `0x7b7ca73f`.
- Playback state: `0x0d9922b4`.
- Cached matrices, queued displacement and stamps: `0xa2a46bf3`.
- Evaluated eye transforms: `0x63bda091`.

The runtime hashes use FNV-1a over serialized bytes as a regression check,
not a security guarantee. Original-code evidence is recorded locally in
`artifacts/animation-check-original.json`. The smoke harness requires all eight
animation telemetry words to match its freshly executed PC check.

`artifacts/xemu/20260908-160714-948313/report.json` passes on XEMU 0.8.136 with
exactly 64 MiB guest memory and the 4627 debug BIOS. Animation hashes match,
and the existing scene still compares to the lit PC reference with maximum
channel difference one and zero pixels above the three-value threshold.
Native guest framebuffer capture is in the same run directory. Available
memory after scene mesh release is 48,418,816 bytes; this is a resident-scene
snapshot after diagnostic cleanup, not combined gameplay peak usage.

Animation is evaluated numerically before drawing the unchanged frozen scene.
No animated character rendering, frame-rate guarantee, complete gameplay camera
or PS2 parity is established by this check. Runtime coverage currently uses
two looping motions without a primary slot or bone overrides.
# Post-selector animation controller

Run `python tools/verify_motion_controller.py` after building the PC probes.
It compares 2,406 original post-selector executions and their unmodified loaded
control callees against shared C, including all controller/playback fields and
registered resource references. Three additional rejection cases check atomic
failure. Evidence: `artifacts/motion-controller-verification.json`.
This excludes locomotion selection and entity gating. PC CTest passes all four
tests and NXDK compilation passes. The subsequent integrated runtime check below
adds controller XEMU coverage for its specific scripted sequence.
# Logical animation state requests

`python tools/verify_motion_request.py` matches 10,000 complete original
0x42a580/0x42a650 executions and checks four safe-rejection cases. Local report:
`artifacts/motion-request-verification.json`. Coverage is finite forward
transitions and controller membership; it excludes locomotion selection.

## Controller-to-eye Xbox runtime check

The shared diagnostic now starts with no slots and scripted logical state zero.
It requests crouch (state 8) at frames 4 and 20, standing (state 0) at frames 7
and 40, using duration .25 seconds. Frames 22 through 25 force standing through
the controller override while transition time continues. All 64 frames apply
the post-selector controller, update playback at 1/30 second, evaluate 25 bones
and the eye, then verify the same-generation cache preserves queued displacement.
The playback hash also includes the 24-byte controller and both resource refs.

PC and original instruction execution match:

- Bone matrices: `0x93422512`.
- Playback, controller and references: `0xf24148d9`.
- Cached matrices, displacement and stamps: `0xd45d8bb2`.
- Eye transforms: `0x4d5ecb4f`.

`tools/verify_animation_check.py` executes original 0x42a580, the post-selector
0x41f2b6 block and unmodified loaded-control, playback, skeleton and tag callees.
`artifacts/xemu/20260908-163832-540225/report.json` passes with the same four
hashes and all eight animation telemetry words on stock 64 MiB XEMU 0.8.136.
The run used `python tools/xemu_smoke.py --no-capture`; no framebuffer was
acquired. Renderer descriptor and resident-scene checks also pass, without a
new visual comparison. The telemetry layout remains schema 8.

This replaces the earlier fixed .5/.5 blend runtime profile. It does not recover
the locomotion selector, physics/AI side effects, player initialization flags,
bone overrides, actual gameplay camera or animated character rendering.
# Movement selector tail

`python tools/verify_motion_movement.py` matches 6,000 executions of original
0x41f7c1..0x41f94f with unmodified predicate, vector, membership and request
callees. It compares all controller fields and adds two C-only rejection cases.
Report: `artifacts/motion-movement-verification.json`. Earlier priority,
candidate selection and physics/AI behavior are outside this verification.
# Animation priority prefix

`python tools/verify_motion_priority.py` matches 5,010 executions of the original
0x41f400..0x41f5ad prefix, including its unmodified callees. Ten targeted velocity
cases cover the x87 threshold with small orthogonal components; the ordinary
double rewrite failed one of them and was replaced with original x87 precision.
An observation-only hook stops at fallthrough 0x41f5ae. Report:
`artifacts/motion-priority-verification.json`. Middle selection, physics/AI and
actual entity initialization remain outside the check.
# Remaining time and action activity

`python tools/verify_motion_action.py` matches 7,000 original 0x428d10 and
type-two 0x5033d0/0x501bd0/0x51c270 executions with no callee replacements.
The test checks returned predicate/float bytes and unchanged entity/model RAM.
Coverage includes zero weights, loop/freeze flags, cursor boundaries, absent
slots, invalid action indices and large positive end ticks. Local report:
`artifacts/motion-action-verification.json`. Candidate-helper side effects and
other character types are not covered.
# Entity action starts

`python tools/verify_motion_action_start.py` matches 6,000 original action-start
executions through type-two loaded controls, checking complete playback state,
32 resource references and sound-class requests. It observes and stops at
0x434da0 sound resolver entry; no original callee is replaced. Actual sound
selection/playback is excluded. Report:
`artifacts/motion-action-start-verification.json`.
# Game clocks and deadlines

`python tools/verify_timer.py` matches 9,800 complete original operations and
checks six malformed-input rejections without mutation. Original functions:
0x4fa2d0, 0x4fa320, 0x4fa330, 0x4fa360, 0x4fa3e0, 0x4fa3f0, 0x4fa420.
Both clocks, pause nesting, deadlines and query results are compared. Report:
`artifacts/timer-verification.json`; detailed scope is in docs/TIMERS.md.
# Movement settings

`python tools/verify_movement.py` matches 6,000 complete original 0x427450
executions with unmodified 0x40a210. It compares response, speed and mode bytes
and unchanged remaining entity RAM; three C-only rejection cases preserve
state. Report: `artifacts/movement-settings-verification.json`. See
docs/MOVEMENT.md for mappings and limitations; collision/input integration
is outside this check.
# Integrated selected-turn effects

`python tools/verify_turn_effects.py` matches 2,400 original selected-turn block
executions with unmodified action, absent-sound, timer and movement callees;
two C-only rejection cases preserve outputs. It compares complete playback/
reference state plus candidates, turn flag, deadlines and movement settings.
Report: `artifacts/turn-effects-verification.json`. Preceding decisions/reset,
valid-sound playback and runtime integration are excluded; see docs/TURN.md.
# Turn direction gate

`python tools/verify_turn_direction.py` matches 5,208 original direction-gate
executions, including unmodified math callees, eight dot-boundary cases and
200 length-threshold neighbors. Report: `artifacts/turn-direction-verification.json`.
Observation hooks stop before later decisions/reset effects. The x87 dot
comparison fixes an observed double-precision mismatch; see docs/TURN.md.
# Remaining turn-candidate branches

`python tools/verify_turn_finish.py` matches 4,800 original remaining-branch
executions and unmodified movement/action/absent-sound callees. Complete
playback/reference state, effects and candidates are compared. Report:
`artifacts/turn-finish-verification.json`. This excludes preceding reset/decision
logic and valid-sound playback; detailed conditions are in docs/TURN.md.
# Assembled candidate helper

`python tools/verify_turn_update.py` matches 3,004 complete original 0x41f9f0
executions with unchanged callees for absent weapon-entry/sound fixtures.
Six outcome branches are observed, including 26 selected-turn and 107 secondary
turn cases; four explicit distance cases exercise the 8.2 threshold. A C-only
test rejects a missing required reset callback. Report:
`artifacts/turn-update-verification.json`. Populated reset/audio adapters and
Xbox runtime integration remain unverified; see docs/TURN.md.

# Sidestep runtime integration (supersedes the controller-only profile)

`python tools/inspect_animation_names.py` verifies 23 state names and 45 action
names against their original initializer instructions, identifying actions
17/18 as sidesteps and 19/20 as rolls. `python tools/verify_animation_check.py`
matches the new shared 64-frame diagnostic against unchanged original callees.
Starts are 17,17,18; active actions suppress restarting; reset is never reached.
Pose, state/effects, cache and eye hashes are respectively `0x26ec2ef5`,
`0x4a139142`, `0x18528f6d`, `0x53433e81`. The state hash now includes four
resource reference counts, movement/candidate/timer fields and sound output.

PC Release and NXDK builds passed; all four CTest checks passed. XEMU run
`artifacts/xemu/20260908-173252-156413/report.json` passed with stock 64 MiB
and identical hashes, using `python tools/xemu_smoke.py --no-capture`.
No framebuffer files were produced. XBE SHA-256:
`37186ae25e4bc8d992f8a20d2812afda028592f4a2ed655e400588d5111e35b5`;
ISO SHA-256:
`f3b17e020d68878caa5fbb21a779d1f106248cfaab633cf73c0e30131eb56044`.
Observed available RAM after static scene CPU mesh release is 48,410,624 bytes;
this remains a frozen-scene measurement, not full-game peak memory.

This adds candidate-helper runtime coverage for absent roll mappings and sound
classes. Populated reset/audio adapters, roll runtime coverage, actual entity
initialization, the full locomotion selector and animated character rendering
remain unverified. The diagnostic combines guard sidestep files with its existing
miner rig, not a reconstructed gameplay entity; see docs/TURN.md.

# Locomotion candidates in the shared runtime

`python tools/verify_turn_update.py --candidates` passes 3,014 original block
executions, observing 12 candidate combinations and ten x87 speed-boundary
fixtures. The 3,004-case complete helper verification still passes after sharing
the magnitude comparison code. `python tools/verify_animation_check.py` matches
the expanded 64-frame profile, including candidate outputs and missing-state
fallbacks. The state hash is now `0xc85a78c2`; pose, cache and eye hashes remain
`0x26ec2ef5`, `0x18528f6d`, `0x53433e81`. This supersedes the preceding profile.

PC Release/NXDK builds and all four CTest checks pass. Numeric-only XEMU run
`artifacts/xemu/20260908-174051-074482/report.json` passes in stock 64 MiB,
with no framebuffer capture. XBE SHA-256:
`c13b950d33a1579c2b755ef2e2de13eda88daa8d90ee9f86b25c9c7bc22b9db9`;
ISO SHA-256:
`a50c1c023cef89cc7bc645c9b1882ff8f089b4b35bcb3bbfab086c2ed9a55f44`.
Available RAM after frozen-scene CPU mesh release is 48,406,528 bytes; full-game
peak and animated character rendering remain unverified. See docs/TURN.md for
the missing combat predicates, preceding reset and later physics/state selection.

# Entity-derived combat eligibility

`python tools/verify_entity_predicates.py` passes 2,007 original-code fixtures
and a separate C-only cycle rejection. `python tools/verify_animation_check.py`
matches all 64 frames with entity-derived readiness/eligibility now included
in the state hash: `0x9a4d7dc2`. Pose/cache/eye hashes remain `0x26ec2ef5`,
`0x18528f6d`, `0x53433e81`. This supersedes the forced-combat profile above.

PC Release/NXDK builds and all four CTest checks passed. Numeric-only stock
64 MiB XEMU run `artifacts/xemu/20260908-174822-587362/report.json` passed
with matching hashes and no framebuffer capture. XBE SHA-256:
`9830e66dc5ad9b9396836b5a78d3ff8dd8f822fd6b3d916ba394ec6a9d88f370`;
ISO SHA-256:
`f37f777977420e8bc70c480b09e093d87bef206db1097f6ab0af6ac0ec1d76af`.
Available RAM after frozen-scene CPU mesh release remains 48,406,528 bytes.
The 2,007-case registry/seat/attachment coverage runs on PC against original
instructions; the Xbox profile uses a single equipped entity view. Full-game
peak memory, actual entity creation and animated rendering remain unverified.

# Preparation timer and invalid-weapon resets

`python tools/verify_locomotion_prepare.py` passes 5,000 original block fixtures
(695 reset invocations), plus two C-only adapter-error cases. The shared
64-frame oracle now observes 16 original invalid-weapon resets and compares the
800ms deadline changes. Its state hash is `0xb8a774ba`; pose/cache/eye hashes
remain `0x26ec2ef5`, `0x18528f6d`, `0x53433e81`, superseding the prior profile.

PC Release/NXDK builds and all four CTest checks pass. Numeric-only stock 64 MiB
XEMU run `artifacts/xemu/20260908-175407-747170/report.json` passes with identical
hashes and no framebuffer capture. XBE SHA-256:
`8077d928f816f8eeb8c5951c605bfe236f7f55a11b3fe07e6a45e4ac4fb79c76`;
ISO SHA-256:
`676b9442a301396b4e27edc155a6fecc636bfc2ddf99d4e56b11727a821b96b9`.
This validates the gate and ordering, not reset effects for valid weapons.
See docs/TURN.md for the diagnostic adapter's explicit invalid-index scope.

# Valid-weapon reset and animation interruption

`python tools/verify_weapon_reset.py` passes 2,400 original-code cases: 1,448
complete and 952 match state at required external-operation boundaries. Missing
adapters return RF_NOT_FOUND with matching prior effects; successful external
operations remain unverified. `python tools/verify_animation_check.py` passes
the expanded 64-frame profile with valid weapon 0 active at frame 48, nonloop
stop, 16 reset calls and starts 17,17,18,18. Pose/state/cache/eye hashes are now
`0xdc7c08a6`, `0x4785c534`, `0x21cd6b06`, `0x60a29326`, superseding prior profiles.

PC Release/NXDK builds and all four CTest checks pass. Numeric-only stock 64 MiB
XEMU run `artifacts/xemu/20260908-180054-139788/report.json` passes with matching
hashes and no framebuffer capture. XBE SHA-256:
`a72be69d6bf2671e4e3097cfcbaa7e7333a0017d3a13845b4acbc74945e82453`;
ISO SHA-256:
`67150d9da154f464ba5698f40137577c38964d965b3caa4f81bb9415c04ebc33`.
See docs/WEAPON.md for callback boundaries and the remaining gameplay work.

# Effect switching and corrected player lookup

The earlier local-release boundary was erroneous: 0x48aa90 is a read-only
lookup. Updated weapon-reset verification passes 2,400 cases with 1,598 complete,
160 sound-stop, 97 release-sound, 83 effect-stop and 462 player-reset boundaries;
132 original local-player lookups execute to completion. Effect-switch verification
passes 4,000 complete original cases and two C-only bounds rejections.

The shared profile now calls effect switching from valid weapon reset and hashes
the two objects' enabled bytes/timestamps. State hash is `0x8989b3f8`; pose/cache/
eye hashes remain `0xdc7c08a6`, `0x21cd6b06`, `0x60a29326`. PC Release/NXDK builds
and four CTest checks pass. Stock 64 MiB numeric-only XEMU run
`artifacts/xemu/20260908-180743-138413/report.json` passes without framebuffer
capture. XBE SHA-256:
`0dfdf9136f18d9d69e110e3c2621fa0ad13480f781a9ec5b2d3acf93ac117368`;
ISO SHA-256:
`188bc821377a8fc3b8fca6143bea7b1e3a0e6229a249a95e171cb8f3a355071d`.
Sound/player-reset implementations and effect ownership/rendering remain open.

# Ammunition and replacement choice

`python tools/verify_weapon_inventory.py` passes 4,000 complete original reserve/
replacement-choice cases and two C-only bounds checks. The 64-frame original
comparison and shared PC/Xbox diagnostic now hash ammo reserve and replacement
selection across exhaustion at frame 32. State hash is `0x278ff164`; pose/cache/
eye remain `0xdc7c08a6`, `0x21cd6b06`, `0x60a29326`. This supersedes prior hashes.

PC Release/NXDK builds and all four CTest checks pass. Numeric-only stock 64 MiB
XEMU run `artifacts/xemu/20260908-181325-357221/report.json` passes with matching
hashes and no framebuffer capture. XBE SHA-256:
`00283fa52cfe1f08c601090ab1289b7716c70874dd48d6758093cb806533bf8b`;
ISO SHA-256:
`346c4ee1c00051cc78875f0241342f2f05c9517e201d19681b4e9ea96ec2b2fc`.
Observed available RAM after static-scene CPU mesh release is 48,402,432 bytes,
not full-game peak. The diagnostic observes replacement decisions without
applying the switch; full empty-weapon handling/presentation remains open.

# Empty-weapon decision flow

`python tools/verify_weapon_empty.py` matches 4,000 original decision-block
executions: 3,272 NONE, 64 PAIR, 202 MESSAGE and 462 SELECT. All reached predicate,
ammo and replacement callees run unchanged; execution stops at final outgoing
operations to check their arguments. Presentation updates before the block and
actual operation execution are excluded.

The shared 64-frame profile now also hashes the empty-weapon action. State hash
is `0x21293620`; pose/cache/eye remain `0xdc7c08a6`, `0x21cd6b06`, `0x60a29326`.
PC Release/NXDK builds and all four CTest checks pass. Numeric-only stock 64 MiB
XEMU run `artifacts/xemu/20260908-181948-915592/report.json` passes with matching
hashes and no framebuffer capture. XBE SHA-256:
`3f39e36caf4ff8cc74acbf1f4632738007616eae6c60369d32f2e98d98967bcf`;
ISO SHA-256:
`8da9ba33400334322c3e5626bc3c9d8788c4b4dd1b31f3a957451db9129e4849`.
The diagnostic still observes commands without applying weapon switches or
displaying ammo messages; see docs/WEAPON.md for remaining integration work.

Selection queue checkpoint: `python tools/verify_weapon_queue.py` passes 640
complete original 0x4acd50 executions with the unchanged 0x4fa3e0 tail callee,
including full player-view checks for unrelated writes. Win32 Release build,
all four CTest cases and NXDK build pass. Queue mutation is not connected to
the runtime diagnostic yet; no new emulator execution or screenshot is claimed
for this checkpoint. Firing, selection eligibility and activation remain open.

Selection tail checkpoint: `python tools/verify_weapon_selection.py` passes
4,000 original 0x4a4c91 cases (2,373 complete, 453 message, 519 apply and 655
followup boundaries). Original predicates and queue/timer callees execute
unchanged; external adapters are observed before entry, not replaced. Includes
819 queue mutations and 501 followups without queue mutation. The 640-case
queue verifier, four CTest cases, Win32 Release and NXDK builds also pass.
Earlier selection gates and successful external callbacks are not covered.
No new emulator runtime check or screenshot is claimed for this checkpoint.

Selection followup checkpoint supersedes the previous three-boundary profile:
4,000 expanded cases now execute original 0x4ad8a0 unchanged. Results are
3,071 complete, 408 message and 521 apply boundaries, with 797 queue mutations
and 872 followup clears (687 without a queue mutation). All eight followup
bytes match, including preservation of +f96/+f97. The 640 queue cases also
verify preservation of the added state fields. Four CTest cases, Win32 Release
and NXDK builds pass. Runtime diagnostic integration and successful transition
adapters remain open; no new emulator run or screenshot is claimed here.

Selection runtime checkpoint: shared 64-frame PC/original/XEMU profile now
hashes the 16-byte selection state after empty-weapon decisions. Original
execution confirms one queue call and one followup clear at frame 32, then
duplicate-request suppression. Expected words are [2,25,64,3699116198,
3589827904,567110406,1621267238,1404], state hash d5f86d40. Original comparison,
four CTest cases, Win32 Release, NXDK and stock 64 MiB XEMU pass. Numeric-only
report: artifacts/xemu/20260908-183800-793148/report.json; capture_requested=false.
XBE SHA256 f2c57855beb6b4fa37d2563b5e761fedba718af62f008969d54975d16fd0a665;
ISO SHA256 7fc6e0e98156492690aff51f1cfbf4527a4ba91580ba503854ed2320a5ec4630.
Earlier selection gates, local transition, presentation and actual weapon
activation remain excluded. Unchanged visual scene was not captured.

Current-weapon checkpoint: 3,000 original-code cases pass (2,590 complete,
including 428 nonlocal presentation early returns; 410 local presentation
boundaries). The 64-frame original profile now executes the whole empty
handler from 0x4a6f10 to its outgoing action and agrees with the reconstructed
nonlocal lookup plus decision flow. State hash remains d5f86d40. Win32 Release,
four CTest cases, NXDK and stock 64 MiB XEMU pass. Numeric-only report:
artifacts/xemu/20260908-184258-453142/report.json; no framebuffer capture.
Local presentation, transition and earlier selection gates remain open.

Presentation checkpoint: 4,000 original 0x4ae0d0 executions match (3,031
complete, 117 cleanup, 153 resource, 57 mode and 642 missing-model boundaries).
Original string-length/timer/mode callees remain unchanged; boundary callbacks
are observed before entry. Compact state, return value and unrelated player
bytes are compared. Win32 Release, four CTest cases, 3,000 current-weapon cases
and NXDK build pass. This validates existing-model selection and mutation,
not mesh loading, rendering, successful external operations or Xbox runtime
integration of local weapon presentation. No screenshot or new XEMU run.

Presentation gate checkpoint supersedes the prior cleanup-boundary profile:
4,000 original cases pass, with 3,236 complete, 54 resource (0x550820), 25
binding (0x48ab90) and 685 missing-model assertion boundaries. Unchanged
original callees include 113 no-op cleanup calls, 157 resource gates and 59
mode gates. Resource and binding arguments are also compared. Win32 Release,
four CTest cases and NXDK build pass. Successful deeper resource/binding
operations and local-presentation Xbox runtime integration remain open.
No new emulator run or screenshot is claimed for this change.

Model material-count checkpoint: 2,000 full original-code cases pass for
0x503690 and 0x4a76f0, covering all wrapper branches, negative counts and
wrapping sums; one C-only bounds rejection preserves output. Original object
views remain unchanged. Win32 Release, four CTest cases and NXDK build pass.
Material allocation/copy, binding and Xbox runtime integration remain open.
No new emulator execution or screenshot is claimed.

Material constructor checkpoint: 1,000 complete original 0x54a7c0 executions
match all 200 bytes from varied initial contents, with unchanged SEH/vector
constructor/color helpers. Surrounding canaries, return pointer and SEH-head
restoration also pass. Win32 Release, four CTest cases and NXDK build pass.
Material allocation/copy, binding and Xbox runtime integration remain open;
no new emulator execution or screenshot is claimed for this checkpoint.

Material copy preparation: 2,000 original cases pass, comprising 567 complete
no-allocation paths and 1,433 first-allocation boundaries. All 200 destination
bytes, source immutability, canaries and first allocation size are checked.
One malformed-name case preserves destination and plan. Win32 Release, four
CTest cases and NXDK build pass. Successful owned-array allocation/copy and
runtime binding remain open. No emulator run or screenshot for this checkpoint.
