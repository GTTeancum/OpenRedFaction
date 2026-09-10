# Explosion recipe reconstruction

Original RF.exe SHA256: b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

`tools/verify_explosion_defaults.py` executes original `48dd90` with parser,
string-copy and emitter-name lookup return values supplied. Its 256 cases cover
0, 1, 2 and 6 central emitters and all combinations of six optional fields.
This verifies loader writes and branches, not the original parser or live effects.

The original definition has stride 0x98 at 75e520; active definition index is
75ec44. The six central handles start at +2c, process bytes at +44, minimum-size
floats at +4c, play-time factors at +64 and central count at +7c. Process bytes
are stored without an additional boolean normalization. Absent minimum size is
zero; absent play-time factor is FLT_MAX (0x7f7fffff).

The central random-position factor at +90 is a single shared float. Each central
entry overwrites it, including writing zero when its optional field is absent.
With no central entries, it is untouched. It must not become an independent
per-emitter value in the reconstruction.

Sparks handle +80 defaults to -1 and count +84 to zero. A present sparks block
resolves the emitter and reads its required integer count. Head handle +24 and
tail handle +28 default to -1. Head time +8c and random-position factor +94 are
untouched when the head is absent; with a head, time is required and absent
random-position factor becomes zero. Explosion play time is at +88.

Missing central emitter resolution branches to the fatal diagnostic at 48dff5;
that branch is not executed by the verifier. Optional sparks/head/tail lookup
failure behavior and the general emitter lookup remain separate recovery work.
Six slots follow from record offsets, not a verified runtime overflow guard.

The authored charge_explode vclip selects rocket hit, whose central emitters
are ordered: explosion main part 3, explosion boom, flamethrower fire_large,
explosion flare, explosion main_smoke center, flamethrower_explode_large. Its
sparks emitter is explosion random bits 2, with count 20. The bounded recipe reader now retains this metadata. Resolved emitter
ownership is implemented; live execution remains unimplemented.

The shared recipe reader is verified against all nine authored recipes on PC
and compiled NXDK. Optional absent fields are zeroed only in owned metadata
and marked absent; they are not asserted to be original runtime defaults.
The reader rejects a seventh central emitter and preserves output on errors.

Resolved metadata owns nine fixed emitter slots: central 0..5, sparks 6, head 7,
and tail 8. A bit mask identifies resolved slots. Archive loading reuses one
scratch buffer and releases it before returning; no archive pointers remain.
All nine installed recipes resolve successfully. Missing central names return
an error; missing optional names remain unresolved, without creating particles.

Native 64-MiB XEMU replay 20260910-081353 hashes all nine resolved definitions
and the charge_explode vclip against PC. Original emitter lookup 497550, its
string helper 5001d0 and CRT comparator pass 146 cases. The optional recipe
reference explosion random bits 2 is absent from installed emitters.tbl and
returns -1 in the original lookup. Rocket hit therefore resolves central slots
0..5 only (mask 63), not its sparks slot. The initial harness expectation of
mask 127 failed and was corrected after this original-code verification.

Creation entry 48e640 is called from the code_explode path in 4c16e0.
The span 48e7c5..48e879 skips central emitters below their minimum size
(including unordered comparisons), and scales min/max velocity, min/max
radius and min/max life by explosion size. It also multiplies the shared
central random-position factor by size. All these outputs match the shared
owned-copy helper in 600 original/PC/NXDK fixtures. This does not yet create
emitters, sample positions or execute the explosion update loop.

Update function 48e290 adds frame delta 5a4014 to elapsed +20 before work.
Central slot processing requires a live pointer, process byte exactly 1, and
elapsed strictly below play_factor * size. Recipe expiry uses elapsed strictly
greater than play_time, after central processing, and releases all live central
emitters. Shared central-only timing matches 700 original update fixtures
(164 expirations) with process/release callbacks intercepted without mutation.
Trail updates, callback mutation, active-list ownership and scene phase remain
outside this helper; callers must process selected slots before releasing them.

Emitter creation 497ca0 calls 497020; the central update calls 4972f0.
Alternating phase duration 496f60 chooses on/off timing by nonzero enabled
byte, consumes one draw through 504db0/57312d, and computes
(base - variance) + 2 * (draw / 32768) * variance before rounding to float
and clamping to 0.1f. Duration and RNG state match in 1140 PC/NXDK cases
against these unchanged original functions with only the CRT thread pointer
supplied. This helper does not yet construct or process an emitter.

Emitter update 4972f0 first checks the low byte of global enable at 59fd1c.
With alternation flag 0x20, it adds frame delta, toggles when elapsed reaches
or exceeds duration, discards all overshoot and calculates one new duration.
Only the enabled low byte is changed; a single update never toggles twice.
Enabled continuous emitters (flag 4) bypass the timer query. Other enabled
emitters request emission only when the timer query returns a nonzero low
byte. Disabled emitters neither query the timer nor request emission.

The shared clock/actions helper matches 2560 original/PC/NXDK cases with
592 toggles and 609 emission requests. Fixtures cross global enable, flags,
enabled byte, delta and timer result independently, with varying clock bounds.
The original phase-duration and RNG routines execute unchanged. The timer
result is supplied, emission is intercepted without mutation, and parent
lookup returns null. Phase calculation precedes timer query and emission;
the original parent lookup follows them. This verifies decisions, state and
RNG advancement, not live particle execution. Parent attachment, emission
callback effects and spawn-timer reset must be integrated separately.

Particle allocation 496840 has now been executed in 320 recovery fixtures:
two pool indices, emitter/global ownership, room present/absent, five flag
patterns, free/empty pools and four RNG seeds. Only the CRT thread pointer
is supplied; vector copies and random-range callees execute unchanged.
The 0x78-byte record and a four-byte adjacent guard are checked.
This is original-code evidence, not a PC/NXDK particle implementation test.

Each pool descriptor is 0xfc bytes apart. Its free sentinel is 7a3b08 +
index*0xfc; global active sentinel is 7a3b80 + index*0xfc; allocated count
is 7a3bf8 + index*0xfc. A successful creation removes the first free node
and appends it to either the global active list or emitter +b0/+b4 list.
An empty pool leaves the output pointer, count and RNG unchanged; it does
not evict a particle or allocate heap storage in this function. Pool capacity
and recycling remain to be recovered separately.

Recovered record offsets (hex): links 00/04, supplied owner handle 08,
position 0c, velocity 18, age 24 (zero), color 28, destination color 2c,
second initial-color copy 30, lifetime 34, radius 38, growth 3c,
acceleration 40, gravity 44 (authored scale times float 9.8), bitmap 48,
frame count low word 4c, secondary flags low word 4e, pool byte 50,
orientation 54, flags 58, finish-VBM age 5c, copied parameter 60,
room 64, emitter pointer 68, previous-position copy 6c.
Parameter +48, copied to record +60, still needs semantic identification.
Record bytes 51..53 retain their prior contents; bytes 78..7b are an adjacent
guard, not part of the record (confirmed by initialization stride).

Creation ORs active flag 1 and clears collision flag 0x10 when room is null.
Random-orientation flag 0x200 consumes exactly one draw and chooses angle
in [0, float(2*pi)); otherwise angle is zero without a draw. Successful
creation increments the selected pool count and writes an optional output
pointer. Runtime pool integration, release/update logic and rendering remain
open; do not infer a complete live effects system from this recovery test.

Shared C rf_particle_initialize now implements the record-initialization
portion of 496840 using a 76-byte spawn packet and 120-byte particle record.
The verifier compares every output byte and RNG state against all 160
successful original allocations on PC and compiled NXDK, with varying
finite position, velocity and scalar fields. Caller-provided list links and
untouched record bytes are preserved. Three invalid pool indices additionally
verify rejection without changing the particle or RNG on both targets.
Handles are explicitly 32-bit caller-owned identifiers; they are not host
pointers. No pool allocation/linking or resource resolution occurs inside
this initializer. The caller must obtain a free node before initializing it;
recycling, simulation and scene rendering remain open.

Pool initialization 494e70 establishes a 0x78-byte record stride, correcting
our earlier 0x7c-byte candidate extent. Both compiled record layouts and
fixture sizes are corrected; the earlier extra word was an adjacent guard.
Original capacities are 500 (pool 0) and 1100 (pool 1), totaling 192000 bytes
of records. verify_particle_pools.py executes initialization unchanged and
checks every node's links, cleared flags/secondary word and preserved data.
Pool descriptors remain 0xfc bytes; they are not particle records.

Emitter cleanup 497230 detaches its particles to global sentinel 7bd670,
clears each emitter pointer, preserves source order and appends after existing
particles. Fifteen unchanged-original fixtures cover 0/1/2/8/31 source
particles and 0/1/3 existing destination particles. No particle flags, ages,
other payload or pool counts change. Emitter release must not recycle these
live records immediately.

Update 495120 copies current to previous position, then (for the verified
unowned path) advances age and radius before checking death. Age >= life or
radius <= zero clears flags, unlinks the particle, appends it to its pool's
free-list tail and decrements that pool's count. Thirty-six original fixtures
verify both death reasons at equality, both pools and different source/free
list lengths. Owner gating and non-expired simulation remain separate work.
Shared pool integration remains open; these original tests establish its
capacity, ordering and ownership contract before implementation.

The shared fixed pool is now implemented in particle_pool.c. Caller-owned
storage holds the original 500/1100 record capacities; no operation allocates
heap memory. List headers use 32-bit record/sentinel indices, so links do not
depend on host pointer width. Five base lists represent both free lists, both
global active lists and the detached list; caller-supplied additional headers
represent emitter ownership. This is storage representation, not an altered
particle capacity or order. Initialization deliberately zeros stale payload.

Creation removes a free head, runs the verified record initializer and appends
to the selected active list. Exhaustion preserves RNG/output and returns
RF_NOT_FOUND. Detachment appends live records to the detached list and clears
emitter ownership. Recycling clears flags, appends to the originating free
list and decrements its live count. Age/physics updates and the decision to
recycle remain the caller's responsibility. Invalid indices, emitter handles
and recycling an inactive record return RF_RANGE without mutation.

verify_particle_pool_runtime.py passes 3679 commands on PC and compiled NXDK
against original 496840, 497230 and recycling span 495615..495697. It exercises
1829 allocations/recycles, four saturation rejections, mixed emitter/global
ownership, repeated detach and invalid operations. Seven snapshots compare
all 192000 record bytes and all list headers after pointer-to-index conversion;
per-command statuses, returned indices, counts and RNG also match. The original
initializer runs on zeroed storage to match the shared initialization contract.
This verifies the pool in compiled code, not native scene simulation; emitter
execution, live updates, rendering and native memory accounting remain open.

Shared rf_particle_pool_step_free now advances unowned detached particles
through the recovered 495120 free-flight path, recycling expired records.
Previous position is copied first, then age and radius advance. Death occurs
before motion. Survivors clear flag 0x8000 and move using the old velocity;
acceleration changes normalized speed afterward, followed by Y gravity.
Zero-velocity normalization 4fabd0 returns speed 1 and direction +X, which is
preserved in this reconstruction. Color interpolation uses (age/life)^2 with
a float ratio and integer truncation. Record +30 is the current color, so its
shared name is corrected from color_initial to color_current.

The free-flight verifier executes full original 495120 with actual arithmetic
callees in 2048 fixtures, including 1083 expirations. All shared PC/NXDK record
bytes and counts match. Nine rejection cases additionally preserve state for
unsupported ownership/world features and malformed deltas. Collision, swirl,
wind and damage are explicitly unsupported by this path; scene integration
must dispatch them to their recovered implementations, not silently omit them.

The initial PC discrepancy was a harness floating-point environment error:
Unicorn's control word was zero, selecting 24-bit x87 precision. The comparison
now explicitly sets 0x027f (53-bit precision, nearest rounding, masked exceptions)
for original and NXDK machines; the related duration/creation/pool/tick checks
were rerun and pass. The temporary numeric tolerance was removed: comparison
is exact. This explicit test environment is not a measurement of the original
game or NXDK native control state. Native FP state verification remains open,
as does auditing other floating-point verifiers that initialize their own CPUs.

Native FP measurement is now available through rf_fp_control_diagnostic:
entry before CRT, main entry, before/after effect diagnostics and after scene
completion. XEMU replay 20260910-085724 measured 0x027f at all five checkpoints
without changing the control word. This confirms the explicit harness setting
for this NXDK build under the tested debug BIOS, not the original PC game's
full startup state or untested hardware configurations.

Replay 20260910-085856 additionally executes all 2057 free-flight fixtures in
the native 64-MiB guest, including nine rejection cases, using the shared pool.
The output hash matches the PC/original-verified reference. Record storage is
192000 bytes, allocated once and freed after the diagnostic. This tests stepping
and recycling in XEMU; it does not render live particles. Existing resource
lifetime and campaign replay checks also pass. The optional fixture file is
staged/restored by xemu_replay_check.py and absent from the normal disc.

Particle render entry 494b90 selects its frame in span 494baf..494c8f before
binding bitmap base+frame. rf_particle_frame_index recovers that zero-based
selection. The frame-count word is signed: counts <=1 select frame zero.
Otherwise normal mode floors age/life*count+0.5; hold-last flag (secondary 4)
uses age/(finish_age*life)*count+0.5. Loop flag 0x100 takes precedence and
uses floor((age-floor(age))*15/count+0.5), including the original division by
count. All modes clamp to [0,count-1]. Constant 589854 is float 15.

2880 fixtures execute the original span with unchanged floor/conversion and
clamp callees against PC/NXDK at explicit 0x027f x87 precision. They cover
signed counts, time boundaries and mode precedence. Six invalid-input fixtures
verify preserved outputs outside the supported finite/conversion domain.
This determines the frame but does not bind textures, manage animated frame
residency or draw billboards. Those renderer connections remain open.

Normal particle drawing routes through 515b40 -> 555ac0 -> 555230. The first
wrapper only dispatches for renderer 0x66. The second transforms/projects the
center and checks visibility before the billboard routine. Velocity-stretched
particles instead route through 515ba0 -> 558e30 and remain separate work.

rf_particle_billboard_build recovers the camera-space construction span
5552a0..555483. Sine/cosine are rounded to float. The supplied size is applied
to both dimensions, then the longer bitmap dimension expands by its aspect
ratio. Camera scales come from original globals 1818b48/1818b4c. The four
corners retain center Z, and output order follows the original polygon's
reversed local-vertex pointer array, with UVs (0,0),(1,0),(1,1),(0,1).
Intermediate rounding matches the recovered instruction sequence, including
the retained double height*cos term.

1024 fixtures execute original trigonometry, aspect and corner arithmetic
with only bitmap dimensions supplied at the resource-query seam. All PC/NXDK
corner and UV bytes match under 0x027f x87 precision. This constructs the quad
before projection, clipping, depth bias and blend state; it does not claim
finished particle rendering. A positive bitmap size is required by the shared
API; invalid dimensions and nonfinite inputs preserve output.

Point projection 5477a0 is now reconstructed as rf_particle_project. Existing
projected/rejected bits (flags &3) short-circuit without touching coordinates.
With clamping enabled, Z<=0 sets rejected bit 2. Otherwise projected bit 1 is
set, reciprocal Z is stored (FLT_MAX for zero Z), and normalized coordinates
are computed before viewport scaling/offset. The clamp control uses its low
byte; when enabled normalized X/Y are bounded to [0,2].

Depth offset global 1e652e8 replaces reciprocal Z with 1/(Z-offset) only when
offset is nonzero and 20*offset < Z. Screen X/Y still use the original 1/Z.
The X normalized intermediate is rounded to float; Y stays in double precision
until final screen conversion, matching the original instruction sequence.
648 fixtures execute the full unchanged original function and compare every
point byte against PC/NXDK at 0x027f precision, including signed/zero depths,
threshold-adjacent values and cached flags. This does not implement polygon
clipping, billboard depth bias, blend state or raster-depth conversion.

Backend inspection confirms the current shared mesh still carries only RGB
vertex color, while particles require current-color alpha and their render
modes. Those interfaces must be extended alongside the recovered render-state
behavior before inserting particle triangles into the existing scene stream.

## Emitter cone sampling

`rf_particle_cone_sample` reconstructs original 0x4fadb0 in local +Z space.
The first CRT draw selects Z uniformly from the supplied cosine minimum toward
1, storing a float. A second draw selects an azimuth using the original float
2-pi constant at 0x5894ac. The radial square root rounds to float before the
sine/cosine products. There are exactly two draws even at cosine minimum 1;
the emitter's separate branch may bypass this function for that value.

`tools/verify_particle_cone.py` executes the unchanged sampler and actual random
helpers, comparing local XYZ and RNG state to PC and compiled NXDK in 4096
cases. All bytes agree, including full-sphere and narrow-cone boundaries. Four
out-of-range/nonfinite API guards preserve output and RNG. No allocation occurs.
The comparison uses the verified 53-bit x87 environment.

`rf_particle_cone_oriented` reconstructs the full 0x4fae00 wrapper, with a basis
built by 0x4fcfa0 and transform at 0x4facb0. Both horizontal axis components
strictly inside +/-0.0001 snap the basis to positive or negative Y according to
the input Y sign; a zero axis snaps to positive Y. Otherwise only the right
vector is normalized and the up vector is the forward/right cross product.
Nonunit forward axes retain their original behavior. The transform preserves
original product/addition order and float stores, with no allocation.

`tools/verify_particle_cone_oriented.py` executes the entire unchanged wrapper,
including basis construction and actual CRT random helpers. All XYZ and RNG
bytes match PC/NXDK in 4096 cases, including exact vertical thresholds and
adjacent floats, zero and nonunit axes, and cosine endpoints. Thirteen invalid
input guards preserve output and RNG. This is compiled NXDK verification in
Unicorn under the established 53-bit x87 environment, not a new native gameplay
capture. Parent direction resolution and emitter execution remain separate.

The current
trace of 0x496c50 also identifies spawn displacement along the selected
direction, speed/radius/lifetime random draws, optional parent velocity and a
spawn-timer reset after particle allocation. Those steps still need a combined
original-executable emission replay before live campaign emitters are claimed.


## Full emitter emission reference replay

`tools/verify_particle_emission_trace.py` now executes unchanged 0x496c50
through parentless 0x496bc0, all vector/cone/RNG helpers, actual 0x496840
allocation and 0x4fa360 timer reset. Only the CRT thread-storage pointer is
supplied by the existing harness. It records 512 reference fixtures under
ignored artifacts, with 256 successful and 256 exhausted pool attempts.

The verified order is optional two-draw cone selection, speed, radius, life,
allocation (one orientation draw only if allocation succeeds and flag 0x200
is set), then delay. Direction cosine >=1 bypasses the cone entirely; values
below -1 are clamped in the emitter itself. The delay is multiplied by 1000
before truncation to integer milliseconds, without an intermediate float
store, and the timer wraps at 1072800000 with a strict greater-than check.
Exhaustion still changes the spawn packet and deadline and consumes all other
draws. Allocation arguments are pool 1, packet, room, authored-position
pointer, owner handle, null output pointer and emitter pointer. The authored
position argument is opaque here; it must not be confused with the owner.

The harness asserts call order, RNG advancement, exact allocation arguments,
record position/velocity copying, owner/emitter linkage, pool count, cosine
mutation and timer wrap. Original before/after records are retained for the
pending C integration; this is not a PC/NXDK emission parity claim. Parent
resolution, inherited velocity and parent ownership rules remain unverified
by this replay. No new campaign visual is implied.


## Shared parentless emission

`rf_particle_emitter_emit` now joins cone selection, spawn displacement,
speed/radius/lifetime draws, the shared pool allocator and timer reset.
The caller owns the 156-byte emitter state, existing fixed particle pool,
RNG and emitter list handle; the function allocates no storage. Nonnegative
owner handles are explicitly unsupported until parent resolution is recovered.
Pool exhaustion preserves the output index but commits packet, cosine, RNG
and timer updates, matching the original.

The full replay exposed a mistaken interpretation of 0x40a0b0: it computes a
dot product, not a vector length. For emitter flag 8, the rounded random speed
is multiplied by the sampled direction dotted with the resolved emitter axis,
adding products in Z/Y/X order. Negative projections are retained. Spawn
radius displacement happens before this speed scaling, along the sampled
cone direction. This matters for broad cones and nonunit directions.

`tools/verify_particle_emission.py` regenerates and compares 2048 full original
runs with PC and compiled NXDK code. All emitter fields, spawn packet, deadline,
RNG and particle bytes agree after translating pointer links to shared indices.
The suite includes 1024 successful and 1024 exhausted attempts; 1536 cases vary
position, nonunit direction, speed ranges, signed spawn radius, radius/life
ranges and delay ranges. It also checks live counts and the emitter list head.
Six CTest tests pass. NXDK comparison uses Unicorn; no native live-emitter or
campaign rendering claim follows from this result. Attached emitters and the
campaign tick/render connection remain open.


## Attached emitter emission

`rf_particle_emitter_emit_parent` extends the shared path with a caller-owned
72-byte resolved parent view. Original 0x496bc0 transforms authored position
and direction by the parent's basis, adds parent position, and normalizes the
transformed direction unless emitter flag 0x40 is set. The position and direction
stored in the emitter remain authored values. A missing parent leaves those
values untransformed; a negative owner skips lookup and ignores a supplied view.

Original 0x496c50 adds parent velocity when emitter flag 0x80 is set, even when
flag 0x40 bypasses the transform. Particle ownership selects the parent handle
for velocity inheritance, or when parent +0x34 is positive and class flag 0x40
is set. Original 0x4c90f0 bounds-checks class index and reads that flag from
0x85cf70 + index*1360; invalid class indices contribute zero flags. The caller
must supply the matching resolved object and class view, with valid lifetime.
The existing parentless API retains its explicit nonnegative-owner rejection.

`tools/verify_particle_emission_parent.py` executes 2048 full original emissions
with actual 0x40a0e0 object lookup and 0x4c90f0 class predicate, in addition to
unchanged transform, random, allocation and timer routines. Emitter state,
particle payload, RNG and deadline match PC/NXDK exactly after translating
links/handles. Coverage includes absent/ignored parents, translation, rotation,
nonunit basis, movement/velocity flag combinations, class index bounds, positive
and nonpositive lifetime fields, and pool exhaustion. All 2048 parentless cases
still agree, both builds succeed and six CTest checks pass. Degenerate transformed
directions are explicitly rejected by the finite C API. This has not connected
campaign parent lifecycles, simulation or rendering and is not a native visual
validation; compiled NXDK routines are compared through Unicorn.


## Combined emitter update

`rf_particle_emitter_update` joins the recovered 0x4972f0 sequence using a
184-byte caller-owned runtime: emission state, phase definition and phase
clock. It advances/toggles the phase, queries the spawn deadline, emits at
most once, then refreshes the emitter room from a resolved parent unless
flag 0x40 is set. The newly created particle keeps the pre-refresh room.
Global enable uses its low byte; disabling it leaves runtime/RNG unchanged.
Continuous emitters bypass the deadline decision. Exhaustion is a successful
update with an emission attempt but no created particle. The result reports
phase/timer/emission actions and the created index, or UINT32_MAX.

`tools/verify_particle_update.py` executes unchanged original 0x4972f0 with
actual phase-duration randomness, timer query, complete emission/allocation,
object lookup, class predicate and parent room helper. It compares full
runtime, particle payload, RNG and actions against PC/NXDK in 2048 cases:
393 phase changes, 489 emission attempts and 177 successful allocations.
Coverage combines low-byte enable values, alternating/continuous flags,
expired/future/disabled deadlines, parent inheritance and room refresh,
missing parents and exhausted pools. PC and NXDK builds and all six CTest
checks pass. This is still a callable shared update, not a campaign frame
integration; emitter creation, stable campaign parent views, existing-particle
simulation and rendering remain to be joined. NXDK comparisons use Unicorn.


## Emitter-linked unowned particle simulation

`rf_particle_pool_step_unowned` extends the verified free-flight path to retain
an emitter association. Original 0x495120 calls 0x4973e0, which checks whether
the emitter's owner handle at +4 is nonnegative. If so, surviving particles
expand the emitter's +0xa0 value using squared distance from center +0xa4.
This runs after movement but before acceleration/gravity. Vector subtraction
rounds components to float, then the squared sum uses the original X/Y/Z
order and is compared before the final float store. Expired particles do not
expand bounds. Negative-owner emitters leave bounds unchanged.

The caller supplies a 20-byte matching emitter bounds view. The existing
step_free API remains a wrapper that rejects emitter-linked particles without
that view. No allocation occurs. Nonnegative particle ownership and collision,
swirl, wind or damage remain explicitly unsupported; those paths cannot be
silently advanced as free-flight particles.

`tools/verify_particle_unowned_step.py` executes full original 0x495120 and its
real emitter predicate/vector helpers for 2048 cases. PC/NXDK particle records,
counts and bounds agree exactly, including 1083 expirations and 455 bounds
expansions. The 2048 detached cases and nine rejection guards still pass, as
do all six CTest checks. Both builds pass. NXDK verification uses Unicorn;
this is not a campaign simulation or visual validation.


## Emitter initialization reference

`tools/verify_particle_emitter_init_trace.py` executes full original 0x497020
with a supplied nonzero room and no traversal start point, using a parentless
owner. All normalization, immediate emission/allocation, random, timer and
phase-duration callees run unchanged. The replay records 1024 cases, including
256 immediate emission attempts, 128 allocations and 512 phase initializations.
Before/after emitter and particle records are saved under ignored artifacts.

The initializer copies only the low 16 bits of emitter flags, preserving the
upper half. It copies/normalizes authored direction, establishes the empty
emitter list and assigns the resolved room. If initially-on bit 0x10 is set,
it either emits immediately (bit 2) or draws an initial delay in milliseconds.
Only afterward does it assign the caller enabled value's low byte, preserving
upper bytes. Thus a disabled caller value does not suppress immediate emission.
Alternate bit 0x20 then clears phase elapsed time and draws phase duration using
the new enabled low byte. Without it, prior duration/elapsed remain untouched.
Without initially-on, the existing spawn deadline is retained. The replay
checks exact random advancement across these branches and pool exhaustion.

The original also leaves packet velocity/radius/lifetime untouched until an
emission occurs, preserves opaque packet +0x48 and adjacent +0x9c, and copies
template +0x80 to runtime +0x144 outside the compact update state. Shared
initialization must make these retained fields explicit rather than assume a
blanket memset is equivalent. Fresh-object defaults and room traversal still
need separate evidence. This replay does not claim shared C initializer parity
or any campaign/visual progress.


## Shared emitter initializer

`rf_particle_emitter_initialize` now consumes a typed 132-byte resolved template
and caller-owned runtime/list. It follows the verified copy, normalization,
initial-emission/delay, enabled-byte and phase sequence. Unwritten runtime fields
remain intact; opaque source_id and copied_80 are returned separately for the
future enclosing object. Bitmap and room handles are supplied externally.
The emitter list must already be empty, avoiding orphaned particles when a
caller attempts to reinitialize a live list. No heap storage is allocated.
The finite API rejects degenerate directions, unrepresentable phase ranges
and delays outside the supported timer period.

`tools/verify_particle_emitter_init.py` regenerates all 1024 full original
fixtures and compares compact runtime, opaque outputs, particle bytes, RNG,
allocation count and list head with PC and compiled NXDK. All agree, including
256 immediate attempts, 128 successful allocations and 512 phase setups.
Both builds and six CTest checks pass. The verified initialization scope is
parentless with a supplied room; caller-resolved parent support shares the
separately verified emission path but is not claimed by these init fixtures.
Fresh-object defaults, original room traversal and campaign loading remain
open. NXDK comparison uses Unicorn, not a new native visual test.


## Fresh emitter storage and construction

Original 0x496a70 invokes the CRT array constructor 0x5736fb for 128 records
of 344 bytes at 0x7b2a70 (44032 bytes). Their PE-backed static storage begins
zero-filled. Constructor 0x496fd0 calls vector, color, spawn and sentinel-member
constructors, but only timer constructor 0x4fa340 writes data: deadline +0x154
becomes -1. The other member constructors leave the existing bytes intact.

`rf_particle_emitter_fresh` creates the corresponding 184-byte compact runtime:
zero fields and deadline -1. It is only for first-use storage, with separate
pool/list initialization. Calling it on live or reused slots would lose state
that original reuse retains and is outside its contract. The original capacity
is now explicit as RF_PARTICLE_EMITTER_CAPACITY=128.

`tools/verify_particle_emitter_fresh.py` proves constructor write coverage using
four poisoned fills, then runs the actual array constructor against original
zero-filled storage and checks all 128 entries. Every compact state matches PC
and compiled NXDK. Both builds and six CTest checks pass. NXDK comparisons use
Unicorn. The free-list setup at 0x4973ff..0x497460 and allocator 0x497ca0 still
need shared lifecycle integration; allocator post-initialization work includes
bounds setup and must not be omitted when campaign emitters are introduced.


## Fixed emitter lifecycle reference

`tools/verify_emitter_pool_trace.py` executes original fixed-list setup at
0x4973ff..0x49745a after static construction, then unchanged allocator 0x497ca0
and release 0x497d80. Template parsing and atexit registration are outside the
replayed setup range. Allocation includes full initializer, immediate particle
creation and original post-initialization bounds work. Release includes actual
0x497230 particle detachment. The harness checks every emitter and detached
particle list link and both live counts after every operation.

The sequence performs 320 allocations, 192 releases and two exhausted-pool
attempts. Exhaustion returns null without advancing RNG. Allocation removes
the free head and appends the active tail; release does the reverse, appending
the slot to the free tail. Live particles move to the detached list and clear
only their emitter reference (alongside list links), retaining particle count
and payload. Emission and phase fields not overwritten by initialization survive
reuse. Nonzero callback, center and phase sentinels make that retention explicit.

After initialization the allocator zeros maximum squared distance (+0xa0) and
stores an estimated radius at +0x9c using template acceleration/gravity, maximum
life, maximum speed and maximum radius. It leaves the center (+0xa4) intact.
These observations and before/after allocation records are saved as ignored
reference fixtures. Shared fixed emitter allocation/release, bounds estimate
parity, room traversal and campaign integration remain open; this replay is
original-executable evidence, not shared or native gameplay validation.


## Shared fixed emitter pool

`rf_emitter_pool` now manages 128 caller-owned 228-byte slots (29184 bytes),
using index links for free/active lists. Particle handles map to slot index+1,
requiring 133 particle list headers. Initialization requires empty emitter lists;
allocation consumes the free head and appends the active tail, while release
detaches particles and appends the free tail without clearing retained payload.
No heap allocation occurs. Pool exhaustion preserves RNG and output index.

After the shared initializer, allocation stores original estimated radius:
0.5*(abs(gravity_scale*float(9.8))+abs(acceleration))*max_life*max_life
+abs(max_velocity)*max_life+max_radius, preserving the original operation order
and final float store. Maximum squared distance is zeroed; bounds center is
retained. Source identity and the opaque template field are stored per slot.

`tools/verify_emitter_pool.py` compares original lifecycle fixtures with PC/NXDK
for 320 allocations, 192 releases and two exhaustion attempts. It checks slot
state, all free/active links and heads, counts, radius, RNG, created particles
and detached payload. Templates vary speed, acceleration, gravity, life and
radius ranges; reused slots contain deliberate retained-state sentinels. Both
builds and six CTest checks pass. The 29184-byte slot figure excludes the
192000-byte particle array, list headers and manager structs. Native campaign
memory accounting, room traversal, frame integration and rendering remain open;
NXDK lifecycle parity is measured through Unicorn.
