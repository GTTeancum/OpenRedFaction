# Red Faction Xbox / PC — open work

Capture and post screenshots only when there is a new visible result.

## Shortlist

- [ ] Extend the shared level-directory loader into a complete campaign world loader.
- [ ] Recover the gameplay camera and entity eye transform instead of using raw spawn coordinates.
- [ ] Draw the loaded static geometry and resolve its materials, lightmaps, visibility, and unknown records.
- [ ] Fingerprint the original executable and reconstruct its subsystems as portable C/C++ using Ghidra and documented evidence.
- [ ] Implement streaming asset access and level loading within a stock Xbox's 64 MiB unified memory.
- [ ] Build an Xbox renderer and corresponding PC renderer that preserve at least PS2 visual parity.
- [ ] Restore the complete single-player campaign, including Geo-Mod, AI, physics, scripts, audio, menus, and saves.
- [ ] Validate performance and correctness in 64 MiB XEMU before USER hardware testing.
- [ ] USER: Review visual comparisons against the PS2 version when representative scenes are available.
- [ ] Consider multiplayer only after single-player completion.

## Current camera evidence

Swept tree and local room queries each match 5,000 original calls on PC and
NXDK; bind swept results to loaded-world face IDs, validate real-level sweeps
and XEMU execution, then recover transformed queries and actor response.

Zero-radius collision tree traversal now matches 1,800 complete original calls;
recover tree construction and world room selection, then connect loaded geometry
and mutable metadata to the running game.

Live Mines collision queries now use initial file/room filter metadata and
match all 7,418 original face-query results; recover later room mutations and
world traversal before using them for gameplay.

Collision face bounds now match the original finalizer for all 7,418 Live Mines
faces, including its 0.0001 expansion; continue recovering room state and
runtime filter metadata before world traversal.

Face access now preserves the original 32-bit file flags across 460,720 faces;
recover post-load flag changes and room ownership before enabling gameplay
collision metadata and world traversal.

All 7,418 Live Mines faces now bind to collision views and match original ray
results on PC and NXDK; recover runtime filter metadata and world traversal
before using the query for gameplay visibility or actor movement.

The combined zero-radius face query now matches 6,000 complete original calls
on PC and NXDK; connect real level faces and world traversal, then recover
texture-check modes and swept actor collision.

The candidate face filter now matches 28,356 original cases on PC and NXDK;
join it to geometric tests, recover later texture rejection and hit updates,
then implement world traversal.

Projected polygon containment now matches 6,858 original cases on PC and NXDK;
combine it with plane/box tests and recover face filters and world traversal.

The one-sided segment/plane test now matches 11,876 original cases on PC and
NXDK; recover polygon containment and face filtering before querying the world.

The original segment/box collision primitive now matches 15,678 original cases
on PC and NXDK; reconstruct world traversal and support queries described in
docs/COLLISION.md to complete crouch/stand physics.

The stance gate matches 2,292 original-code cases and crouch eligibility matches
8,751 cases through its visibility boundary; reconstruct the target-eye ray
query and crouch/stand collision effects before driving gameplay state selection.

The Xbox scene now plays registered authored stand/walk/crouch states from the
miner's 19-motion set; replace the diagnostic request schedule with gameplay
state selection and add authored action/weapon-specific playback.

Bulk registration now resolves 180 authored state groups, including 19 miner
base motions; feed these state sets into actor playback and retain explicit
failure for the unresolved edf_ship group.

Motion-cache lookup and reference acquisition now match 1,041 original-code
cases and supply diagnostic registration identities; finish payload release
and bulk authored state registration before replacing the fixed file setup.

Motion identity/loop-flag registration now matches 2,098 original-code cases
and drives the passing XEMU fixture; connect bulk authored state registration
and motion-cache ownership to replace the remaining fixed four-file setup.

Authored state binding now opens 1,137 compiled motions and reports the two
missing EDF1_Idle references; register these handles and drive real actor states.

State-motion metadata now matches all 1,139 base/weapon declarations across 63
classes; connect those mappings to registration/playback and resolve missing
EDF1_Idle plus the already missing edf_ship model without inventing substitutes.

Live Mines now retains its world and textures while miner UID 9858 animates for
64 frames on stock-memory XEMU, with the final pose matching PC; replace the
scripted miner sequence with authored actor states and recover the gameplay eye.

Placed diagnostic models now explicitly clip at the world's 0.1-unit near plane;
1,200 original polygon fixtures and a real miner boundary test pass, with the
existing combined image unchanged and numeric 64 MiB XEMU passing. Recover the
original near-plane setup and extend the frozen scene into animated actors.

Live Mines and miner UID 9858 now draw together on PC and stock-memory XEMU
with a passing framebuffer comparison; restore the gameplay camera, reconcile
near clipping and extend the frozen diagnostic into an animated actor scene.

Two Live Mines miners now produce posed geometry at authored transforms using
the world preview camera; combine world/actor draws and reconcile near clipping
before treating their camera paths as fully equivalent.

All 78 Live Mines entity records now bind by UID to installed skeletal meshes
and authored skins with exact transforms; feed these bindings into world drawing
and resolve each actor's animation state instead of the scripted miner fixture.

Animation scratch now uses a fixed 27 KiB heap allocation, reducing its Xbox
stack frame to 19,152 bytes; the measured stream-frame subtotal is 31,196 bytes.
Measure library/kernel stack demand and connect real actors to world drawing.

Parker's skin now completes 64 frames on stock-memory XEMU and matches the PC
framebuffer. Audit diagnostic stack usage and the earlier NV2A assertion, then
connect authored skins and real level entities to combined world drawing.

All five miner skin bundles now pass material and budget checks, and Parker's
authored skin renders in the PC preview. Connect skin selection on Xbox and
real level entities, and verify the original runtime skin-switch semantics.

The skeletal compiled-name helper matches 1,960 original-code cases and resolves
49 entity classes to installed meshes; 41 capacity guards pass. Recover the
separate .v3d model path, investigate missing edf_ship.v3c, apply skin lists and
connect real level actors to world drawing.

Entity asset metadata now matches all 63 table classes and 168 class/skin
selections. Recover compiled model filename resolution and apply skin lists;
investigate level class names camera1 and Bucket Bot before actor creation.

The bounded level-entity reader matches 1,610 records across 66 installed
levels. Resolve entity classes/skins to model assets and feed real transforms
into scene drawing; Live Mines miner1 UID 9858 is a nearby placement candidate.

Supplied world-camera/entity transforms now drive the running model stream on
64 MiB XEMU, with moving and fully culled placement checks on PC. Load real
level entity transforms and combine placed models with the world draw path.

Entity-local camera and view rotation now match 2,000 original placement-block
cases. Connect supplied world/entity transforms to scene model drawing and
recover the remaining view-stack and secondary-view state.

The miner inspection now streams 64 changing frames through reusable CPU/GPU
buffers on 64 MiB XEMU. Add sustained scene playback, world placement, original
lighting/material effects and performance measurements before PS2 comparison.

A textured, posed miner now renders on PC and 64 MiB XEMU, with a passing
framebuffer comparison and translucent faceplate. Add visible animation playback,
world placement, original lighting/material effects and PS2 visual comparison.

Resident model batches now assemble triangle routing, clipping and emission,
and real miner poses exercise index output on PC and 64 MiB XEMU. Independently
compare full assembled batches and connect their materials/vertices/indices to
the Xbox and PC draw paths for the first visible animated model.

Clipped-vertex projection and fan emission now match 2,400 original projection
cases and 600 output polygons, including 2,200 generated vertices. Connect these
stages to routed resident model triangles and verify the assembled draw inputs.

Multi-plane polygon traversal now matches 600 complete original-code triangle
fixtures byte for byte, including temporary-slot lifetime and output order.
Extend integrated coverage to near/custom planes and varying colors, then
recover clipped-vertex projection and bounded triangle-fan index emission.

Clipping temporary-vertex allocation and release now match 2,043 original
operations, including the 47-success allocation limit. Assemble the per-plane
polygon walker using the recovered pool, intersections and attributes.

Generated-vertex clip classification now matches 3,000 original cases and
shares the verified fresh-projection mask calculation. Assemble temporary
vertex allocation/release and polygon traversal around the recovered stages.

All seven clip-plane intersection branches now match 2,800 original cases.
Assemble intersection, attributes, clip classification and temporary-vertex
lifetime into per-plane polygon traversal before draw submission.

Clipping UV/RGB interpolation now matches 3,000 original-block cases under
explicit x87 rounding setup. Recover plane intersection factors/positions and
assemble per-plane polygon clipping with bounded temporary vertices.

Triangle clipping-input assembly now matches 6,000 original records, preserving
own UVs and reused positions/lighting. Recover edge interpolation, temporary
vertex lifetime and per-plane traversal before clipped triangle submission.

Triangle rejection/direct/clipping routing now matches 3,000 original cases.
Recover polygon clipping and bounded index emission, then connect the routed
triangles to material binding and Xbox draw submission.

Model triangle facing now matches 2,000 original renderer cases, including
two-sided flags and degenerate inputs. Connect frustum rejection, clipping and
index emission to the processed animated batches.

Real miner poses now feed resident render batches on PC and 64 MiB XEMU,
passing 47,616 vertex visits with bounded reusable buffers. Compare those render
outputs against original code and connect triangle submission for visible models.

All 599 installed model batches pass controlled resident-render processing,
including UVs and duplicate copies. Connect real evaluated poses and emulator
batch buffers, then triangle clipping and visible submission.

Resident batch processing now assembles deformation, projection, lighting and
reuse, matching 250 original-code batches. Exercise real animated geometry
through it and connect triangle clipping/material binding/draw submission.

Fresh model projection, depth and visibility now match 2,000 original-code
cases. Assemble deformation, projection, lighting and duplicate reuse into
resident batch processing, then connect triangle submission.

Duplicate-vertex processing now matches 2,000 original branch executions.
Recover fresh projection and connect cache lifecycle, deformation, lighting
and model draw submission using the resident batches.

Visible fresh-vertex normalization and lighting now match 2,000 original
call-site cases. Connect duplicate-vertex cache reuse and draw submission;
the missing normalization step is now explicit in the recovered render path.

The original camera-distance metric and detail choice now match 2,000 tested
cases. Supply live camera/scale state and connect selected model geometry to
draw submission; the current visible scene still contains static geometry only.

File thresholds now drive the resident loader through the recovered selector,
including the shared diagnostic on 64 MiB XEMU. Connect the live camera metric
and model draw submission before implementing in-scene detail switching.

Original model detail selection now matches 2,000 executable cases, including
flag overrides and exact threshold boundaries. Retain file thresholds and
connect the camera metric to selection-driven geometry residency and drawing.

The shared animation diagnostic now uses a budgeted resident geometry loader
on PC and 64 MiB XEMU. Independent traversal validates all 170 model LODs,
including exact-budget success and one-byte-under rejection. Connect resident
batches to renderer deformation, lighting and draw submission for visible models.

All 599 model batches now resolve through their LOD texture tables to owned
material indices. Connect these mappings and renderer-specific deformation
to draw submission, then validate the first visible character output.

All 744 miner LOD0 vertices now deform over 64 animation frames on PC and
64 MiB XEMU, matching 47,616 original collision-deformation evaluations.
Connect renderer-specific vertex processing, materials and lighting to drawing.

The shared miner diagnostic now prepares skinning matrices from real skeleton
and animation data across 64 frames, matching original code on PC and 64 MiB
XEMU. Connect actual vertex batches, material slots and lighting to drawing.

Skinning-matrix preparation and generation caching now match 2,000 original
cases across 8,000 bones. Connect actual stored transforms and animation poses
to prepared matrices, then feed deformation, lighting and model drawing.

Complete model lighting setup now matches 2,000 original-function cases.
Connect world candidate lights and model inputs, finish second-stream/pose
preparation, then integrate the recovered deformation and lighting into drawing.

Selected-light direction now matches 2,000 original cases, including flag
0x400 and singular inputs. Assemble fixed lighting and global gates with the
verified selection, direction and attenuation helpers for renderer integration.

Selected-light attenuation now matches 2,000 original cases. Finish light
direction normalization/rotation, fixed lights and global gates before
connecting the recovered setup to model rendering.

Nearest local-light selection now matches 2,000 original executions. Finish
fixed-light setup, selected-light direction/attenuation and global gates, then
connect game-derived lighting inputs to model rendering.

The model lighting helper now matches 2,000 complete original executions,
including non-finite vectors. Connect original light setup and second-stream
preparation before validating fully lit model rendering.

The bounded vertex-reuse reader matches all installed records, including
24,861 valid backward references. Integrate the renderer's cached position,
clip/depth and lighting reuse while recovering second-stream preparation.

Rendering's fresh-vertex deformation now matches 2,000 original executions
for both streams; recover second-stream preparation, duplicate reuse and
lighting before connecting the deformed batches to drawing.

Collision vertex deformation now matches 2,000 original executions: weights
use byte/256 and stop at the first zero, without sum normalization. Connect
prepared bone transforms and independently recover rendering deformation.

All active model bone indices fit their skeletons; all 12 non-finite normals
and 19,060 vertices with non-255 weight sums are triangle-referenced. Recover
the original deformation consumer before choosing weight or normal treatment.

Vertex/triangle readers now match all 85,866 vertices and 99,214 triangles in
the 95 models. Recover runtime bone-weight interpretation and handling of
12 source vertices with non-finite normals, then integrate skinning and drawing.

The bounded model batch directory now exposes eight aligned data regions for
all 599 batches across 170 LODs in 95 models. Decode the shared format descriptor,
verify positions/UVs/indices and skinning, and connect material slots to drawing.

Per-model material residency now connects actual decoded textures to owned
records with deduplicated local slots and a combined allocation budget.
All 95 models / 733 materials pass field and exact-budget checks; the largest
bundle peaks at 1,837,928 bytes. Connect bundles to model drawing and weapon
presentation, then add cross-model sharing/eviction and a total scene budget.

TGA format and material-alpha classification now match original instruction
blocks and accompany all decoded images. Connect per-model texture residency
and handles to owned materials, then integrate model rendering and presentation.

The shared named texture loader decodes all 385 model-referenced TGAs correctly.
Map original texture-format transparency, add per-model residency and handle
ownership, then connect decoded textures to model rendering and presentation.

Disk material conversion now covers all 733 installed records with bounded
scalar ownership. Resolve real texture handles/transparency and integrate model
instance materials with rendering before validating visible weapon presentation.

All 733 serialized materials across the 95 installed V3C models now stream
through the bounded reader and match independent traversal. Recover conversion
from their 84-byte disk layout to runtime materials before connecting ownership.

Instance materials now own bounded copies of their three arrays with cleanup
and failure checks. Connect this ownership layer to actual model loading and
texture overrides, then validate local weapon presentation in Xbox runtime.

Material fixed-field copying and array-size planning now match 2,000 original
cases through completion or first allocation. Finish bounded ownership of the
three copied arrays before instance materials can be published and rebound.

The 200-byte model material constructor now matches 1,000 full original
executions. Recover material copying and bounded instance allocation, then
connect texture overrides and local weapon presentation to owned model instances.

Model material counts now match 2,000 complete original executions across
wrapper types. Recover per-instance material allocation/copy and texture binding
before connecting the remaining weapon presentation adapter to live models.

The no-op presentation cleanup dependency is removed, and resource/backend
and mode-kind gates match 4,000 expanded original cases. Recover the remaining
resource/material binding operations and live model ownership for local presentation.

Weapon presentation control flow now matches 4,000 original-code cases using
already-loaded model handles. Implement its cleanup/resource/mode adapters and
actual model ownership, then connect local presentation and transition to gameplay.

Current-weapon lookup now matches 3,000 original-code cases and runs before
empty-ammo decisions in PC/XEMU; the original profile starts at the handler
entry. Recover local-player presentation, transition and earlier selection
gates to complete actual weapon switching and gameplay integration.

Ammo exhaustion now drives the selection tail, queues one replacement and
clears followup state in the shared 64-frame PC/original/XEMU diagnostic.
Recover earlier selection gates, local transition and presentation before
actual weapon activation and full gameplay locomotion can be assembled.

Selection followup now executes its actual three-field reset, matching 4,000
expanded original-code cases with 872 resets. Recover local-player transition
4aa0b0 and preceding selection gates, then integrate selection in Xbox runtime.

The selection tail now matches 4,000 original-code cases through paired
remapping, duplicate suppression, ownership checks and request queuing.
Recover the preceding 4a4a50 gates and actual message/apply/followup adapters,
then connect selection to gameplay and validate it in the Xbox runtime.

The weapon-selection queue mutation now matches 640 complete original-code
cases and builds for PC/NXDK. Recover selection eligibility and activation,
current-weapon presentation and firing adapters before connecting the empty
weapon decision to live gameplay; no new visible result is available.

Empty-weapon decisions now match 4,000 original-code cases across paired
firing dispatch, ammo messages and replacement selection, plus the XEMU diagnostic.
Implement current-weapon presentation updates and execute the selected actions,
then finish sound adapters and remaining gameplay locomotion physics.

Ammo lookup and ordered replacement-weapon selection now match 4,000 original
cases and an ammo-exhaustion XEMU profile. Complete the empty-weapon handler,
weapon switching/presentation, sound adapters and remaining locomotion physics.

Effect-pair switching now matches 4,000 original executions and runs through
weapon reset in XEMU; the false local-release callback was removed after
identifying a read-only lookup. Implement sound/player reset adapters, effect
ownership/rendering and following physics/state selection for full locomotion.

Valid-entity weapon reset now matches 2,400 original cases through completion
or explicit external-operation boundaries, and active-weapon nonloop stops run
in the shared XEMU diagnostic. Implement sound/effect/player reset adapters and
following physics/state selection, then assemble full gameplay locomotion.

The pre-candidate 800ms timer/reset gate now matches 5,000 original executions
and runs in the shared XEMU profile with 16 invalid-weapon resets. Recover valid
weapon reset effects and following physics/state selection, then assemble the
full locomotion selector with actual entity creation and registry updates.

Entity lookup, weapon/seat traversal, attachment readiness and combat eligibility
now match 2,007 original-code fixtures and feed the shared 64-frame XEMU
diagnostic. Implement actual entity creation/registry updates, preceding 800ms
reset, following physics/state selection and populated reset/audio adapters.

Locomotion candidate selection now matches 3,014 original executions, including
integrated sidestep/roll effects, asymmetric missing-state fallbacks and extended
precision speed thresholds; the expanded 64-frame diagnostic matches 64 MiB
XEMU. Recover combat predicates, preceding 800ms reset, following physics/state
selection and populated reset/audio adapters to assemble the full selector.

The shared diagnostic now matches original instructions and 64 MiB XEMU for
64 frames of controller blending, real sidestep actions, candidate selection,
playback, cached bones and eye transforms. Recover populated reset/audio
adapters, roll runtime coverage and actual entity initialization, then finish
the full locomotion selector and gameplay-camera integration. Original action
names identify 17/18 as sidesteps and 19/20 as rolls; `rf_turn_*` is historical.

The assembled candidate helper matches 3,004 complete original executions for
absent weapon-entry and sound fixtures, with all six outcome branches covered.
Recover populated reset/audio adapters and actual entity initialization, then
validate the full helper in Xbox runtime and finish gameplay-camera integration.

Remaining turn-candidate branches now match 4,800 original-code cases, including
activity-based restart suppression and fallback candidate selection. Recover
the preceding reset/decision logic and sound playback, then integrate the
complete helper into entity animation selection and the gameplay camera.

Turn direction gating now matches 5,208 original-code cases, including x87
dot-product boundaries and local-vector transformation. Integrate remaining
candidate/reset decisions with recovered turn effects; the full gameplay
selector, camera and animated character rendering remain open.

Selected-turn effects now match 2,400 integrated original-code cases across
action restart, five deadlines, candidates, turn flag and movement settings.
Recover the preceding candidate/reset decisions and valid-sound playback;
the complete gameplay selector and camera integration remain open.

Movement settings now match 6,000 original-code cases for speed, mode and the
conditional response field. Integrate the recovered action starts, five timers
and movement-setting effects into candidate/turn helper 0x41f9f0, then finish
the complete selector and actual player initialization.

Shared game clocks and deadlines now match 9,800 original-code cases, including
nested pauses and wrap boundaries. Connect the turn helper's five 1,200 ms
deadlines, recover its movement effects and finish candidate selection;
platform clock wiring and runtime timer validation remain open.

Entity action starts now match 6,000 original-code cases through loaded motion
restart and sound-class dispatch. Recover sound selection/playback, turn-helper
timer and movement effects, and the remaining candidate-selection logic before
integrating the complete gameplay selector.

Action-activity and remaining-time queries now match 7,000 original-code cases;
activity depends on end minus cursor, not slot weight. Recover candidate/turn
helper 0x41f9f0, including action starts and timer/movement side effects, then
finish the middle selector and actual player initialization.

Animation priority selection now matches 5,010 original-code cases, including
forced states, linked-entity predicates and the x87 velocity threshold. Recover
the middle candidate-selection and physics/AI branches, then connect actual
player entity fields to the complete selector and gameplay camera.

The movement selector tail now matches 6,000 original-code cases using entity
movement-vector, mode and direction fields. Recover upstream priority rules,
candidate selection, physics/AI side effects and actual player initialization
before connecting these decisions to the gameplay animation controller.

The shared controller-to-eye diagnostic now passes 64 frames in stock 64 MiB
XEMU, matching PC and original-code hashes through requests, interrupted blends,
overrides, playback and cached skeleton evaluation. Recover locomotion selection
and actual player initialization; animated character rendering remains open.

Logical state requests and membership checks now match 10,000 original-code
cases, including interrupted blends and standing fallback. Recover locomotion
selector 0x41f400 and actual player initialization flags, then integrate the
controller with playback and eye evaluation in the Xbox runtime diagnostic.

The post-selector animation controller now matches 2,406 original-code cases,
including transition timing, overrides and loaded-motion weight assignment.
Recover locomotion selector 0x41f400 and its state-transition setter, then
connect actual player flags and controller updates to eye initialization.
PC tests and NXDK compilation pass; this controller is not yet exercised in XEMU.

Standing/crouching/restored-standing pose queries now match 600 original bone
matrices and 24 eye transforms across eight starting states. Resolve controller
0x41f400 and actual player initialization flags: creation updates animation by
1/30 second before eye setup, so phase and cursor cannot be chosen arbitrarily.

Character reset and deferred animation-stop controls now match 1,800 original
cases, including preservation of slots/references until the next update.
Verify the full standing/crouching eye-offset setup sequence using these controls
and resolve initial-pose sampling before applying it to the gameplay camera.

Loaded-motion insertion, weight assignment and restart now match 3,200
original-code cases; the runtime diagnostic uses recovered insertion instead
of constructing slots directly. Recover lazy resource registration/loading and
the actual player initialization calls, including bone overrides and displacement
producers, before connecting the gameplay camera.

The shared 64-frame blended-skeleton check now passes in stock 64 MiB XEMU,
matching PC and original-code hashes for bones, eye transforms, playback and
cache state. Recover bone overrides, displacement producers and player setup;
animated character rendering and gameplay-camera integration remain open.

Per-bone generation caching now matches 960 repeated/invalidated original pose
queries, preserving queued displacement on cache hits. Verify animation in
Xbox runtime, then recover bone overrides, displacement producers and actual
player initialization before changing the gameplay camera.

Pending root displacement now matches original consumption and propagation
through 8,000 bone matrices and 320 eye transforms, including a synthetic
second root. Recover the producers of displacement, bone overrides and player
setup, then verify the integrated animation path in Xbox runtime.

Playback now feeds archive sampling, per-bone primary attenuation, pose blending,
parent composition and eye-tag evaluation, matching 4,000 original bone matrices
and 160 eye transforms after complete updates. Recover player setup, root
displacement and bone overrides, and add an Xbox runtime animation check before
using this path for the gameplay camera; the rendered diagnostic is unchanged.

Complete forward playback now matches 10,240 original-code updates across
160 scenarios of 64 consecutive frames, including mixed loops, completion,
freeze handling, compaction, primary state, generation wrap and sticky events.
Connect this state to archive sampling and skeleton evaluation, then recover
actual player initialization and camera animation inputs. No new rendered
behavior yet; publish native game/emulator screenshots when visible progress
is available. Xbox compilation passes; runtime integration remains open.

Non-loop cursor advance and primary-candidate selection within the mixed-loop
branch now match 3,000 original-code cases. Assemble the complete update from
the verified stages and test consecutive frames, preserving the original
slot-index versus motion-ID distinction in fade-bypass lookup.

The non-loop completion pass now matches 1,360 original-code cases, including
freeze-slot retention and primary-state clearing. Assemble and verify the full
animation update across consecutive frames, including primary selection during
advance, zero-weight removal and frozen-instance early exit.

Animation-slot removal now matches 2,040 original-code cases, including ordered
compaction, selected-index repair and reference-count clamping. Integrate this
with non-loop completion and freeze handling, then connect the playback state
machine to archive sampling and skeleton evaluation.

Loop phase-to-cursor mapping and two-marker event crossing now match 3,000
original-code cases, including floor rounding and exact marker boundaries.
Integrate phase/cursor updates, then recover non-loop completion, frozen slots
and inactive-slot removal for complete animation playback.

Shared loop-phase advance now matches 1,600 original-update comparisons,
including wrap flags and dominant-slot selection. Recover end freezing and
inactive-slot removal before
integrating continuous playback with the verified skeleton path.

Character tag placement now matches 1,000 original-wrapper comparisons; this
wrapper rotates and translates the evaluated tag without an extra scale step.
Recover actual player setup inputs and animation slot state, then integrate and
validate the animated camera path in XEMU.

The shared single-motion skeleton path now matches 300 original bone matrices
and 12 animated miner eye transforms for stand/crouch poses. Recover looping
slot updates, root displacement, overrides, model scaling and camera state
selection before applying this model-space eye to gameplay or the Xbox preview.

Archive-backed sampling now matches 131,965 full-track evaluations while holding
only two rotation keys and two position keys. Cache disk reads and integrate
these samples with slot weights, parent composition and eye attachment evaluation.
Open validates all tick ordering; 62 malformed-file fixtures are rejected.

Local pose blending now matches 1,600 original-code matrix comparisons across
one through sixteen contributors. Connect per-track samples and normalized
weights to parent composition, root motion and the model's eye attachment;
the complete 0x51b500 evaluator and playback update still remain open.

Elapsed-to-tick conversion now matches 8,005 original-instruction checks.
Reconstruct the remaining 0x51ba80 update state machine: weighted shared loop
phase, per-motion cursors, end freezing and inactive-slot removal; then connect
cached archive playback and skeleton evaluation.

The shared archive-backed RFA reader now validates all 1,009 installed motions
and reads all 489,545 rotation keys and 147,716 position keys bit-for-bit without
whole-file allocation. Connect indexed reads to cached playback, engine-time
conversion and skeleton evaluation; trailing regions in 57 files remain unclassified.

Per-track blend-weight envelopes now match 575,460 original-code comparisons
covering all 26,393 installed track envelopes plus 500 synthetic envelopes.
Connect weights and sampled transforms through the original skeleton blending
path after implementing playback caching and engine-time conversion.

Rotation-track sampling and easing now match 570,824 original-code comparisons
covering all 26,393 installed tracks plus 300 synthetic tracks. Implement
playback caching, engine-time conversion and animated skeleton
evaluation before integrating camera setup. Single-key sampling at/before its
tick deliberately avoids an original out-of-bounds read by decoding directly.

Packed quaternion interpolation now matches 1,288,315 original-code comparisons
at five blend positions across all 256,663 distinct installed adjacent/single-key
pairs plus 1,000 synthetic pairs. Xbox runtime arithmetic equivalence remains
to be checked when the animation path is integrated into the diagnostic.

Packed rotation decoding is reconstructed in shared C, with all 65,536 signed
component values checked in each of four positions against original instructions.
The decoder, packed interpolation and track evaluation are available; integrate
them with motion loading and animated poses.

Position-key sampling is reconstructed in shared C with 755 bit-exact original
comparisons covering stand/crouch and synthetic tracks. Recover engine-time
conversion and full
motion loading before integrating the camera pose.

Original state initialization confirms stand slot 0 and crouch slot 8; the
base miner1 motions resolve to installed ult2_stand/ult2_crouch RFA assets.
The motion-envelope audit accounts for all 1,009 files and preserves additional
regions in 57 files. Recover keyframe encoding, timing and weapon override
selection before using these motions to set camera offsets.

Attachment transforms now preserve the original unnormalized quaternion path,
with 856 exact original-instruction comparisons including all 755 installed
records. Bone transforms retain normalization; do not use the bone API for
attachment setup. Animated parent poses and model integration remain open.

Shared C now computes aligned LOD attachment ranges and reads all 755 installed
attachment records bit-for-bit; miner's `eye` references bone 8 in each LOD.
Integrate skeleton and attachment tables, validate upper parent bounds, and
recover animated bone poses before using this local attachment as a camera.

Shared C now traverses V3C section structure directly in VPP archives, including
zero-sized SUBM chunks, while leaving LOD blobs on disc. Bone verification uses
structural boundaries instead of signature searches. Decode LOD geometry,
attachments and animation data next; the directory alone does not load a model.

The suspected bone remapping is a stable depth-sorted evaluation order, now
reconstructed in shared C with 200 exact original-instruction comparisons
(95 asset skeletons plus 105 synthetic cases). It does not change file indices.
Use this order when integrating animation poses; full V3C parsing and animation
sampling/blending remain unfinished.

Parent transform multiplication at 0x51c620 is reconstructed in shared C and
matches 2,852 original-instruction comparisons bit-for-bit on PC. Original bone
remapping and full hierarchy/animation evaluation remain open; isolated matrix
products do not yet establish a correct animated eye pose.

Local bone normalization and transform construction are now reconstructed in
shared C; 2,852 PC results match the original instructions bit-for-bit, covering
all 2,452 decoded asset bones and 400 scaled synthetic quaternions. Continue with
parent composition, animation evaluation and proper V3C traversal before wiring
the reconstructed eye pose into gameplay.

The shared raw BONE payload decoder now preserves fields across 95 detected
installed sections (2,452 bones), with no runtime allocation and validation of
finite transforms and acyclic parent indices. Finish structural V3C section
parsing, name processing and original quaternion conversion before pose use;
the verification tool's signature scanning is only diagnostic discovery.

Player creation now identifies the default `miner1` class, and unhooked original
filename resolution confirms `miner.vcm` -> `miner.v3c`. The asset audit resolves
45 of 46 entity-table character references; investigate `edf_ship.vcm` use and
fallback before treating its missing .v3c as a blocker. Parent-chain pose
evaluation is traced to 0x51b2e0/0x51b500 but remains unreconstructed.

Character tag lookup is now reconstructed in `src/core/model.c` and included in
PC/NXDK builds; 306 comparisons against original 0x51d5b0 and its unhooked
default-locale comparator pass. This covers ordered lookup, not model loading
or animated poses. Continue into 0x51b2e0, asset name resolution and player model
selection; runtime pointer layouts must not be used as file layouts.

Camera evidence update: original offset initialization at 0x423bd0 samples the
model's `eye` tag into class standing/crouching fields using identity rotation
and zero origin. Eight isolated original-instruction cases pass in
`tools/inspect_eye_setup.py`, with explicitly synthetic model hooks. Missing
tags and the info +0x724 flag 0x20000 behavior are verified. Recover player model
selection, animation indices, asset parsing and pose evaluation before feeding
these offsets into the campaign camera; no guessed eye height is authorized by
this evidence. See `docs/CAMERA.md` for addresses and remaining uncertainties.

## Accepted requirements

- Original Xbox is the primary platform; only stock 64 MiB hardware is supported.
- A PC build must share the Xbox-compatible game implementation and resource limits.
- C/C++ reconstruction and recompilation using NXDK; Ghidra is available for analysis.
- Single-player campaign first; multiplayer is a stretch goal.
- Resource reductions are allowed only while maintaining PS2-level visual parity.
- XEMU is the primary emulator; CXBX-R may assist with diagnosis.
- Never control the desktop or send host input; use source, files, logs, non-interactive commands, and process-contained test harnesses.

## Milestone 0 — evidence and toolchains

- [x] Set the persistent project goal.
- [x] Inspect the initial workspace: only an empty Git repository was present.
- [x] Locate NXDK at `C:/nxdk` and Ghidra at `C:/Programming/ghidra_11.3.2_PUBLIC`.
- [x] Locate the game, record file hashes, PE architecture/version/imports, and asset archive inventory.
- [x] Record compiler, SDK, emulator versions and reproducible commands.
- [x] Evaluate Open Faction and Dash Faction as references, recording provenance and reuse terms.
- [x] Compile and validate minimal PC and Xbox artifacts; distinguish scaffolding from recovered game behavior.

## Milestone 1 — reconstruction foundation

- [x] Create a reproducible headless Ghidra project from a hash-identified executable.
- [ ] Map startup, filesystem/archive loading, memory allocation, game loop, rendering, audio, input, and level transitions.
- [x] Start a recovered-function register with original addresses, evidence, dependencies, confidence, and behavioral tests.
- [x] Implement bounded archive access and inspect the first campaign level's section boundaries.
- [x] Load level directories and spawn transforms in shared C, validate all 94 installed levels on PC, and validate Live Mines in 64 MiB XEMU.
- [x] Load budgeted resident static geometry in shared C, validate all 94 installed levels, and verify Live Mines geometry in 64 MiB XEMU.
- [ ] Reconstruct level payload parsing and original archive mounting/override semantics.
- [ ] Establish native emulator visual capture without any desktop capture or window input.
- [ ] Establish memory accounting that includes executable, heaps, stacks, GPU surfaces, audio, and transient loads.

## Milestone 2 — playable campaign slice

- [ ] Load and draw the first campaign area on both platforms.
- [ ] Restore collision, player movement, controller mapping, weapons, enemies, events, and Geo-Mod.
- [ ] Restore HUD, sound, dialogue, cutscenes, saves, and level transitions needed by that slice.
- [ ] Compare matched PS2 scenes and capture native emulator output for review.

## Milestone 3 — campaign completion and validation

- [ ] Extend subsystem coverage through every campaign level and document completion evidence.
- [ ] Run memory stress, loading, save/reload, destruction, AI, and frame-time regression scenarios.
- [ ] Validate on 64 MiB XEMU, then prepare a reproducible USER hardware test package.
- [ ] Fix hardware findings and finish PC regression coverage.

## Current status

Original inputs are at `D:/Programming/GitHub/OpenRedFaction/Installed_Game`;
source work is in this repository on D:. RF.exe and tables.vpp match the
Red Faction 1.20 NA baseline. Inventory: 39 files, 15 VPP archives, 8,410 entries.

Both a 32-bit PC archive diagnostic and an NXDK XBE/XISO compile. The shared C
VPP reader matches the complete installed archive directory inventory and passes
malformed-input/bounds tests. One function, the filename checksum at `0x0052be70`,
has been reconstructed and verified against original x86 for 9,671 test inputs
in the C locale. Ghidra identified 7,248 candidate functions including libraries;
this is not a measure of completed reconstruction.

The diagnostic boots and validates tables.vpp on 64 MiB XEMU with both installed
4627 debug and Complex 4627 v1.03 BIOSes using the xemu rendering backend.
The headless `-display none` path faults before entry in this emulator build;
its exact cause remains a non-blocking harness investigation. The harness uses
isolated config/EEPROM, temporary HDD writes, and monitor-only guest telemetry.
UT99 reference harness location resolved to
`C:/Programming/GitHub/UnrealTournament_1.40/UT99-Xbox/Tools`.

All 94 installed RFL level files have been scanned for section boundaries.
The first campaign level is `L1S1.rfl` (Live Mines), version 180, 3,572,594 bytes,
with 29 payload sections plus an end marker. Static geometry is 1,637,649 bytes
and lightmaps are 1,130,684 bytes on disk; neither number is a runtime memory estimate.

No playable game, PS2 visual comparison, or hardware validation exists yet.

### Latest progress

Moved the authoritative source project, references, and analysis databases to
`D:/Programming/GitHub/OpenRedFaction` after the user's location correction.
Original inputs remain in its `Installed_Game` subfolder and are ignored by Git.
The C-drive task workspace contains relocation pointers; its old build outputs
are retained under `local/legacy-build-c` on D:.

The shared C level loader now validates v180 section directories, bounds, header
offsets, end markers, and finite spawn transforms without loading whole RFL/VPPs.
All 94 installed directories and spawn transforms agree with independent binary
reads; two CTest suites pass. Live Mines' directory, geometry/lightmap section
sizes, and exact spawn position are verified in 64 MiB XEMU from a disc containing
the unmodified 68.8 MB levels1.vpp archive.

Static geometry now retains original payload bytes and builds texture/room/face
offset indices under an explicit allocation budget. Live Mines has 6,658 vertices,
7,418 faces, 29,110 corners and 54 rooms, using 1,667,605 requested bytes for this
geometry representation. All 94 installed levels load on PC; the largest requests
2,131,016 bytes. Accessor-bound and allocation-budget checks pass, as do nine
corrupted geometry cases. XEMU verifies counts and first/last decoded positions
with geometry resident. This is not yet a rendering, collision, or gameplay test.

The initial untextured static geometry preview now renders on PC and Xbox from
the stored Live Mines spawn. Three frames execute in 64 MiB XEMU; a native guest
framebuffer capture matches the PC reference within the diagnostic edge tolerance
(191 of 307,200 pixels exceed three channel values). Fixed a W-buffer override
after back-buffer selection. This provisional camera and flat face coloring do
not establish original rendering fidelity; textures, lightmaps and gameplay remain.

The shared C TGA decoder now reads 24/32-bit raw and RLE images directly from VPP
entries with a fixed 4 KiB input buffer and explicit output allocation budget.
All 16 Live Mines TGA references resolve across maps1/maps2/maps3/maps4/maps_en;
RGBA8 base images total 2,117,632 bytes. USERBMAP remains unresolved. Pixel output
matches Pillow for 41 installed images covering all supported observed header
variants and every RLE image; 17 synthetic origin/truncation/packet cases pass.
One installed paletted TGA remains unsupported.

The shared material loader now uses bounded geometry-name access and explicit
archive search order. It allocates 2,118,040 bytes including 17 material slots;
the exact budget passes and one byte less fails with cleared state. It loads the
16 images inside stock 64 MiB XEMU and retains USERBMAP as an explicit missing
slot. Guest pixel checksum 0xa3aeb67d matches independent decoding. The material
images remain resident alongside geometry and the three-frame GPU diagnostic.
The shared preview now clips source UVs with geometry and carries u/z, v/z, 1/z
and material indices. Its vertex allocation is now 1,460,280 bytes. The PC reference
samples original textures with perspective division and nearest/repeat sampling;
artifacts/live-mines-textured-pc.png shows the first base-textured tunnel view.
Xbox now uploads swizzled base textures and samples projective UVs. Selecting
border-color source fixes initial corrupt sampling because images contain no
border texels. The textured tunnel is visible, but the strict PC comparison fails:
29,235/307,200 pixels exceed three channel values (mean maximum-channel error 1.346).
Resolve sampling differences before claiming PC/Xbox equivalence. Lightmaps,
filtering, animation and archive override rules remain open. Fixed stale Xbox
header dependencies caused by Windows/POSIX path mismatch; a rebuilt XEMU frame
with the expanded vertex stride passes the untextured comparison.

Next: bind decoded materials to geometry on both renderers, load lightmaps, recover portal/room visibility,
and investigate trailing records that the community geometry loader leaves opaque.
Original geometry-loading function semantics still require Ghidra reconstruction;
the new loader implements the observed file layout and preserves unknown bytes.

Sampling diagnosis: optional RF_PREVIEW_SNAP_1_16 quantizes only the PC diagnostic
screen coordinates; it reduces the textured mismatch to 1,222 pixels (0.398%),
still FAIL against the existing tolerance. This is an unverified raster precision
hypothesis, not a hardware rule or a relaxed acceptance threshold. Default PC
output remains unchanged. A 1/32 sample-center shift was less effective and removed.

Lightmap inspection now covers exact image boundaries in all 94 installed level
sections. Live Mines has 23 RGB lightmaps, expanding to 1,507,328 RGBA bytes;
largest installed level expansion is 3,670,016 bytes. The shared C lightmap loader
now loads all 94 levels under exact output/record budgets, rejects one byte less,
and clears allocations on failure. Live Mines' 23 decoded pixel hashes match
independent expansion of original RGB bytes. PC/NXDK builds pass. Geometry mapping
indices now resolve through the first word of each 96-byte record and all indices
are in range across all 94 levels. Live Mines' lightmaps remain resident in 64 MiB
XEMU alongside base materials and geometry; the base-textured frame is unchanged.
Lightmap UV propagation, blending rules and GPU sampling remain open. Open Faction
uses doubled modulation, but original-binary investigation shows this is conditional.
Original initializer 0x515730/constructor 0x411e00 emulates to solid mode 0x400024
at 0x1808328. Original state function 0x54f160 uses modulation in texture-source
case 4 and doubled modulation in case 5, with runtime byte 0x1cfcc1d selecting a
4-to-5 remap. Trace that flag and lightmap conversion before fixing the lighting
equation. See docs/RENDER_STATE.md; no unconditional lighting pass was substituted.

The runtime flag is now identified as TextureOpCaps MODULATE2X: original write
0x545f46 extracts bit 0x10 from 0x1cfcb58, and the original logger calls it D3Dm2x.
The shared mesh now clips lightmap UVs and carries projected coordinates plus
resolved image indices (56-byte vertices, 2,044,392 bytes for Live Mines).
The second texture stage now samples lightmaps with linear clamping and combines
them with linearly wrapped base textures using doubled modulation. Draws split at
material/lightmap boundaries. The first lit tunnel renders on PC and 64 MiB XEMU.
Strict comparison still FAILS: 5,248/307,200 pixels exceed three channel values
(mean maximum-channel error 0.609). PC/NXDK builds and existing CTest checks pass.
Original texture conversion, fog, alpha, mipmaps and PS2 comparison remain open.

The shared projection now uses the same explicit 1/16-pixel grid on both targets;
the former PC-only diagnostic flag has been removed. The latest lit-frame
comparison PASSES: maximum channel difference 1, zero pixels over the existing
three-value tolerance. Schema-7 XEMU telemetry measures GPU image requests
3,624,964 bytes and vertices 2,044,392 bytes, with 44.25 MiB available after uploads.
This frozen-scene snapshot is not a full-game peak or PS2 parity result.

Camera investigation now identifies first-person entry at 0x40ddf0, camera
position access at 0x40d760 and projection setup at 0x547150. First-person entry
uses the entity's computed eye position/orientation; the raw spawn lacks that
transform. Original projection constants are fingerprinted in camera-evidence.json.
No guessed eye-height or original near/far interpretation was introduced. See
docs/CAMERA.md for the entity/model and projection work still required.

Camera update dispatcher 0x40d850 is now exported; first-person mode copies the
player eye point to camera position before additional effects. Corrected thiscall
metadata for original vector helpers 0x409f40/0x409f70 makes that data flow explicit.
An original-instruction operand scan records candidate eye-field references for
further tracing; the standing/crouching eye writer is still unresolved.

The writer is now identified as 0x4194e0. Its non-linked branch is reconstructed
in src/core/eye.c: states 8/9/10 select crouching, offsets are transformed by the
entity orientation and blended during transitions, then added to position.
400 deterministic cases match original x86 execution exactly in the PC test.
The animated eye-tag branch returns explicit RF_NOT_FOUND; model-derived offsets,
entity state loading and actual camera integration remain open.
