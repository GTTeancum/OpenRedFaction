# Authored force regions

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
