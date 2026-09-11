# Authored force regions

Current campaign status: the owned local-player path queries and applies force
regions before controller propagation/support refresh and physics. Ordinary
carry, replacement/fall/cap, turbulence and camera shake use the verified shared
functions. Turbulence and rendered camera effects consume the particle runtime's
owned RNG stream. This is integrated reconstructed behavior, not proof of the
original game's entire frame trajectory or global object/RNG scheduling.

The v180 section `0x1100` reader follows original PC function `462f60`
(RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`).
Each record contains UID, name, position, disk-order matrix, label, one header
byte, shape, extent, strength and flags. Shape 1 stores one radius; shapes 2
and 3 store three size components. Names and the header byte are retained
without assigning runtime behavior. Original construction discards them.

`rf_level_force_next` commits the cursor and output only after a complete
record, and requires exact section exhaustion. Unknown shapes, nonfinite
floats and truncated data fail. Strings are limited to 255 non-NUL bytes.
`rf_level_owned_forces_open` copies records in authored order into one
budgeted allocation, independent of the source archive after return.

`inspect_force_regions.py` inventories 142 records across 27 levels.
`verify_force_reader.py` compares every C record byte with that disk inventory,
checks each record truncated by one byte, exact budgets and archive-independent
lifetime. Maximum retained allocation on the tested PC build is 10,812 bytes,
including the owner and excluding allocator overhead. NXDK compiles and links
the reader; this does not establish native Xbox execution of its ownership API.

Runtime selection was independently verified by `verify_force_region_select.py`.
Force application and alternate airborne speed-cap ownership remain to be
connected and verified. The reader
comparison is not execution of the original parser or a gameplay fidelity test.

`rf_physics_force_region_build` now converts an authored record to the 108-byte
runtime layout. It rotates the disk matrix rows into runtime order, computes
bounds and squared radius, preserves flags/strength and sets activation to one.
The apparent vector operation at `40a3f0` is negation, confirmed by disassembly;
the oriented-box path passes negative and positive half extents to `539a40`.

`verify_force_build.py` executes original prepared construction blocks
`46306a`, `4630d8` and `463167` through `4631b1`, retaining their vector and
bounds callees. All 108 output bytes match PC and NXDK for all 142 authored
records (1 sphere, 2 axis boxes, 139 oriented boxes). File reads and prior field
writes are supplied at the boundary; this is not a complete loader execution.
World registration and force application remain open.

`rf_physics_force_region_influence` reconstructs direction and strength from
`486949..4869f6`. Displacement uses body physics position `+e4`, whereas the
earlier selector uses public position `+3c`. Flag `10` normalizes displacement;
otherwise direction is runtime matrix row 2. Flag `8` scales by squared
distance divided by region radius squared; flag `4` uses one minus that ratio,
and `8` wins when both are set. When flags `&3` are zero, a factor below one
from **body radius (`+180`) squared / mass (`+98`)** further scales strength.
This is not region radius divided by mass. Negative strengths and factors
beyond the selected region are not clamped.

`verify_force_influence.py` executes the original block and all vector callees
for 4,096 cases covering every low-five-bit flag combination. All direction
and strength bytes match PC/NXDK. The shared API rejects nonfinite results;
in particular it reports the radial-at-center singularity rather than choosing
an invented direction. PC failure tests check unchanged output for zero radial
distance, zero falloff denominator and invalid mass. Eligibility, rotation,
velocity application, falling transitions and alternate-cap ownership remain
outside this API and are not established by these checks.

`rf_physics_force_actor_carry` implements the non-`40` actor branch
`486b73..486c1c`. Its destination is cached support velocity at `+8a0`, not
actor velocity at `+144`. Modes 1/2 suppress the incoming direction's Y
component; the existing carry Y remains. The scaled direction is added without
dt. Modes 3/8, or class kind 1 with attachment field `+1380 == -1`, clamp the
result's length to strength using the original signed comparison and scale.
The body dirty flag `80000000` is set. Negative strength is not silently made
positive; nonfinite results fail transactionally.

`verify_force_carry.py` checks 2,048 original executions, retaining the actual
mode/class predicates and vector callees. It checks the entire actor image:
only `+8a0..+8ab` and the dirty bit change. PC/NXDK results match exactly across
modes 0..9, class kinds 0..2, attachment presence and signed strengths. The
original actor type is supplied as type 0 and the class pointer is prepared;
selection, eligibility, rotation and the `40` replacement-velocity path remain
outside this test. Campaign integration still requires these surrounding steps
and their correct placement relative to support refresh and physics.

`rf_physics_force_air_cap` reconstructs `486b1c..486b6a`. It stores class
horizontal speed unless the replacement velocity's horizontal norm is greater;
in that case it stores norm plus one. It sets body flag `200000` and preserves
velocity. `verify_force_cap.py` checks 2,048 original/PC/NXDK cases, including
exact equality, differing vertical speed and an already-set flag. Whole-actor
comparison permits only the cap at `+1488` and that flag to change.

The replacement branch order is:
write direction times strength to `+144`, call fall transition `4281a0`, play
the first-entry sound through `48a930` if `200000` was clear, then update the
cap. Fall's alternate descriptor predicate `40a270` reads class flags `+724`
bit `400`; descriptor 8 is selected when set, otherwise 3, subject to the
existing descriptor-enabled fallback. The sound call supplies slot `0x53`
and physics position; `48a930` chooses local or spatial playback. These call
details are now covered by the shared composition check described below.

`rf_player_force_replace` composes replacement velocity, fall descriptor and
identity orientation, first-entry sound callback, cap and dirty flag. It
preflights finite results before changing state. The callback observes the
committed velocity/fall state with the old cap and without newly setting the
force/dirty bits; existing bits are preserved. It must not mutate actor state.

`verify_force_replace.py` executes `486ab0..486c1c`, including real `4281a0`,
`40a270`, descriptor lookup and cap code. Only the `48a930` audio boundary is
supplied. All 512 cases match PC/NXDK final state, callback count and observed
state. Original callback arguments are checked for position, slot 0x53,
count zero, volume one and pan zero. Whole-actor comparisons allow only
velocity, physics flags, descriptor/orientation and alternate cap changes.
Disabled low descriptor bytes select descriptor zero. This verifies composition,
not actual audio playback or the surrounding query/eligibility/rotation paths.

`rf_physics_force_eligible` reconstructs the eligibility gates with resolved
query and registry predicates. Body flag `8` is required and a region must be
selected. Region flag `20` rejects objects identified by `48aaf0`: object flag
`8`, or a player-list actor whose `+200` handle references the tested object.
Region flag `2` requires actor mode 1 or 5; if actor lookup fails, body flags
`18000000` instead permit the object. Without region flag `2`, neither that
mode restriction nor the nonactor body-mask restriction applies.

`verify_force_eligible.py` executes the original prefix from `4868c0` through
success at `486940` or rejection at `486c1c`. It retains the real region query,
sphere containment, generation-checked handle lookup, actor type/mode checks
and circular player-list traversal. All 4,320 PC/NXDK decisions agree, with
752 eligible cases and no actor mutations. The shared function consumes the
resolved predicates; this does not yet implement player-list ownership or
campaign force-region registration. Rotation and integrated application remain.

The previously described rotation branch is **force turbulence**: it randomizes
the force direction, not the actor orientation. Sixteen authored regions enable
it, including two in L1S2. `rf_physics_force_turbulence` reads the nibble at
bits 16..19, computes `abs((nibble / (150 / dt)) * strength)` and stores that
amplitude as binary32. It clamps `1 - amplitude` to [-1,1] for the existing
original-oriented cone sampler. The unclamped amplitude is retained for the
later player-view shake call. An enabled nibble consumes two shared CRT draws
even at zero dt or strength; a zero nibble consumes none.

`verify_force_turbulence.py` checks 2,048 original/PC/NXDK cases through
`4869f6..486a72`, retaining clamp, cone, basis, vector rotation and CRT random
code. Only the thread-data pointer is supplied. The test uses explicit 53-bit
x87 precision and checks all direction, strength, RNG and amplitude bytes.
Cases cover all nibble values, zero dt/strength, signed strength, clamp limits
and vertical/nonunit axes. This reuses `rf_particle_cone_oriented` rather than
creating a second approximation. View shake after `486a72`, shared campaign
RNG ordering and integrated force application remain open.

`rf_physics_forces_open` owns compact runtime records directly from the bounded
reader, without retaining name strings or a second authored-record array. It
keeps authored order, uses one budgeted allocation and returns an empty
collection for an absent section. Failures release temporary storage and
preserve the destination; close is repeatable. `verify_force_owner.py` checks
all 142 records across 27 levels against the original-verified constructor,
including exact/short budgets, truncated-section rollback and use after archive
closure. The largest retained collection occupies 1,956 bytes including its
owner, excluding allocator overhead and stack scratch.

Campaign load/exit now opens/closes this owner under a 64 KiB budget.
`CAMPAIGN_FORCES` records count, retained bytes and ordered runtime-record hash;
the native replay checker compares all three with PC. This establishes owned
level data, not live force application: per-frame queries, view shake and
application scheduling are still to be connected.

Native run `replay-20260910-205549` passes the 360-frame staged L1S2 lift replay
on stock 64 MiB XEMU. Guest `CAMPAIGN_FORCES` is `[5, 552, 2519048547]`,
matching PC. The existing actor/controller checks also pass. This confirms
native loading/construction/retention of all five records in that level; it
still does not claim they influence gameplay or cover native level transitions.

`rf_camera_effect_start` supplies the missing resolved-actor `40e0b0` setter:
strength at `+8b4`, duration at `+8b8`, and deadline at `+8bc` after truncating
duration times 1000 to milliseconds. Force turbulence supplies duration 0.05.
`verify_camera_start.py` executes the complete original setter with actual
registry lookup, float-to-integer conversion and timer calls. All 1,024 PC/NXDK
cases agree, including signed duration, fractional milliseconds and timer wrap;
whole-actor comparisons permit only those three fields to change. The existing
camera-effect application and shared-random adapter can consume this state.

Frame placement is established by `487a40` (export `487c33`) and `487cf0`
(export `487d43`). The object gameplay pass calls actor logic `41daf0`, then
force application `4868c0`. After all objects, `46bbe0` propagates controllers,
`41e370` refreshes support velocity, and `487770` executes movement physics.
Thus actor gameplay update must not be confused with the later physics pass:
force application belongs BEFORE controller propagation/support refresh and
physics, not after `actor_tick`. The later `487e00` pass performs support work.
Campaign integration must retain that order and share RNG consumption with
particles/camera effects. These surrounding integrations remain open.

## Campaign connection

The local player's public pose selects the first enabled region; physics position,
body radius and mass determine influence. Eligibility uses the registered type-0
player fixture with object flag8 and no parent. Authored class flags/use-kind
feed falling and carry predicates. Ordinary carry precedes the existing support
refresh; replacement stores its alternate cap for subsequent airborne steering.
The force's 0.05-second shake activates the existing camera effect, applied to
the rendered view without changing aim/body orientation. Other actor and dynamic
object force ownership is still open.

Levels containing replacement regions preload global sound slot0x53 while audio
archive access is available. First entry takes the owned first-person local
playback route through the existing diagnostic audio backend. Its unity gains
remain diagnostic policy, not full original sound-volume/residency parity.

`replay_force_region.py` stages the player at authored L1S2 UID3705 plus a small
X offset, with zero command input for120 frames. This is explicit fixture staging,
not an authored player start. It records four eligible carry/turbulence/shake
activations before the player leaves the volume. Native stock64MiB XEMU run
`replay-20260910-210520` passes with matching PC actor/controller/RNG evidence and
`FORCE_TICKS [119,4,4,4,0,4,4,0,3705,3250303071,0,0]`. The existing360-frame PC
lift carry regression also passes. No new screenshot is claimed: this check is
about physical/RNG integration, not a new rendered asset.

The native checker supports `--level L1S2.rfl --force-uid 3705`, saving/restoring
the dedicated staging file. PC uses `RF_REPLAY_FORCE_UID`. Native replacement
force/sound and repeated-entry coverage, other object types, event-driven force
activation and complete original-frame/RNG scheduling remain open.

## Native replacement fixture

`replay_force_replace.py` uses authored ctf01 UID11113 through the existing
single-player actor runtime. The map is a physics fixture; this does not add
multiplayer gameplay. Checkpoints verify vertical launch, the alternate cap,
and one sound request during seven consecutive force applications. The extended
`--cycle` replay checks a no-force interval followed by return into the region
while still airborne. The entry flag remains set and sound does not repeat;
grounded re-entry/flag clearing is not covered by this fixture.

Native stock64MiB XEMU run `replay-20260910-210958` passes120 frames with
`FORCE_TICKS [119,7,7,0,7,0,0,1,11113,0,1086324736,0]`, matching PC. The force
collection contains12 records in1,308 bytes. With APU output enabled the guest
DSP snapshot contains4,084 nonzero samples in8,192 bytes. That confirms device
output data, not host audibility or an isolated linear sound recording.

The first native attempt exposed an Xbox diagnostic-loader assumption: ctf01
omits entity section0x30000. `logic_storage_open` now accepts that absent
optional section as an empty owned collection with budget accounting, matching
the campaign's separately created player. Malformed or other errors still fail.

Native run `replay-20260910-211137` also passes360 frames, matching PC at ten
replacement applications across later airborne returns and only one sound
request. The cap remains6 and the entry flag remains active at the sampled
checkpoints. This closes native airborne re-entry coverage, not grounded return.


## Linked force activation

Original handlers `4b9330` (on) and `4ba130` (off) iterate the authored
UID array at event +0x29c. Each calls `45d6d0`, which returns the first
matching UID in the ordered force collection; missing UIDs do nothing.
Only byte +0x68 changes. The remaining three activation storage bytes
and all other region bytes are preserved, even for duplicate links/UIDs.

`rf_physics_forces_set_state` reconstructs this mutation with upfront
argument validation. `python tools/verify_force_state.py` executes both
complete original handlers with their actual list and lookup callees,
then compares all region bytes against PC and NXDK in 1,024 cases.
Coverage includes empty lists, duplicate/missing links, zero and
UINT32_MAX IDs, and arbitrary activation padding. Both builds pass.
This verifies the mutation primitive; campaign event dispatch, delayed
actions, and native XEMU scripted activation still require integration.


### Campaign event connection

The verifier now enters original common on/off dispatch at `4b9070` /
`4b9f80` with event type51, establishing the dispatch to the reconstructed
handlers in all 1,024 cases. Campaign startup, trigger/event activation,
recursive propagation and delayed ticking now receive the force owner
explicitly. Type51 actions use authored UID links, independently of object
handle resolution. A missing force owner reports unsupported immediate
actions and leaves delayed actions pending. An empty owner is valid.

`rf_event_probe --force-events` covers all four delayed mode values,
pre-deadline preservation, absent-owner pending state, missing and duplicate
UIDs, first-match preservation, live scheduling and immediate startup.
This is a shared-runtime integration test, not original full-campaign
trajectory proof. Existing Particle_State integration and all six prior
CTests pass; the new force test is also registered with CTest. Full PC
and NXDK builds pass, and the 120-frame L1S2 force replay is unchanged.
Authored scripted sequences still need native64MiB XEMU validation.
