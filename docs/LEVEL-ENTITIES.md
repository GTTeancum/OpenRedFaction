# Level entity records

Integration milestone (PC/XEMU verified; provisional spawn behavior): the shared scene stream now loads the selected
actor's class/material/sphere configuration through `rf_entity_physics_config_load`.
That archive adapter reads entity.tbl once and reuses one temporary allocation
for materials.tbl; its cap is the larger entry size. The 468-byte result owns
all values and survives archive closure. Movement index and optional use-kind
are now included: miner1 has run index 1 and ai-response kind 9, radius 2.
All 63 class records still match independent installed-data extraction; eight
use kinds and ten malformed fixtures are checked. The movement index is an
authored name-table index, not an initialized movement descriptor.

The animation/scene fixture constructs an owned physics body from the exact
frame-zero bone matrices used for skinning, applies class center X/Z suppression
and named sphere overrides, and retains three spheres across 64 rendered frames.
Its 396-byte body includes records; the body setup cap is 4 KiB. This is a
provisional integration: it supplies identity local inertia, zero initial
velocities and the existing scripted pose. It does not claim original spawn
timing, crouch collider switching, collision response, AI, or registry insertion.
The fixture's body remains available after streaming and is closed before the
next scene. `rf_scene_actor_physics_diagnostic` exposes status/frame count,
sphere count/storage, full body hash, sphere hash and radius bits. PC scene
checks cover all 64 frames and cancellation; XEMU compares these eight words
against the same shared PC scene, alongside existing world/door validations.
The run at `artifacts/xemu/20260909-142439-773475/report.json` passes on 64 MiB;
full guest config bytes also match PC in `actor-config-comparison.json`.
The first integrated run exposed NXDK's asserting strtod stub; bounded decimal
parsing now avoids it on both targets. See docs/XEMU-MEMORY.md for memory evidence
and limits, including the remaining provisional pose and local inertia.

`inspect_class_construction.py` executes the complete original 0x42d290 without
hooks on zero, A5 and 5A patterned class storage. Use-kind +1b4 and flags +724/+728
remain untouched, while resolved sphere count +cec is explicitly zeroed.
All 75 descriptors in the executable's initial static image have zero at the
three untouched fields. Thus absent `$Use:` starts at zero for initial static
classes; this is not a claim about reloading a mutated descriptor. Report:
`artifacts/class-construction-inspection.json`.

Further creation tracing: 0x424520 performs model/class setup through 0x4246e0
and sets class flag 0x20000000 after success; it does not select movement mode.
0x433a00 resolves the authored `$Movemode:` through the 16-name pointer table
at 0x596384 and returns the 0x4339d0 descriptor, stored at actor+858.
The separate tensor rewrite at 0x422bb0..0x422c99 is guarded by 0x42d7b0,
which tests class flags &0x401200; miner1's authored flags do not select it.
These findings narrow the remaining spawn path but do not establish the initial
locomotion state or animation cursor. Ghidra now explicitly exports these
creation helpers and the class constructor for continued tracing.

`rf_physics_body_replace_spheres` composes owned sphere copying with the recovered
bounds update for post-creation class-sphere installation. It clears/rebuilds
flag 0x2000 from positive sphere parameter_10, updates bounds and preserves all
other represented body state, including mass and both tensors. An open owner is
required. Peak budget includes body plus old and new records; the old allocation
survives validation and allocation failure. Source may point into the old list.
Final accounted storage drops to body plus the new records after replacement.
Allocator overhead and fixed stack scratch are excluded, as for body_open.

`rf_physics_probe --replace-guards` checks an exact peak budget, one-byte-short
failure, invalid later sphere, closed-body rejection, complete unrelated-state
preservation, analytic radius/bounds, clearing/setting spring flags, source
aliasing, empty replacement and repeated close. PC and NXDK builds pass. These
are shared ownership/sequence checks; the complete original 0x423bd0 replacement
sequence outside that installation block is not covered, and no runtime scene
uses this API yet.

`verify_physics_replacement.py` now executes original 0x42405d..0x424168,
including array clear/append and bounds callees with only heap allocation/free
supplied, against linked NXDK replacement. All 180 cases pass for 0..8 new
spheres and 0/1/8 old spheres, randomized body storage/flags, and positive,
zero, negative or NaN spring parameters. All 308 represented body bytes and
ordered sphere records match; unrelated original storage is checked too.
One-byte-short peak budgets preserve the owner, and 160 injected NXDK allocation
failures preserve old storage. Original clear frees before copying; the shared
API deliberately retains old storage until success to provide transactional
failure handling, accounting for that higher peak. Prior class/pose creation,
later crouch handling and XEMU execution are excluded. Report:
`artifacts/physics-replacement-verification.json`.

Integration ordering correction from the original creation path: 0x422360 clears
the physics parameter block, copies authored mass and any already-cached class
spheres, then calls the generic factory. In 0x49ec90, positive mass branches at
0x49ecb6 directly to 0x49efa4, skipping density-derived mass and tensor inversion.
For miner1 mass 100, do not call `rf_physics_spheres_prepare` unconditionally.
With no cached spheres, the subsequent empty-list path adds a fallback sphere
and calls 0x4fce70; with existing spheres it preserves the input tensor.
This distinction must be represented when binding first and subsequent actors.

Only later does creation set animation state, run controller 0x41f270 and the
1/30-second model update (0x4231f0..0x423208), then call 0x423b90 at 0x42324f.
That guard builds class spheres through 0x423bd0 once per class. It replaces the
first actor's body sphere list and recomputes bounds without repeating the
earlier density/tensor calculation. Class flags 0x24000 suppress posed center
X/Z before caching; miner1 has these flags. The previously checked arbitrary
stand/crouch poses are not proof of the actual spawn pose. Preserve this order
and resolve controller-selected initial state before claiming creation parity.

The class+0x1b4 field tested against 4 by physics creation is a use-kind:
0x489610 parses `$Use:` names vehicle=1, switch=2, command=3, turret=4,
monitor=5, medic=6 (plus two further kinds). A recognized kind also requires
the next numeric field; an unknown kind writes zero, while absent `$Use:`
leaves the supplied field unchanged. It is not a movement-mode enum. Record
its constructor default and bind the optional authored field before general
class physics initialization.

`rf_entity_class_physics_read` reads authored mass, material name and the two
flag lists without allocation, preserving output on errors. Flags use the
original name tables and ASCII-insensitive equality: original 0x57c130's
default-locale branch lowercases A..Z. The required primary list and optional
secondary list may be empty; repeated names OR the same bit. Unknown names,
duplicate fields and nonfinite mass fail. This is metadata scaffolding, not
the complete original parser or its subsequent descriptor mutations.
`verify_class_physics.py` compares all 63 installed classes using flag names
extracted independently from the fingerprinted executable, plus seven malformed
fixtures. miner1 yields mass 100, flesh, primary 0x0102411f, secondary 1.
Class initialization after parsing remains to be traced before body integration.

`rf_entity_material_read` now provides a 24-byte material record from caller-owned
materials.tbl text, with no allocation. It resolves the fixed ten-name index,
falls back to Default for an unknown name, and reads elasticity, friction,
density, buoyancy (authored spelling `$bouyancy:`), and traction. Negative
authored coefficients are retained, including water elasticity and ice buoyancy.
Missing/duplicate fields, quoted numbers and nonfinite values fail without
changing output. Bitmap prefixes, debris and hit sounds are outside this reader.
This is bounded metadata scaffolding; original parser equivalence is not claimed.
`verify_entity_materials.py` compares all ten installed records, twelve lookup
cases and six malformed fixtures including output preservation. PC probe and
NXDK builds pass. Runtime archive binding and live body use remain open.

Class physics input evidence: 0x41bbe7 stores authored mass at class+0x68;
0x41bc0b calls material-name lookup 0x4686c0 and stores its byte at +0x98.
The ten names at 0x59cb10 are Default, Rock, Metal, Flesh, Water, Lava, Solid,
Sand, Ice, Glass, so Flesh is index 3. Unknown material names return index 0.
Material coefficients are runtime data, not constants in the executable image:
0x467e68/0x467e85/0x467ea2 store elasticity/friction/density from materials.tbl
at 0x649f50/54/58 plus index*28. The installed flesh entry supplies .2, .8,
800 respectively (buoyancy .1, traction 1). Read these through the archive;
do not hardcode flesh values or depend on table declaration order, which places
Glass before Sand despite its index 9.

0x41bc2e passes 28 name pointers at 0x594598 to flag parser 0x513020;
0x41bc5a passes eight at 0x594608. The parser ORs 1 shifted by the matching
name index. For miner1's authored primary names the bits are walk=0,
fly=1, climb=2, holds_weapons=3, sentient=4, swim=8, mouselook=14,
humanoid=17, envirosuit=24, yielding 0x0102411f before later class changes.
Secondary collide_player is bit 0. The parser's comparison at 0x5130b1
calls 0x57c130; its case behavior and complete class defaults still require
verification before implementing general authored flag loading.

Named class collision-sphere overrides now use `rf_entity_sphere_overrides`.
Original 0x423e7e..0x42405d walks declarations in order, using
0x503260 -> 0x5014c0 -> 0x53c2fb to find the first ASCII-insensitive name
prefix match. Missing names do nothing; repeated declarations apply in order.
The two authored scalars always replace their respective fields, with the
selected scalar chosen by the network byte. Radius and spring constant/length
replace defaults only when radius or spring constant is positive. The original
parser at 0x41d320 identifies the optional spring fields by literal table tags;
the physical meaning of the required scalar pair remains unassigned.

`verify_entity_sphere_overrides.py` checks 270 original/PC/linked-NXDK cases,
including prefix collisions, mixed case, empty/missing names, repeated overrides,
and nonpositive/NaN optional values. It executes original lookup callees without
hooks and checks unrelated class storage remains unchanged. The bounded shared
API additionally rejects unterminated names and counts above eight before writes.

`rf_entity_sphere_declarations_read` extracts these declarations without allocation
from caller-owned table text. This is metadata scaffolding, not the complete
original parser: unrelated fields are skipped. Optional radius precedes optional
spring constant and required spring length. Finite numeric values and names of
at most 23 bytes are required. Errors preserve output. Independent installed-data
comparison covers all 151 declarations in 63 classes and six malformed fixtures,
including output preservation. miner1 supplies scalar pairs .5/.5, 1/1 and 5/2
for csphere_0/1/2, with no radius or spring replacement. Material coefficients,
authored class flag mapping and live body integration remain open; these tests
do not establish a spawned miner in XEMU.

`rf_level_owned_entities_open` now retains authored entity records in one
budgeted allocation. Each item owns the existing decoded record plus its complete
raw span, preserving fields the reader currently skips. The input level/archive
can close after success. This is input ownership, not original gameplay entity
construction: flags, relationships and other undecoded bytes must still be mapped
from original loader evidence before they drive event actions.

The byte cap includes the owner, item array and raw record bytes, excluding
allocator overhead and bounded stack scratch. Open requires an empty owner and
publishes only after two complete validated scans and raw-byte reads; failures
leave output unchanged. Source data must stay stable during open. Close frees
the one allocation and clears the owner; repeated close is safe.

`python tools/verify_level_entities.py --owned` compares every decoded field and
raw record byte for all 1,610 entities across 66 installed levels with entity
sections. The PC probe closes the archive and poisons the level before emitting
owned data, checks exact and one-byte-short budgets, rejects reopening a live
owner and checks repeatable close. All pass; maximum accounted PC allocation is
101,209 bytes. Level tests also reject all 155 truncations of the minimal record
and malformed later records without publishing an owner. The ordinary reader
comparison still passes, both PC/NXDK builds succeed and four CTest checks pass.
Report: `artifacts/owned-level-entities-verification.json`. Xbox residency and
registration of these records are the next integration steps.

The Xbox level path now opens owned entities after triggers and events, sharing
the existing 512 KiB logic-data cap. Failure releases all three owners. The
resident-data hash includes decoded entity records followed by their complete
raw bytes in authored order. Diagnostic words 10/11 expose entity count and
accounted bytes; words 4/7/8 now include entities in total bytes and hashes.
`xemu_smoke.py` derives these expected values from the PC owner export and
compares them after the normal lifetime checks. Gameplay registration and
event-target dispatch are still separate work.

Stock 64 MiB XEMU run `20260909-123707-654594` passes: 78 entities occupy
101,209 accounted bytes; combined entity/trigger/event ownership is 355,013
bytes. All decoded/raw bytes match the PC-derived hash `40a0567f` through 66
lifetime checks and archive closure. The 600-frame door render trace remains
`22d17eea`, with all 2,400 controller ticks matching PC. This validates resident
inputs alongside the diagnostic scene, not entity spawning or gameplay events.
No framebuffer was captured because rendering behavior is unchanged.

## Original entity loader mapping

Section comparison 460fcf selects 0x30000, with loader 464010 called at 461006.
The loader reads three relationship words immediately after the script name
and editor byte. Its scalar overlay writes the first to entity +0x51c, the
second (friendliness) to +0x1f8 and the low byte of the third to +0x28. Authored
UID goes to +0x20. Other meanings of these relationship fields remain unnamed.

`tools/verify_entity_loader_fields.py` executes original 4647ef..46483c with
supplied loader locals and zero FOV. Two hundred cases compare all 0x900 target
bytes on zero/A5 storage, including boundary/random relationship, friendliness
and UID words. Friendliness is copied without clamping; the third word truncates
to a byte. FOV zero produces 1.0 at +0x840. This does not execute file reads,
entity creation, nonzero FOV calculations or the rest of the loader. Report:
`artifacts/entity-loader-fields-verification.json`.

Static original-code tracing maps the second byte in the trailing 17-byte flag
block to creation flag 2, and the sixteenth to creation flag 4, using nonzero
tests. Factory 422360 maps these to object flags 0x4000 and 0x20000 respectively.
The loader applies hide 48a570 after attaching child objects if 0x4000 is set.
These factory/loader paths are traced, not yet execution-verified end to end.

The installed-data verifier now reports these source values for Live Mines:

| Event targets | Class | Initial friendliness | Creation flags |
| --- | --- | --- | --- |
| 8696, 8697 (UnHide/Goto_Player) | env_guard | 0 | 2 |
| 8678, 8324, 8326 (Set_Friendliness) | env_guard | 1 | 0 |

All five have relationship words `[1, friendliness, 0]`. This connects the
door-event targets to authored hidden/friendliness input instead of inventing
defaults. The expanded inventory still matches all 1,610 decoded records across
66 levels. Next expose the needed fields in shared runtime construction and
verify factory initialization before registering live entities.

`rf_level_entity_spawn_read` now exposes these four recovered fields from an
owned record in shared C: relationship +0x51c, friendliness, low byte +0x28 and
creation flags. The 16-byte result introduces no allocation or change to owned
record layout. It checks UID consistency, raw span boundaries, the optional
field flag and exact record exhaustion before publishing. It expects a retained
v180 record already validated by the main reader; it does not repeat transform
or string-content validation, construct entities, or infer defaults.

`python tools/verify_level_entities.py --spawn` passes all 1,610 entities across
66 levels on PC and actual NXDK-linked code under Unicorn. The PC probe runs
after archive closure, and checks one-byte-short record rejection and unchanged
output for every entity. Synthetic level tests cover all 155 truncations of a
minimal record, trailing bytes, invalid optional flag, full-word friendliness,
low-byte truncation and nonzero creation-flag bytes 1/2/255. Both builds and all
four CTest checks pass. Report: `artifacts/entity-spawn-verification.json`.

## Shared creation-flag conversion

`rf_entity_creation_object_flags` reconstructs the flag assembly at the start
of original factory 422360: descriptor +0x94 equal to 3 contributes 0x10000,
creation bit 1 contributes 8, bit 2 contributes 0x4000 and bit 4 contributes
0x20000. Other creation bits do not contribute at this stage. This is the flag
value prepared before generic allocation, not a final initialized entity.

`tools/verify_entity_creation_flags.py` executes the original entry through
422477 without intercepted helpers, using a valid class and absent player
index. All 1,152 cases match the shared function on PC and actual NXDK-linked
code: six descriptor kinds, 64 creation words including upper bits, and network
bytes 0/1/2. Network byte exactly 1 adds creation bit 8 earlier in the factory,
but that bit does not change this object-flag assembly. Player-index-dependent
descriptor substitution, invalid class handling and later initialization are
not covered. Existing entity predicate checks remain green (2,007 cases and
one cycle rejection). Report: `artifacts/entity-creation-flags-verification.json`.

## Original constructor and allocation

Type 0 in allocator 487100 selects allocation size 0x1494 (5,268 bytes) and
constructor 40e380. `tools/verify_entity_construction.py` executes that complete
constructor and all its callees on zero, A5 and 5A storage. It also executes
the complete type-0 allocator path with only heap boundary 573619 supplied,
covering allocation success/failure and count/high-water combinations. Three
constructor cases and twelve allocator cases pass full object-byte comparisons.

The constructor initializes selected string/container and timer subobjects;
it does not zero the full entity. The report records all zero and inactive-timer
write ranges. In particular, UID, type, handle, positions, object flags and
friendliness retain patterned input at this stage. An empty pointer array at
+0x1418 is initialized, but the attachment head +0x268 is cleared afterward by
the allocator. Do not turn constructor-no-op vector/matrix methods into inferred
zero/default transform initialization.

On success the allocator additionally clears +0x27c and +0x268, calls 48a160(0)
(writes +0 and copies position +0x3c to +4), increments global count 73a850 and
updates high-water value 73db0c if exceeded. It inserts the object at the tail
of the list with sentinel 73d880, writing object links +0x10/+0x14. A further
three-step sequence verifies tail insertion and all retained object bytes after
each allocation, including previous-tail and sentinel updates. Failure returns
null without those changes. Both constructor and allocator restore the exception
chain in the fixtures. The generic 486da0 factory, class/asset initialization,
handle assignment and completed gameplay entity creation remain outside this
evidence. Report: `artifacts/entity-construction-verification.json`.

## Generic factory identity and scalar defaults

`tools/verify_object_factory_fields.py` executes complete generic factory 486da0
with no world and no model. Allocation 487100, name assignment 4ffa80 and physics
initialization 49ec90 are intercepted; handle-pool operations, parent lookup,
vector/matrix copies and 48a160 execute unchanged. All 168 cases compare the full
0x1494 object and input parameter block: seven object types, four flag patterns,
valid/stale parent and three radii. This is not a full entity spawn or validation
of the intercepted effects.

The factory consumes the original free handle slot, stores its generated handle
at +0x2c, assigns a temporary UID at +0x20 from decrementing global 59f7e4, sets
type +0x24 and parent handle +0x30, and copies parameter word +0x10 to object
+0x1fc. It sets health +0x34 to 100 and armor +0x38 to zero. Position copies go
to +0x3c, +0x6c, +0x238 and (via 48a160) +4; matrix copies go to +0x48/+0x244.
The no-world spatial assignment writes zero at object +0.

Without a valid parent, +0x28 becomes zero and friendliness +0x1f8 becomes 1.
With a valid parent, those two fields are inherited unchanged. The level loader
then overwrites them with authored values, explaining why these defaults must
not replace the recovered level fields. The parent handle itself is retained
even when it fails lookup.

Input object flags receive 0x6000000, and an initial hidden bit 0x4000 also adds
0x8000. Types other than 5/6/8/9/10 receive 0x400000 after physics/spatial setup.
Several reference fields become -1; +0x270 becomes byte 255 and +0x278 zero.
With no model, model +0x80 is zero, model index +0x84 is -1, and nonpositive
radius selects 1.0. A negative input radius is replaced in the parameter block
by that result; zero remains zero there. Flag 0x10000 clears parameter +0x94
bit 0x20 before physics initialization. Full comparisons verify all other bytes
remain as supplied to this isolated overlay.

Report: `artifacts/object-factory-fields-verification.json`. Model loading,
world placement, physics initialization, failure rollback and completed class
setup remain to be reconstructed/integrated before operational entities.

## Physics initialization execution

`tools/verify_physics_initialization.py` executes complete original 49ec90,
49f010 and all callees without hooks. The 120 fixtures use no collision-sphere
mode, an empty destination sphere array, positive supplied mass and identity
orientation/inertia. Five material indices, four flag patterns, three masses
and two positions verify selected fields and unchanged parameter bytes.

Material indices 1..9 select their table entries; zero, 10 and -1 use entry 0.
The tested coefficients are copied from 649f50/649f54 with stride 28. Parameter
+0xc and mass +0x14 become physics +4 and +0x10. Positions become +0x5c/+0x68,
orientation +0x74/+0x98, and inertia +0x14/+0x38 in the identity fixtures.
Linear velocity goes to +0xbc; the +0x78 input vector goes to +0xc8 and its
mass-scaled value to +0xd4. The routines reset the empty-list collision radius
to zero at +0xf8, store flags at +0x120, clear +0x124 and establish additional
sentinel/scalar values checked by the verifier. Parameter radius 99 does not
override the empty-list radius in this mode.

Report: `artifacts/physics-initialization-verification.json`. This establishes
execution of the no-sphere path, not a complete shared initializer. Dynamic
sphere allocation, generated mass/inertia, nonidentity inertia and full entity
physics integration remain open; ordinary entity factory parameters commonly
select collision-sphere mode and need that additional path.

## Fallback collision sphere

`tools/verify_physics_fallback_sphere.py` executes complete original preparation
49ec90 and its callees with no model and empty source/destination sphere lists.
Only heap allocation 573619 is supplied; original array growth, constructors,
element copies, material lookup, initialization and bounds work execute. All
180 fixtures pass across five material indices, four sphere modes, three masses
and three positive radii. Material coefficients are seeded fixture values, not
claims about the installed material table's loaded values.

When parameter mass is nonpositive in this no-model path, it becomes material
density times radius squared. Positive mass is preserved. This observed formula
must not be replaced with a sphere-volume formula. Empty source lists receive
one fallback sphere: zero local center, supplied radius and -1 at element +0x10.
The initializer copies it into the destination physics list; the resulting
physics radius matches the supplied radius. Sphere mode flags remain unchanged.
The sixth word of the 24-byte element is copied too, but the fixture does not
assign it a default or meaning because the fallback local does not establish it.

The original grows each empty list to capacity 16 (384 bytes), allocating two
such buffers in these cases. Growth helper 40eeb0 and element copy 40ef70 run
unchanged. This reveals temporary-versus-runtime storage behavior; it is not
an instruction to keep the original allocation granularity in the Xbox port.
Report: `artifacts/physics-fallback-sphere-verification.json`. Existing sphere
lists, geometric-model inertia, allocation failure and a shared physics
implementation remain open.

`rf_physics_fallback_prepare` in `src/core/physics.c` now reconstructs the
fallback's generated/preserved mass and five defined sphere fields. The caller
provides resolved density, radius and mass for the no-model/empty-list branch.
The 24-byte output contains mass, zero center, radius and parameter +0x10 = -1;
the undefined sixth original sphere word is not exposed. No allocation occurs.
Sphere ownership, inversion of inertia and runtime insertion are still external.

Expanded `verify_physics_fallback_sphere.py` checks 4,020 original cases across
67 radii against PC and actual NXDK-linked code; mass and defined sphere bytes
match exactly. Nine additional port guards reject nonfinite inputs, negative
density/radius and generated-mass overflow without changing output on either
build. Guards are port policy, not claimed original behavior. Both builds and
all four CTest checks pass. Exhaustive floating-point equivalence, including
all density values and extreme exponent combinations, remains unproven.

## Authored sphere transfer and ownership

`tools/verify_physics_sphere_copy.py` executes complete preparation with existing
sphere lists and positive mass, supplying only heap allocate/free. Ten fixtures
cover counts 1/2/16/17/32 and positive/negative element parameter +0x10. Original
growth allocates capacity 16 then doubles to 32, copies every 24-byte record in
order and frees the previous buffer. Positive +0x10 sets flag 0x2000 in both
parameters and runtime physics. Axis-aligned fixture offsets verify radius as
the maximum center distance plus sphere radius. General-center rounding,
generated inertia and allocation failure are not covered.

Shared `rf_physics_spheres_open/close` now retains these records in one allocation
with exactly the required capacity. Budget includes owner and records, excluding
allocator overhead. Centers and radii must be finite and radii nonnegative;
the remaining fields, including opaque +0x14 bits, copy unchanged. Open requires
an empty destination and preserves it on error; source may close after success.
Close frees storage and clears the owner. This is storage ownership, not physics
initialization or application of the recovered flag/radius rules.

The PC probe compares all original output records, poisons its source, checks
exact/short budgets, rejects live-owner reopening and verifies repeated close.
Ten original lists plus an empty list pass; malformed-center, negative-radius
and null-source guards preserve output. NXDK builds successfully; resident Xbox
sphere ownership is not yet exercised. Report:
`artifacts/physics-sphere-copy-verification.json`.

## Authored sphere mass and tensor accumulation

`rf_physics_spheres_accumulate` reconstructs original 49ec90's existing-sphere
mass-generation loop, ending before the matrix inversion call at 49edf0.
The caller selects this branch when sphere mode is enabled, the source list is
nonempty and parameter mass is nonpositive. The routine preserves the initial
mass and all nine tensor elements instead of zeroing them. Thus a supplied -1
mass remains a -1 offset in the accumulated result.

Each sphere contributes `radius^3 * density * 4.188790321350098`, rounded to
float. Its center contributes the parallel-axis tensor terms; the original
loop adds no local `2/5 * mass * radius^2` sphere term. Ordered float stores
matter: the x*z contribution is stored before subtraction, whereas x*y and
z*y remain in x87 registers. The final diagonal reloads rounded x*x and y*y;
the other diagonals use retained products. Shared C makes these float stores
explicit and uses double intermediates. This is verified over the fixtures,
not a proof of equivalence to extended x87 arithmetic for every binary32 input.

`tools/verify_physics_sphere_mass.py` executes original 49ec90 through 49edf0
without intercepting any calls. Its 640 cases cover nonpositive initial masses,
zero and nonsymmetric initial tensors, all four sphere modes, material-index
fallback, 1/2/3/16/17/32 spheres, and zero/random centers and radii. It checks
unchanged source records and all parameter bytes outside the mass/tensor.
The resulting 40 bytes match PC and compiled NXDK exactly in every case.
Another 25 port-only invalid-input cases verify unchanged output, including
nonfinite inputs, negative radius/density, empty lists and overflowing results.

PC/NXDK builds and four CTest checks pass. Report:
`artifacts/physics-sphere-mass-verification.json`. The API allocates no memory
and also accepts an output alias of the initial structure. Matrix inversion
at 4fccf0, geometric-model mass generation, full initializer integration and
live Xbox entity physics remain open. No new rendered behavior is claimed.

## Inverse inertia tensor and composed sphere preparation

`rf_physics_tensor_inverse` now reconstructs complete 4fccf0, including the
4fc4c0 determinant and 505260 two-by-two minors. The determinant is compared
with zero before rounding; if zero, the original leaves every matrix byte
unchanged. Otherwise, division uses the stored float determinant. The first
eight cofactors are also stored as floats before division, while the ninth
remains in the FPU. This ordering is retained by the shared implementation.

The composed sphere fixture 198 exposed a double-precision determinant
mismatch that shifted all nine inverse elements. A small x87 determinant
helper now retains the original extended precision and operation order on
32-bit PC/NXDK, saving and restoring the caller's control word. Other targets
use a long-double fallback and remain unverified. The surrounding inverse
calculation and the new `rf_physics_spheres_prepare` composition are shared C.

`tools/verify_physics_tensor_inverse.py` executes complete original inversion
without hooks. All 1,007 zero/identity, integer-singular, near-singular,
mixed-scale and random nonsymmetric inputs match byte for byte on PC and
compiled NXDK. Twenty-one port-only cases reject nonfinite input and
unrepresentable determinants without changing output. NXDK also checks all
1,028 cases with aliased input/output and verifies control-word restoration.
These are finite fixtures, not an exhaustive floating-point proof.

`verify_physics_sphere_mass.py --prepare` extends execution through the original
inversion call at 49edf0 and stops at 49edf5. Shared accumulation followed by
inversion matches all 640 original cases, plus 25 unchanged-output guards,
on both builds. Original source spheres and all parameter bytes outside the
mass/tensor remain unchanged. The accumulation-only checks still pass.

Reports: `artifacts/physics-tensor-inverse-verification.json` and
`artifacts/physics-sphere-prepare-verification.json`. Both builds and four
CTest checks pass. The composed helper does not yet populate a runtime body,
copy its spheres, handle geometric-model mass generation or register live
entities. Those are still required before campaign physics integration.

## Body field coverage and world-space inertia

The no-sphere original initializer verifier now adds 360 complete 0x170-byte
destination comparisons to its 120 earlier preparation cases. Inputs include
dyadic nonsymmetric tensors and orientations, seeded material coefficients,
nonzero position/velocity vectors, three destination fill patterns and direct
49f010 preserve-state values 0, 1 and 255. Every destination byte and unchanged
parameter byte matches the independently assembled expected layout.

The two vectors at body +0xe0/+0xec are cleared; zero-radius lower/upper bounds
at +0x108/+0x114 equal the position. Body +0x124 is cleared only when the third
argument's low byte is zero. Other unassigned words retain caller storage.
This verifies original initialization writes, not defaults for those untouched
words. The original constructor remains responsible for its own fields.

`rf_physics_tensor_world` reconstructs complete 49cd30: transpose the orientation,
multiply orientation and local inverse tensor through 40ea80, then multiply
that float intermediate with the transpose and copy the result to body +0x38.
Read as row-major arrays, the result is `transpose(O) * (T * O)`; no change to
the original vector storage convention is implied. The shared helper preserves
40ea80's per-element summation order and the intermediate float stores, using
double products/sums. It allocates no memory and supports either input alias.

`tools/verify_physics_tensor_world.py` executes the entire original update and
all callees with no hooks. All 1,200 identity, quarter-turn, arbitrary-matrix,
diagonal and nonsymmetric tensor cases match PC and compiled NXDK exactly;
every body byte outside +0x38..+0x5b remains unchanged. Thirty-seven port guards
reject nonfinite inputs or overflowing products with unchanged output. NXDK
also checks both input aliases for every case (2,474 extra calls).

Reports are `artifacts/physics-initialization-verification.json` and
`artifacts/physics-tensor-world-verification.json`. This does not establish
exhaustive floating-point equivalence or a shared complete body initializer.
Connect the mapped body fields, retained spheres and general-center radius
calculation before live entity initialization. There is no new visual output.

## Sphere radius and world bounds

`rf_physics_spheres_bounds` reconstructs complete 4a0cb0. It resets radius to
zero, then takes the largest local center length plus sphere radius. Original
40a000 computes the center length; 4a0ce2 stores that length as a float before
adding the radius. The sum is also stored as float before the maximum update.
No body orientation is applied to this enclosing radius.

Bounds are position minus/plus the resulting radius, rounded to float and
ordered per axis through original 539460. Equal endpoints select the second
as minimum and first as maximum, which preserves the original signed-zero
behavior for an empty list at a negative-zero position. The helper allocates
no memory, permits an empty list and rejects nonfinite inputs, negative radii
and unrepresentable intermediate/output values without changing the result.

`tools/verify_physics_sphere_bounds.py` executes complete original 4a0cb0 and
all callees without hooks. All 1,000 empty and 1/2/16/17/32-sphere cases match
PC and compiled NXDK exactly for radius and six bound floats. Every other body
byte and all source sphere bytes remain unchanged. Cases include arbitrary
centers/positions and signed-zero empty bounds. Eighteen port-only guard cases
also preserve output on both builds. Report:
`artifacts/physics-sphere-bounds-verification.json`.

Shared C uses double products and square root before the required float stores;
these comparisons do not prove equivalence for every extended-x87 input. The
radius, bounds, tensors, mapped initialization fields and sphere owner are now
available, but still need to be combined into a runtime body initializer and
connected to entity construction. No new rendered behavior is claimed.

## Shared owned physics body

`rf_physics_body_open/close` now combines fresh 49f010 field initialization,
world tensor updates, sphere bounds and retained sphere records. Input material
coefficients are already resolved, and mass/local inverse tensor are already
prepared. This models the original third-argument-zero path with an initially
empty destination sphere list. It does not redo 49ec90 mass generation.

The 308-byte `rf_physics_body_state` represents fields assigned by that path:
coefficients, mass, local/world tensors, current/previous transforms, velocity,
the input vector and mass product, cleared vectors, radius/bounds, flags and
the verified scalar/timer defaults. Uncertain fields keep original-offset names.
Original untouched words are omitted rather than assigned invented defaults.
Body flags incorporate 0x2000 when a copied sphere's +0x10 parameter is positive.
The input parameter structure is immutable; callers obtain updated flags from
the resulting body, unlike the original mutable parameter block.

Only flags masked by 0x70 select sphere copying; otherwise the source list is
ignored. The body owns its copied records after source release. Its budget
includes the complete body and exactly count*24 record bytes, excluding heap
overhead; the existing sphere owner accounts its own subset without double
counting. On 32-bit PC/Xbox, an empty body accounts 324 bytes and a 32-sphere
body 1,092 bytes. Failure preserves the owner, reopening a live body fails,
and closing frees records and clears the complete structure.

`tools/verify_physics_body.py` executes complete original 49f010 and all callees,
supplying only heap allocation/free. For 480 cases across eight flag modes,
0/1/2/16/17/32 spheres, varied transforms and zero/positive mass, all 308 shared
state bytes and all retained records match original output on PC and compiled
NXDK. Original parameters change only at the expected flags word. Both builds
exercise exact/one-byte-short budgets, reopening, source poisoning and repeated
close. NXDK additionally exercises 200 allocation failures and 62 nonfinite
parameter cases, preserving caller storage without leaking allocations.

Report: `artifacts/physics-body-verification.json`. NXDK checks run linked code
with a supplied process-local heap, not a live Xbox body in XEMU. Runtime entity
registration, class/model parameter resolution, remaining constructor fields,
body reinitialization and geometric-model mass generation remain open. There
is no new rendered output.

## Class-dependent physics parameters: integration gap

`rf_entity_creation_physics_flags` reconstructs original 42268b..42270e from
class flags at +0x724/+0x728, class kind +0x1b4, creation bit zero and the network
mode byte. Physics flags start at 0x80000000; the sphere-mode mask 0x70 is set
when secondary class bit 2 is present or primary class bit 0x40000 is absent.
Kind 4 adds 0x1000. Primary bit 0x4000 plus player creation adds 0x80. Primary
mask 0x401200 chooses 0x4000 instead of 8. A nonzero network mode plus player
creation adds 0x8000. These are separate from generic object-creation flags.

`tools/verify_entity_physics_flags.py` compares 9,216 original-block results
against PC and compiled NXDK. It exhausts the five relevant primary flag bits,
adds random full-width flags, and covers secondary flag combinations, class
kinds 0/4/-1, four creation masks and network bytes 0/1/2. The original player
locals are supplied after normalization to creation bit zero; no calls are
intercepted. Report: `artifacts/entity-physics-flags-verification.json`.

The immediate runtime target is Live Mines miner1 UID 9858. Installed entity.tbl
declares mass 100 and material flesh, plus collision entries named csphere_0,
csphere_1 and csphere_2. The adjacent numeric pairs are not yet assigned shared
semantics. The original factory reads a different, resolved class sphere array
at descriptor +0xcec: 42da40 addresses inline 40-byte entries after its count.
The factory copies center from entry +0x18, radius from +0, parameter from
+0x10 and opaque word from +0x14 into the 24-byte physics record. This is a
disassembly map pending execution verification and model/tag resolution.

That missing class/model conversion is why the body owner cannot yet represent
the authored actor correctly. The separate scalar Collision Radius declaration
must not silently replace the named sphere list. Complete that conversion and
material lookup before binding the resident level entity to a runtime body;
the factory-flag tests alone do not establish a spawned campaign entity.

## Model sphere source located and streamed

Original 423bd0 constructs resolved class spheres from model data before
applying named table overrides. 503250/501490 obtain the model sphere count;
503270/501500 obtain center and radius. For animated model kind 2, 501500
fetches a bone matrix through 51c590 and transforms the stored local center
with 4ff020. Kind 1 copies the stored center directly. Model sphere records
are 44 bytes: name[24], parent int32, center float[3], radius float. These
execution paths are currently disassembly/decompiler evidence, not a complete
verified reconstruction of 423bd0.

The installed V3C files contain one 44-byte CSPH section per sphere (type
0x43535048). New `rf_model_file_collision_sphere` streams the selected record
without loading the model payload. It adds a safe name terminator, validates
parent >= -1, finite centers and nonnegative finite radius, and preserves output
on errors or end-of-list. Parent validation against a loaded skeleton and bone
pose transformation remain caller responsibilities.

For miner.v3c the actual records are:

| Name | Parent | Local center (approximate) | Radius |
| --- | ---: | --- | ---: |
| csphere_0 | -1 | (-0.000073, -0.231521, -0.018492) | 0.60 |
| csphere_1 | 15 | (-0.000405, 0.110772, 0.031623) | 0.40 |
| csphere_2 | 8 | (-0.000423, 0.092343, 0.068714) | 0.15 |

The table's required numeric pair is copied into resolved record +8/+0xc,
with +4 selecting the first in single-player and second in network mode.
It does not directly replace these model radii. The optional table radius
override is applied only when positive. Optional +0x10/+0x14 values similarly
replace the resolved fields only when +0x10 is positive. These mappings still
need executable comparison before integration; do not infer physical meanings
for the unnamed coefficients from their numeric values.

`tools/verify_model_spheres.py` compares the shared PC reader against independent
file inspection for all 114 records across 95 installed V3C files. All fields
match. It rejects 55 malformed payload cases (44 truncations, trailing byte,
nonfinite fields, invalid parent and negative radius). NXDK compilation passes;
live Xbox streaming of these records is not yet exercised. Report:
`artifacts/model-spheres-verification.json`. Next, connect the existing skeletal
pose data to the two bone-relative miner spheres and apply the class overrides.

## Animated collision sphere placement connected to sampled poses

`rf_model_collision_sphere_pose` now transforms a loaded CSPH center through
the caller's evaluated bone matrix and copies its radius. Parent -1 uses an
identity matrix, matching 51b2e0; nonnegative parents select the supplied bone
array. It preserves 4ff020's component-specific addition order and float stores.
Finite-input checks, a nonnegative radius requirement and bone bounds checks
leave output unchanged on failure. Virtual-bone/attachment indices beyond this
array remain unsupported and return RF_RANGE instead of invented placement.

`tools/verify_model_sphere_pose.py` executes complete original 503270, 501500,
51c590, 51b2e0 and transform callees without hooks. Valid cached bone matrices
avoid unrelated animation advancement. Three hundred synthetic queries cover
parent -1 and four actual bone indices. Eighteen additional queries use all
three loaded miner spheres with the existing shared skeleton sampler for
ult2_stand.rfa and ult2_crouch.rfa at ticks 0, 200 and 4000. These real asset
queries connect the new sphere reader's records with the established pose path.

All 318 original center/radius outputs match PC and compiled NXDK exactly.
Thirty-five port-only invalid-parent, negative-radius and nonfinite sphere or
matrix cases also preserve output on both builds. Report:
`artifacts/model-sphere-pose-verification.json`, including the 18 resulting
miner centers/radii. This verifies the query with already evaluated matrices,
not uncached original animation evaluation or full entity initialization.
Named class overrides, material resolution, class-specific center adjustments
and live Xbox runtime binding remain open. No new rendered output was produced.

`rf_level_actor_assets_load` now binds a selected level UID to its decoded
entity record, table metadata and installed compiled skeletal mesh entry.
It preserves the complete authored transform, class/script/state-animation and
skin fields. Only `.vcm` declarations are supported here; empty and other model
types return FORMAT, while missing UIDs/classes/skins/files return NOT_FOUND.
There is no silent alias or fallback model. Caller output remains unchanged on
failure and callers retain their level and mesh archives. Table loading uses
the supplied temporary-byte cap; this per-actor helper currently reloads the
table and is not an efficient bulk scene loader or a gameplay entity factory.

`rf_level_entity_find` validates the entire entity section before publishing
the matching record, rejecting duplicate matching UIDs and malformed later
records. The level tests check absent UID, duplicate UID and a valid first
match followed by malformed data, including output preservation.

`tools/verify_level_actor_assets.py` checks all 78 L1S1 records against the
separately verified entity export, table declarations and archive inventory.
All 78 bind successfully, with exact float transform bytes and ordered skin
lists. Five rejection fixtures cover absent UID/class, unsupported static-model
declaration, empty model and missing compiled mesh, with output preservation.
PC/NXDK builds and four CTest checks pass. This validates binding composition,
not the original gameplay loader, animation selection or rendered placement.
Nearby miner UID 9858 retains its base materials; UID 8322 selects skin b.
Next connect these records to the combined world/actor renderer, keeping
original animation-state and gameplay initialization as separate open work.

Xbox model previews now accept a `model-skin.txt` disc file containing a miner1
skin name (up to 63 bytes including any trailing CR/LF). Missing file selects
base materials; empty, oversized or embedded-NUL selections fail. Both PC and
Xbox use `rf_entity_assets_load`, which loads entity.tbl into temporary heap
storage under a caller-specified cap (512 KiB here), reads metadata, and frees
the table before material loading. Failure preserves the caller's output.
The probe checks all 168 selections against the buffer reader, exact table-size
budgets, one-byte-short rejection and missing-skin output preservation.
PC/NXDK builds and four CTest checks pass.

Diagnostic ABI version 9 adds skin-name checksum and replacement count in words
56/57; `xemu_smoke.py --skin Parker` checks both and derives the selected GPU
texture allocation. PC `--model-skin-last` produces frame 63 for comparison.
Stock 64 MiB XEMU run `20260908-223040-356250` passes all 64 submitted frames,
Parker checksum 0x19d75f2e, 12 replacements and 794,628 GPU image bytes. Its
native framebuffer was visually inspected and passes PC comparison: 4 pixels
over error 3, maximum channel error 114, mean maximum error 0.004235. Repeat
`20260908-223124-961060` passes without capture. Base-selection control
`20260908-223011-295569` also passes without capture. This remains a posed
inspection; no combined level/actor rendering or original skin-switch parity.

Initial stack-local metadata builds hit XEMU's NV2A surface/DMA-limit assertion,
including a base-skin control. Keeping the 4,164-byte selection in static
diagnostic storage instead produced the successful runs above. The executable
reserves a 64 KiB stack; this reduces nested stack demand but does not establish
the assertion's root cause. Audit the animation/preview stack and GPU transition
before claiming the failure fully explained. The harness now gives QMP a
bounded 30-second response timeout and tolerates close errors so an emulator
disconnect cannot prevent process cleanup and writing its diagnostic report.

`rf_model_materials_open_skin` adds bounded ordered primary-texture substitution
to the shared material loader. Nonempty selections must match the complete
SUBM material count; names must terminate within 32 bytes. Zero count uses base
materials. Secondary maps and other serialized fields are retained, while
deduplication and alpha classification use the selected images. The existing
resident/peak budget and failure cleanup apply. This is port-owned skin-loading
scaffolding, not a reconstructed original runtime skin switch. One original
`$Skin:` parser at 0x40f4f0 passes capacity 12 to 0x512e20 at 0x40fa55; that
static observation does not prove the material substitution algorithm.

`tools/verify_model_residency.py --skins` checks all five miner1 variants
(b/c/d/e/Parker), 60 complete runtime material records, selected texture slots,
secondary maps, alpha flags and exact Win32 memory accounting. Each bundle uses
797,800 resident bytes and 799,000 peak bytes. Five insufficient-budget cases,
five missing-texture cases and 20 invalid selection count/name cases fail with
empty outputs. The original base path still passes all 95 models/733 records;
PC/NXDK builds and four CTest checks pass.

The PC diagnostic accepts `--model-skin meshes.vpp motions.vpp output.ppm
tables.vpp skin maps1.vpp ...`. It reads miner1 metadata from the supplied table
(512 KiB input cap), verifies its skeletal filename matches the diagnostic's
miner geometry, then applies its skin list. This is explicitly a miner pose
inspection, not arbitrary entity spawning. Missing skins fail before rendering.
Parker's first-frame preview at `artifacts/miner-parker-pc.png` was visually
inspected; comparison with the same base pose changes 465 pixels, confined to
the face rectangle [283,142)-(309,163). The faceplate remains translucent.
Xbox selection, level placement and original skin-switch equivalence are open;
this new visible result is PC only.

`rf_entity_skeletal_filename` reconstructs the `.v3c` specialization of original
filename helper 0x5142d0, called at 0x51ce8f by skeletal loader 0x51ce60. Its
callee 0x514330 finds the last dot using the unchanged CRT helper at 0x573b10,
copies the preceding bytes and appends the requested extension. This includes
dots in directory names; case is preserved. No dot means append, while empty
input becomes `.v3c`. The caller must therefore skip entities without a model.
The port requires termination within 64 input bytes and a result of at most
63 bytes plus NUL, supports in-place conversion and preserves output on failure.
Those capacity guards are added port policy; the original helper is unbounded.

`tools/verify_entity_model_filename.py` executes the original helper and its
unchanged callees with `.v3c`, comparing all 64 output bytes for 1,960 accepted
fixtures. Another 41 cases validate capacity rejection/output preservation;
the probe self-test also checks in-place conversion and null input. PC/NXDK
builds and all four existing CTest tests pass. This establishes filename
conversion, not file loading or model-type selection.

Of 50 table classes authored with `.vcm`, 49 resolve to names in `meshes.vpp`;
`edf_ship.v3c` is absent there and needs investigation. Twelve other model
declarations use `.v3d` and remain outside this skeletal resolution path.
For example, the archive contains `sturret_head.v3m`, not `sturret_head.v3c`.
The table parser at 0x41b910 distinguishes extension types around 0x41ba35;
recover that dispatch and the static-model path before treating this as a
general entity model resolver. Ghidra export records extension references in
`model-extension-xrefs.tsv`. No runtime scene integration or new image yet.

`rf_entity_assets_read` now selects authored model names and ordered skin
texture lists from caller-owned `entity.tbl` text without allocating memory.
It supports the installed quoted-string, whitespace, parentheses and `//`
comment syntax, with ASCII-insensitive class/skin selection. Limits are 255
bytes per token, 63 per asset name and 64 replacements. Missing class or skin
returns NOT_FOUND; failures preserve the output. An empty model is valid for
a class with no authored mesh, such as the freelook camera. Other table fields
remain uninterpreted. This is metadata-reading scaffolding, not a decompilation
of the original general table parser; unsupported syntax is not claimed.

`tools/verify_entity_assets.py` independently extracts installed declarations
and checks all 63 classes and 168 base/skin selections against the compiled C
reader, including commented-out skins and original replacement ordering.
The probe self-test covers comments/case, missing selections, an unterminated
quote, replacement overflow and unchanged output on failure. PC/NXDK builds
and four CTest checks pass. The caller-side probe caps its table load at 512 KiB;
the runtime API consumes an existing buffer and owns no table storage.

`miner1` names `miner.vcm` and has five authored skin variants (b/c/d/e/Parker),
each with 12 replacement names. Its installed geometry is `miner.v3c`; the reader
preserves the authored extension, and the separate skeletal filename helper
now converts it to the compiled name. Runtime model selection and skin
replacement application are still open.
The installed level classes `camera1` and `Bucket Bot` do not directly match
the table class declarations; do not invent a model or silently alias them.

`rf_level_entities_begin` and `rf_level_entity_next` traverse the installed
v180 entity section (0x30000) without allocating its payload. Each result
contains UID, class/script names, position, reordered orientation, state
animation and skin, plus the section-relative raw record span. Other behavior
fields are traversed to locate the next record but are not interpreted or
applied to gameplay. The class name is not assumed to be a mesh filename.

Strings retained in the public result are bounded to 255 bytes plus terminator;
unretained strings are skipped with section bounds checks. Nonfinite transforms,
invalid optional-range tags, impossible counts and trailing bytes fail. The
iterator and caller result remain unchanged when a record fails. NOT_FOUND
means all declared records consumed exactly the section. The caller keeps the
level/archive open throughout iteration. There is no entity spawning, AI,
inventory, model/skin resolution or game-state initialization here.

The layout lead is Rafal Harabien / wardd64's
[rf-reversed RFL specification](https://github.com/rafalh/rf-reversed/blob/master/rfl.ksy),
published as GPL-3.0-or-later. The downloaded reference on 2026-09-08 has SHA256
`78d4cdf16a66a1fd991405786f553a86e539663c011d8b0eaae43169ab0b9e2b`.
This project adds its own bounded C reader and independent byte-inspection
tool; no generated Kaitai parser is used. Field names are format-reference
annotations, not evidence of original executable loader semantics.

`tools/verify_level_entities.py` reads all installed entity-bearing level
sections and compares every exported field and record span against the compiled
C probe. All 1,610 records in 66 levels pass, with exact section exhaustion.
The report at `artifacts/level-entities-verification.json` includes class counts
and Live Mines placement candidates. In L1S1, miner1 UID 9858 is at approximately
(-92.402, -3.191, 49.761), near the stored player start; the nearest entity is a
Grabber, so proximity alone must not choose the actor's model.

The level tests add orientation-order validation, 155 truncation boundaries,
nonfinite transform, overlong retained string and invalid optional-range-tag
checks. PC/NXDK builds and all four CTest tests pass. Verification covers file
decoding and bounds, not execution of the original entity loader or gameplay.
No new visual output was produced.


## Cached stance centers, rather than per-frame bone colliders

Original 423b90 tests class flag 0x40000000 before constructing spheres with
423bd0. Sixteen original-code cases confirm that an already cached class
returns without entering construction. Uncached cases reach construction;
these guard checks do not execute its model-loading work. Thus the current
frame-zero pose is a provisional source for the cache, but retaining centers
between ordinary animation frames is not itself evidence of a defect.

4289d0 sets actor flag 0x400 and copies center vectors from class array +e30
(crouched) into the existing body spheres. 428a60 first queries upward using
the existing crouched body. Its endpoint preserves public X/Z and computes
Y as class +f74 height difference plus public Y plus float .1, with the final
rounding after both additions. On clear space, it clears flag 0x400 and copies
centers from class +cec (standing). These copies preserve radii, other sphere
fields, array ownership and bounds; a ground query follows and handles support.
The model pose does not supply fresh centers on each of these transitions.

Shared `rf_physics_stance_centers` implements that center/flag commit without
allocation, and `rf_physics_stand_endpoint` prepares the original query end.
The caller must select the cached stance, perform standing clearance first,
and refresh ground afterward. Entity animation completion, crouch timing,
network notification and the original timestamp remain caller responsibilities.

`tools/verify_physics_stance.py` executes the original crouch entry and the
successful-clearance standing suffix up to their ground calls, plus the full
standing entry up to its clearance call. No callees are replaced. All 288
PC/NXDK cases match sphere bytes, flags and endpoint; unrelated original actor
storage is unchanged. Cases span zero through eight spheres and both stances.
Three malformed-argument checks verify failure preserves records and flags.
These APIs are compiled for Xbox but are not yet wired into the live scene:
correct initial standing/crouching cache generation and the clearance/ground
sequence must be connected before claiming working crouch physics.
