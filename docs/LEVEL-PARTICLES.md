# Level particle runtime loading

`rf_level_particles_open` joins the bounded A00 reader, template conversion,
room locator, persistent materials and fixed particle/emitter pools. Both PC
and Xbox builds compile this shared C module. It is a port-owned loader, not
a claim of equivalence to the complete original loader at `45fcf0`.

The state owns 1600 particle records, 128 emitter slots and 133 list headers.
Heap allocation keeps internal pool pointers stable when the output owner is
returned. Materials retain decoded pixels after the source archives close;
the collision world may also close after room handles have been resolved.
Room handles use index + 1; texture handles are local deduplicated slots.
Original level owner 0 is retained, with no supplied parent object.

The original conversion leaves template age-to-finish unassigned. This loader
explicitly initializes it to zero; original stack contents are not reproduced.
Creation follows verified `497ca0`/initializer behavior, including authored
initial emission, but the module does not yet tick or draw campaign particles.
The resolved nonnegative-owner eligibility path is now available through
`rf_particle_pool_step_resolved`; campaign callers must supply actual lookup
results before stepping these level particles.

`python tools/verify_level_particles.py` checks all 87 emitters in 20 installed
levels against the reader, room and material reports. It checks bindings,
enable state, active-list links, initial particle counts, deterministic repeat,
exact-budget success and one-byte-under failure. The native PC probe also
checks internal heap pointers, image access after archives close, empty output
on failure and repeated cleanup. Generate prerequisite reports with
`inspect_level_emitters.py`, `verify_level_emitter_binding.py` and
`verify_level_particle_materials.py`.

Measured 32-bit residency is 238932 bytes for L1S1 and at most 484908 bytes
among these levels. This includes the owner, fixed state, material arrays and
decoded pixels; it excludes caller archive/world storage, stack and allocator
metadata. These are shared PC loader measurements, not native XEMU page
measurements or proof of whole-campaign memory fit.

## Owner eligibility

Original `495120` copies current position to previous position before checking
ownership. Negative handles bypass lookup. Other handles use `40a0e0` (including
generation validation), then take object UID at +0x20, or -1 for a missing object.
`45d630` searches the vector at 0x646080 for the first level entry with that UID.
No matching entry allows simulation. A match calls `497390`: a null room pointer at
emitter +0x4c freezes simulation, otherwise the room byte at +0x160 decides.
This is a room eligibility check, independent of emitter enable at +0x140.
Freezing still commits the previous-position copy, without aging or expiry.

The shared gate contains resolved entry presence, room presence and room visibility
value. Only the low visibility byte matters. Nonnegative owners require a supplied
gate; missing caller information is not treated as a failed original lookup.
The existing unowned API retains its rejection of nonnegative owners. Collision,
swirl, wind and damage remain unsupported for advancing particles.

`verify_particle_owner_step.py` executes full original `495120`, actual handle
and level-vector lookups, room accessor and simulation helpers against the
native PC probe and NXDK machine code under Unicorn. All 2048 cases match exact
particle fields, live counts and bounds; 768 freeze, 670 expire and 344 expand
bounds. Cases include missing and stale handles, missing level entries/rooms,
visibility values 0/1/255/256 and negative-owner bypass with an ineligible matched room.
This is function-level replay, not XEMU gameplay or completed caller integration.

## Frame bounds finalization

`rf_emitter_pool_finish_bounds` reconstructs `497df0`, called after `496480`
simulation in the original `433260` frame loop. With a nonzero global enable
byte it walks active emitters; nonnegative-owner emitters receive
`sqrt(maximum_distance_squared) + max_radius` as their estimated radius and
clear the accumulated distance. Negative-owner emitters retain both fields.
This preserves the original double-precision square root/add before float
storage. `497de0`, called immediately before simulation, is a no-op in this build.

`verify_emitter_finish_bounds.py` passes 2048 original/PC/NXDK single-active-slot
cases (680 updates), including low-byte global gating, signed owners, zero and
negative-zero bounds. Full active-list and campaign integration remain open.
Both original frame-loop passes use vector 0x646080 in authored order:
433374..4333bb before simulation, 433455..433493 after bounds finalization.
Each rechecks the emitter room via 497390 before invoking 4972f0 on the same
emitter pointer. The earlier suggestion of separate collections was incorrect.
Do not substitute emitter enable for room eligibility: phase changes happen
inside 4972f0, while the room check gates entry to that function.

## Source of room eligibility

Original `4d2f80` traverses the room vector at world +0x90 and clears only
room byte +0x160. `431820` calls this reset using the world at 0x6460e8 before
its player-view loop. The reset does not clear the adjacent +0x161 visit flag.

Room traversal `4d4860` sets +0x160 and +0x161 to 1 on accepted visits. It
rejects rooms with nonzero bytes at offsets 0 or 1, records traversal depth
at +0x164 and screen bounds at +0x16c..+0x178. Repeat visits union their
rectangles and retain one room entry in the ordered visible-room list.
Flag 1 skips recursive portal traversal. Caller `4d4760` selects an all-room
nonrecursive fallback or traversal from the supplied starting room.

`verify_room_visibility_lifecycle.py` executes the original reset with 0, 1,
17 and 128 rooms and eight original nonrecursive visits, without stubbing
their callees. It verifies byte-preserving reset, eligibility predicates,
visit flags, rectangle union and duplicate handling. This establishes that
particle room eligibility is render-derived. It does not establish complete
portal traversal or simulation-versus-render timing. The shared diagnostic
renderer currently lacks this room visibility lifecycle; campaign wiring
must recover it rather than substituting emitter enable or camera room alone.

## Shared visibility bookkeeping and timing

`visibility.h`/`visibility.c` provide caller-owned, allocation-free room state
and an ordered visit list. Begin-render clears eligibility; begin-view clears
visited flags, sets depth to 255 and empties the list without clearing render
eligibility. Accepted visits union rectangles and move revisited rooms to the
tail. Portal clipping and room predicates remain the caller's responsibility.

`verify_visibility.py` compares 512 reset/view/visit commands with actual original
functions, native PC and NXDK code under Unicorn. All room fields and list slots
match across eight rooms. Original begin-render uses world vector +0x90 while
begin-view uses +0x9c; this test supplies the same room set to both, and their
membership distinction remains to be reconstructed for scene integration.

The normal `433520` path calls simulation at `43363b`, then rendering at
`433645`; `431820` resets room eligibility before its player-view loop. Thus
that simulation path consumes visibility retained from the preceding render.
Its alternate branch at `433558` simulates and returns without rendering. This
is call-order evidence, not proof of every exceptional game-loop path or startup
visibility initialization. The shared bookkeeping has not yet been wired into
the campaign renderer, and does not on its own implement portal traversal.

## Traversal with resolved portal rectangles

`rf_visibility_traverse` now implements the control flow of `4d4860` using
caller-resolved portal rectangles/rejection flags. It follows authored adjacency
order, intersects the incoming rectangle with each portal rectangle, rejects
zero-area intersections, and uses signed room depth comparisons to avoid walking
back into ancestors. Child depth returns to 255 after each call, including a
blocked child. Detail rooms stop traversal unless they are the supplied special
room. Flag 1 disables traversal while retaining the starting-room visit.

The shared implementation uses 257 caller-owned frames (7196 bytes), avoiding
native recursion and heap allocation. `verify_visibility_traverse.py` compares
256 cyclic four-room/five-portal cases against full original recursion and
PC/NXDK code. All room fields, depth markers, rectangles and ordered list slots
match, including blocked/detail/special rooms and rejected/nonoverlapping portals.
Original portal caches are supplied as already resolved in these fixtures;
projection, cache generation, authored graph loading and native renderer wiring
remain unfinished. This advances the traversal layer, not visual gameplay.

## Authored portal records

`rf_geometry_portals` exposes the v180 records immediately after room-child
lists without extra resident allocations. Each 32-byte record holds two room
indices followed by minimum/maximum vectors. The original loader `4ed520`
reads the indices, resolves both room pointers, calls `4f9890`, then reads the
vectors into portal +8 and +0x14. Constructor `4f9890` appends the new portal
to world and endpoint-room lists. File order therefore supplies adjacency order.

The parser now rejects out-of-range endpoint indices and nonfinite bounds.
`verify_geometry_portals.py` independently walks all 94 installed geometry
payloads and compares 2862 complete records with the PC accessor. Every installed
bound is ordered and finite. The probe checks insufficient-capacity preservation;
`test_geometry_corruption.py` additionally rejects oversized counts, either bad
endpoint and nonfinite bounds in private VPP fixtures. This is file-format and
PC accessor verification; original constructor execution, NXDK runtime binding
and portal screen projection are still open. No installed game input is edited.

## Persistent authored adjacency

`rf_geometry_portal_graph_open` owns portal endpoints/bounds plus room offsets
and adjacency indices in one allocation. It counts endpoints, computes offsets
and appends each portal to both room lists in file order. A self-link would be
appended twice, matching the constructor. Geometry can close after success.
The graph deliberately carries no invented room visibility or projected bounds.

`verify_portal_graph.py` executes original `4f9890` and actual vector append
helpers for all 2862 portals across 94 levels, with preallocated vector capacity.
Each room's ordered links and constructor-assigned index match the persistent PC
graph. The probe reads bounds after geometry closure, checks exact-budget success,
one-byte-under failure with empty output, and repeated cleanup. Measured graph
residency is 1408 bytes in L1S1 and at most 6260 bytes across these levels; owner
and arrays are included, allocator metadata and source geometry excluded.
Both platform builds compile the graph; native Xbox residency, room flags,
screen projection and traversal/renderer integration remain open.

## Portal preprojection classification

`rf_visibility_portal_classify` follows `4d4860`'s call order: `507ba0` first
tests the camera against bounds expanded by exactly 1.0 on each axis, with
inclusive boundaries and float-rounded expansion. Inside uses the full viewport
and bypasses plane testing. Otherwise `518750` selects one of eight box corners
for each supplied view plane and rejects on strictly positive signed distance.
The plane calculation preserves the original Z/Y/X double accumulation order.

`verify_portal_classify.py` executes both original functions and their actual
callees against PC/NXDK code in 2048 cases: 1024 full-viewport, 759 rejected and
265 requiring projection. It covers expanded boundaries, just-outside positions,
zero through eight supplied planes, all corner selectors and zero distance.
This does not reconstruct view-plane generation or infer corner selectors from
normals. The remaining `515d00` path transforms eight box corners and clips six
box faces before accumulating their screen bounds; that projection remains open.

## Clipped box screen projection

`rf_visibility_box_project` now reconstructs `515d00` and the `518bf0` view
transform, reusing verified clipping/projection arithmetic in `effect.c`.
It creates eight corners in original order, classifies them, processes six
faces in original order and accumulates screen extrema from accepted faces.
Rejected boxes leave the caller's rectangle unchanged while clearing visible.
The caller supplies the original view matrix, origin, flat/perspective mode,
clip settings and projection values; renderer dispatch mode 0x66 is supported.

`verify_box_projection.py` executes full original `515d00` with actual transform,
clipping, projection and temporary-pool callees, comparing 1024 PC/NXDK cases.
All flags and rectangle bits match (371 accepted), including rotated views,
flat depth, far clipping, offscreen/behind-camera and degenerate bounds.
Case 103 caught a missing post-clip common-plane rejection in the new box
caller; skipping that face fixed the discrepancy. The particle helper itself
did not need changed arithmetic. This is function replay; view setup, portal
cache integration and native campaign rendering remain unfinished.

## View-plane construction

`rf_visibility_plane_normal` and `rf_visibility_plane_points` reconstruct the
math behind `547b90` and `547b40`. The first preserves the supplied normal;
the second forms float-rounded (b-a) and (c-b), crosses and normalizes them.
Both compute negative normal-dot-point in original Z/Y/X order and select the
minimum-distance box corner through `5398a0`'s strict-positive sign branches.
Zero components take the nonpositive branch. The compact result omits the
original clip-bit tag, opposite-corner selector and padding, which these callers
do not consume. Degenerate three-point inputs are rejected without mutation.

`verify_visibility_planes.py` compares 2048 original constructor executions,
including actual vector/distance/selector helpers, with PC/NXDK results. Normal,
distance and selected corner match exactly, including all normal sign/zero
combinations. Full view setup `546a40` still needs reconstruction: it constructs
the frustum corners, assembles different planes for perspective and flat modes,
and updates scaled clipping distances. These plane constructors alone do not
establish live camera/frustum equivalence.

## Complete frustum assembly from resolved view state

`rf_visibility_frustum_build` now reconstructs full `546a40`: the four corner
positions, side planes, mode-dependent camera/far planes and scaled near/far
distances. Perspective views produce five planes, or six with far clipping;
flat views produce four. It preserves unused output slots, reads low-byte mode
flags, and uses float rounding at the original intermediate vector operations.

`verify_visibility_frustum.py` compares 1024 full original executions with
PC/NXDK code, including the actual vector helpers and constructors. All plane
normals/distances/corners, clip masks, counts, scaled distances and retained
slots match: 512 four-plane, 256 five-plane and 256 six-plane cases. Inputs cover
translated/rotated bases and varying scales/distances. This supersedes the open
frustum-assembly item above; FOV/window-to-view derivation, portal cache wiring
and native campaign rendering still remain open.

## Viewport and FOV scale derivation

`rf_visibility_view_scale_build` reconstructs the selected arithmetic in
`547150`: viewport half sizes/centers, X/Y/Z view scales, flat depth and inverse
depth scale. Perspective values below 2 are already-scaled inputs; other
perspective values use tan(FOV * float-degrees-to-radians / 2). Flat mode halves
the supplied value. Far distance is clamped to 31.25..1000 for perspective Z
scaling. Aspect uses height * pixel-aspect / width, rounded before use.

`verify_visibility_view_scale.py` runs original `547150`, including actual
matrix/frustum callees, while intercepting only graphics-state call `50ce40`.
576 PC/NXDK cases match the selected output bits across modes, viewport/aspect,
FOV threshold and far clamp boundaries. It caught premature depth-scale rounding:
the original retains the unrounded division when deriving X/Y and reciprocal.
The shared routine now does the same. This does not implement every side effect
of `547150`; camera matrix processing, graphics state and live integration still
need their own evidence. Ordinary perspective input requires 0<FOV<180.

## Combined visibility camera

`rf_visibility_camera_setup` composes viewport/FOV scaling, frustum construction
and box-projection state. Original `547150` preserves the input camera basis and
copies it before scaling each row by X/Y/Z view scale. `4fce70` and `4fad00` in
that routine reset separate transform state to identity/zero; they do not
normalize or otherwise alter the camera basis. The shared builder retains the
unscaled basis for frustum planes and the scaled matrix for box projection.
Clip enable, projection clamp and depth offset are explicit caller inputs.

`verify_visibility_camera.py` runs original `547150` followed by full `515d00`
against the composed PC/NXDK camera and projection path in 256 cases. All camera
fields, planes, retained slots and screen rectangles match for translated and
rotated inputs, two viewport sizes, multiple FOVs and flat/perspective/far modes.
All 256 selected boxes project successfully. Only graphics-state call `50ce40`
is intercepted. This supplies a coherent camera input to the portal pipeline;
native renderer wiring, portal cache lifecycle and live gameplay remain open.

### Lazy portal projection connected to traversal

`rf_visibility_traverse_projected` now connects camera classification and box
projection to the existing bounded room walk. After the original signed-depth
test, an invalid portal is classified by `507ba0` and `518750`, then projected
by `515d00` when necessary. Repeated encounters reuse the cached rejection and
rectangle. Unencountered portals remain untouched. `rf_visibility_portals_begin_view`
reproduces `4d4c20`, clearing validity only; the original reset iterates the
world's portal vector at +0xb4. Call this once per view, separately from room
bookkeeping and from each root walk.

The near-camera full-screen path uses render dimensions returned by `50c640`
and `50c650` (globals `17c7bc4`/`17c7bc8`), independently of the viewport extent
and offset used by projection. The harness initially left these render globals
zero; tracing the discrepancy established the separate state. The final fixture
deliberately uses 1920x1080 render dimensions with smaller, offset viewports.

`tools/verify_visibility_projected.py` executes full original camera setup,
recursive traversal, classification, clipped box projection and cache reset.
Only the camera graphics-state call is intercepted. Across 128 cyclic graph
fixtures and 384 walks, PC and NXDK match room eligibility, visited/depth fields,
rectangle unions, order, portal validity, rejection and rectangle bytes. A second
walk after camera movement but without cache reset invokes no original portal
classification/projection calls and retains identical results. Resetting the
cache changes results in 91 fixtures. There are 708 untouched portal observations;
the original makes 781 expanded-box, 697 plane-test and 301 box-projection calls.

The new path allocates nothing and retains the 7196-byte traversal scratch array.
Its caller owns the portal results (28 bytes each) and bounds/validity records
(28 bytes each). These are not yet included in a live level residency measurement.
Malformed-input failures preserve the individual cache being calculated but may
follow earlier room visits; the original has no equivalent validation contract.
Authored room predicates and room-set mapping still need connection before the
live renderer and preceding-frame particle eligibility can use this path.

### Owned initial level visibility

`rf_level_visibility_open` now owns room eligibility, traversal state, initial
primary ordering, portal adjacency, caches and fixed traversal scratch. No source
geometry pointers escape. The level geometry can close before view processing.
Exact requested allocation budgets include the owner and scratch: L1S1 is 13084
bytes, and the largest installed level is 37212 bytes. Allocator overhead and
other live level systems remain outside those figures.

The file's room byte +28 is passed through `4f0300` into runtime room byte +1.
The file's byte +34 is passed through `4ce110` into runtime byte +0, removing
nonzero/detail rooms from the primary list. Traversal rejects either nonzero
runtime byte. **The separate recursion-stop byte at runtime +0x40 is not the
file detail flag**: the constructor sets it to zero. The compact traversal field
named `detail` represents that separate stop and is initialized to zero here.
All-room reset uses world +0x90; per-view reset and fallback iteration use the
ordered primary vector at +0x9c. The new owner resets only those primary rooms'
visited/depth fields per view and retains eligibility across views until the
next render reset. Missing start room, <=1 primary room or disabled portal mode
uses the original all-primary fallback. Runtime changes to room lists/flags
are not reconstructed by rereading initial file data.

`tools/verify_level_visibility.py` independently reads flags and portal records
from all 94 installed payloads, then compares 282 views against full original
`547150`, `4d4760` and `4d2f80` executions. The original skip setter also runs.
Only the camera graphics-state call is intercepted. Owned PC results and NXDK
view-wrapper results match room state, visible order and portal cache bytes,
including two views accumulating eligibility and a subsequent missing-start
fallback. PC exact-budget/one-byte-short checks, post-input-close use and repeated
cleanup pass. NXDK uses borrowed fixture storage for these replays; native owner
allocation and residency still need XEMU validation.

The owner deliberately initializes visibility fields to zero before the first
render. The original room constructor does not assign those fields, so these
fixtures supply coherent zero initial fields rather than claiming the original
startup sequence has been proved. Live frame-loop integration remains open.
This tracing also exposes a separate collision-loader follow-up: its constructor
default `skip=0` must be replaced by the authored +28 flag (tracked in TO-DO.MD).

### Moving-camera scene connection

The shared PC/Xbox actor-follow scene now loads the owned visibility state under
a 64 KiB cap, calculates camera-room membership and performs render reset/view
traversal inside `actor_follow_view`. It uses the same position and unscaled basis
as world projection. The preview's fixed 4:3 projection is represented by an exact
unit FOV factor, 640x480 dimensions and 1000-unit far distance. These are diagnostic
renderer settings, not a claim of final original gameplay camera policy.

The state persists between views and is released on all streaming exit paths.
The existing scene commits physics after presentation for the next iteration;
visibility therefore describes the rendered camera for subsequent simulation.
The original full frame schedule and two emitter passes still need connection.
No face filtering or particle emission is enabled by this change.

`rf_scene_visibility_summary` and the 64-row `rf_scene_visibility_frames` ring
expose owner bytes, counts, camera room, projected-cache count, eligibility hash
and the exact camera for PC logs and native QMP memory inspection. A 664-frame
PC follow replay completes; its final 64 camera frames match original `547150`
and `4d4760` room counts, cache counts and eligibility hashes. The recorded camera
also matches the world-render camera telemetry. Reproduce with
`verify_level_visibility.py --scene-log artifacts/scene-visibility-follow.txt`.

The collision follow-up above is now implemented: loaded world skip flags use
the authored +28 byte. All 94 collision-world ownership/budget/ray replays pass.

Native moving-camera visibility now passes on stock 64 MiB XEMU. The initial
`-display none` runs halted before the game entry marker, with EIP `8001d1ea`,
HLT=1 and CR2=`fff0007b`. The extended kernel stack contains "Possible deadlock,
blocked for more than 2 seconds." and all five entry/FPU markers remain zero.
Changing only the display backend to `xemu`, still requesting a hidden window,
allows the same XBE to boot and finish. This is consistent with the preexisting
no-display limitation recorded in INPUT.md; swapping the HDD did not resolve it.
The smoke tool now excludes `none` and retains early kernel-failure detection
and stack collection. `--no-capture` controls screenshots independently.

`artifacts/xemu/20260910-122743-109871/report.json` reports PASS with 67108864
bytes base RAM and zero added RAM. The 664-frame scripted first-person turn
fixture matches PC, including the final 64 camera/visibility records and
summary `[664,13084,54,29,2,53]`: frame count, owned visibility bytes, rooms,
portals, final visible count and camera room. All five native FPU markers are
`0x027f`. This proves native owner construction and moving-view execution;
13084 remains requested allocation accounting, not isolated physical-page
overhead measurement. It does not prove particle emission or a full campaign.
No screenshots were captured. The owned emulator was reaped and temporary
disc flags and the ISO were restored after the test.

### Static level particle frame connection

The shared actor-follow scene now retains the level particle owner under a
512 KiB requested-allocation cap and consumes the preceding rendered view's
room eligibility. Each simulation interval runs the first file-order emitter
pass, existing physics/events, particle list simulation and bounds finalization,
then the second emitter pass. The list order follows 496480; global pool 1 is
not independently stepped. The wrappers allocate nothing per tick and reject
unsupported particle physics rather than silently omitting it.

This fixture uses an explicit random seed of 1 and a diagnostic 60 Hz clock.
Initial emitters use owner handle zero, which the reconstructed registry never
allocates. A resolved-object callback is available for nonnegative handles, but
live campaign ownership, event-driven emitter changes and interleaving with
the campaign's shared random state remain unconnected. The render-then-physics
preview schedule is not proof of the complete original gameplay frame loop.

The ordinary turn replay sees no emitter room and creates zero particles.
RF_PARTICLE_VIEW on PC, or particle-view.flag on the Xbox disc, places the
inspection camera at the first authored emitter for the first 400 frames, then
returns to the normal camera. This exercises real camera-derived eligibility,
creation, movement and recycling without forcing room visibility flags.

Native report artifacts/xemu/20260910-123902-640828/report.json passes on stock
64 MiB XEMU. Its summary [663,2,238932,28,18,0,10,1595837771] records simulation
ticks, emitter count, requested resident bytes, created, expired, two live pool
counts and final random state. The summary and final 64 tick records, including
active particle record hashes, match PC exactly. Existing camera/visibility
and scene checks also pass. This verifies static-level simulation and requested
owner residency, not a complete campaign or physical allocator overhead.

Particles are not submitted to either renderer yet. No screenshot was captured.
The smoke tool's --particle-view option requires the corresponding disc flag;
the test restored temporary flags and regenerated the normal ISO afterward.

### World billboard submission and scene queue boundary

rf_particle_world_billboard composes the original 555ac0 path from world
position through 518bf0 transform, 5477a0 center projection acceptance, 555230
quad preparation and 5587c0 clipping/submission. It borrows the resolved camera
used for portal projection. The center acceptance step precedes corner clipping;
calling only the existing corner helpers would miss that decision. Perspective
and flat views, radius/aspect, rotation and projection bias retain their existing
verified arithmetic. Errors preserve the output and no allocation occurs.

verify_particle_world_billboard.py executes full original 555ac0 with only bitmap
dimensions and the final 551900 GPU submission intercepted. All 1024 cases match
PC and NXDK polygon bytes: 458 rejected, 543 quads, 13 triangles, nine five-vertex
polygons and one six-vertex polygon. Cases vary translated/rotated cameras,
perspective/flat mode, center clamping, clipping and far eligibility. Both builds
and the six CTest checks pass. This is compiled-code emulation evidence, not
a new native XEMU framebuffer test.

Draw-queue tracing identifies a separate integration requirement: 4d3ab0 builds
a room's mixed object queue, then calls 4967a0 for global/detached particles and
497c20 for emitters. 4d3560 performs sphere rejection and records callbacks in
a shared 2048-entry queue. Emitters are queued as groups; 497bf0 invokes 494b90
for each particle in the emitter's linked-list order. Queue sorting/dispatch
and composition with other transparent scene objects still require recovery
and verification. Raw decompilation establishes investigation targets, not
verified queue behavior. Velocity-stretched particles also use a separate path.
No live particle draw has been added yet and no screenshot was captured.
