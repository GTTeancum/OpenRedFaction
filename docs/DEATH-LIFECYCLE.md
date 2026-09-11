# Death lifecycle reconstruction

The live campaign does not yet have a complete death-start, dying-update,
game-over or respawn owner. Negative health and the death-audio bit are not
substitutes for those transitions.

## Death-entry state prefix

`rf_entity_death_entry_sp` reconstructs the SP state prefix of original
41fdc0 through41fe59. It returns without mutation when entity810 bit1 is
already set. Otherwise it clears the three words at714, conditionally clears
144 when the resolved42a020 low byte is zero, clears150, sets810 bit1 and
clears1a8 bit8000. Other flag bits and falling144 bit patterns are preserved.
The vector names retain original offsets because their full ownership and
meaning have not yet been established here.

`python tools/verify_death_entry.py` passes4096 cases:2074 already dying and
2022 entering. The original executes from41fdc0 including its SEH setup and
actual4fad00 vector clearing, with SP network globals zero. Only42a020 is
supplied at a predicate boundary. The harness stops before48c9f0 collision
teardown or at the already-dying return; it compares exact owner bytes with
the PC probe and compiled NXDK helper. It also verifies unchanged original
actor bytes outside selected fields and the predicate call count. Random
vector words include arbitrary floating-point representations; supplied
predicate words cover zero low bytes with nonzero upper bytes.

The helper is intentionally not called by the live scene yet. Next work is
integration of the verified48c9f0 cleanup and remaining41fdc0 side effects, followed
by41ee40 dying-update and the player/camera handoff. This isolated instruction
comparison is not an XEMU gameplay or complete lifecycle claim.

Validation: PC Release and NXDK builds pass; all12 CTests pass. Original
RF.exe SHA256:b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

## Collision-pair retirement

`rf_collision_pairs_retire` reconstructs full48c9f0. The original active
list lives at73db28 and the available list at75db30. Each has a head pointer
and a32-bit count. Node offsets0/4/8 are next/first actor/second actor; the
remaining payload is untouched. The runtime supplies resolved actor identity,
not an authored UID or registry slot number.

Traversal saves next before removal. Matches on either endpoint decrement
the active count, repair the head or previous link, and prepend the node to
the available list while incrementing its count. Previous remains unchanged
after removal, preserving consecutive-removal behavior. Thus removed nodes
appear in reverse traversal order ahead of the existing available list.
48cc70 is a free-list push, not a heap free. No allocation occurs here.

`python tools/verify_collision_retire.py` passes4096 fixtures and43750
retirements. It executes the complete original48c9f0 with its unmodified
40a490/48ccf0/48cc90/48ccb0/48cc70 helpers, without substituted callees.
Fixtures include empty lists, arbitrary node order, either endpoint and both
endpoints matching, existing available lists, consecutive removals, and
32-bit counter wrap. PC and NXDK agree with the original on all resulting
links, identities and counters. Original and NXDK full node buffers also
match, including untouched payload. Both builds and all12 CTests pass.

The reconstructed API requires valid disjoint finite lists and exclusive
ownership. The64-byte fixture stride is test storage, not a recovered full
original pair size. Live pair creation, payload layout and ownership remain
to be connected; this component does not independently enable player death.

## Death-action selection

`rf_entity_death_select` reconstructs420c00 with a read-only420d00 clearance
callback and the explicit shared CRT random owner. It selects action16 when
flags810 bit400 is set or either138c/1390 equals13. Otherwise actions6/8/11
query direction1 and7/9/12 query direction0; a zero low-byte response forces
random fallback. Other predicate return bits do not affect the branch.

Action12 surviving clearance consumes one random draw: odd retains12, even
falls through to a second draw. An unset/rejected action consumes one draw
and selects5/14/15 using draw modulo3. A missing selected motion falls back
to5 if available, otherwise-1. Selection leaves the actor unchanged. The
caller still owns committing824, motion playback and subsequent death flags.
Inputs are limited to action-1 or0..44 and callbacks must not mutate the
actor or random stream. Arbitrary original out-of-range memory reads are
not supported by this API.

`python tools/verify_death_selection.py` passes8192 cases, comparing complete
original420c00 with real40a130/42a650 predicates and57312d CRT draws. Only
420d00 clearance and577eef thread-data lookup are supplied. PC/NXDK match
selection, random-state advancement and clearance callback order; owner
bytes remain unchanged. Cases consume zero/one/two draws5592/2296/304 times.
Both builds and all12 CTests pass. Geometry clearance and playback remain
unimplemented in this chain; no XEMU death animation is claimed.

The outer SP41fdc0 path skips this selector for an ordinary player detected
by4895d0 with globals6fc4d8/64ecb9 zero, storing824=-1. Flags810 bit80 also
bypasses its animation branch. Otherwise a supplied83c action precedes the
selector. Further outer work includes model overlay cleanup, motion mapping,
428c90 playback and the base class294->724 bit200000 behavior, which sets
entity810 bit02000000 instead of the usual post-play bit8. These details
are traced dependencies, not yet a reconstructed complete death owner.

## Clearance oracle for420d00

The next C implementation can now be checked against
`tools/verify_death_clearance_original.py`. This runs full original420d00,
including real vector copies, scaling, distance and local-space transforms.
Only498e80 is intercepted, recording both endpoint vectors and asserting
flags1 and a null optional hit output. It supplies explicit return words.
8192 fixtures pass; ray counts1/2/3/4 occur1429/942/580/5241 times,3091
cases allow the direction and1776 are rejected by nearby actors after all
four ray queries succeed. Full actor buffers remain unchanged.

Verified endpoint construction uses position3c, forward60, extent180 and
height-like field78. Direction's low byte equal1 selects extent180*3;
all other bytes select extent180*(-3). Scale forward by that length to
obtain the displacement. The ordered rays are:

1. Position to position plus displacement.
2. The same ray lowered by field78*0.5 on Y.
3. At halfway along the displacement, downward by field78+1 on Y.
4. At the full displacement, downward by field78+1 on Y.

For the first two rays only a low-byte result exactly1 rejects clearance.
For the two downward rays a zero low byte rejects; any nonzero byte passes.
Each rejection returns immediately without remaining queries or actor scans.
This distinction matters even though the normal498e80 return is boolean.

After the rays, the original traverses the entity list at5c95ec until the
5c9360 sentinel, following28c. It considers raw class294->74 bit4. The candidate
must lie within a3D squared distance of (abs(length)+candidate180)^2. It
transforms candidate position minus dying actor position into the actor's
local orientation48. For direction byte1, local Z must be>=0; for byte0,
Z must be<=0; other nonzero bytes do not apply the sign gate. It then rejects
when abs(local X)<abs(local Z). Equal magnitudes do not reject. There is no
explicit self-identity exclusion in this loop. Candidate payload is unchanged.

The independent oracle uses finite dyadic coordinates and four exact yaw
orientations. It covers noncanonical direction/result upper bytes, class-word
filtering, early exits, existing nearby actors and both direction branches.
It does not establish arbitrary floating-point edge behavior. The C port
must retain intermediate float rounding and original transform operation
ordering; this oracle should be extended to compare that compiled code.

498e80 itself tests moving solids and the static world via4df1c0. The native
ray implementation, actor-list owner and final animation playback must be
connected before this can run as part of campaign death selection. Do not
replace these checks with unconditional clearance or an empty actor list.

## Shared C clearance implementation

`rf_entity_death_clearance` now implements the complete420d00 decision path
with borrowed ordered actor candidates and a498e80 ray callback. It retains
float storage boundaries and double intermediate arithmetic for the original
53-bit x87 precision setting, including Z/Y/X transform accumulation order.
The callback provides flags1/null-output ray semantics. Inputs must remain
stable, with finite representable intermediate values. Neither actor owners
nor candidate buffers are mutated; no allocation occurs.

`python tools/verify_death_clearance.py` compares exact ray endpoint bytes,
query counts and final decisions against the original and both compiled
PC/NXDK implementations. All16384 cases pass: the prior8192 independent
dyadic fixtures plus8192 finite float fixtures with continuously varied yaw,
positions, extents and nearby actors. This extends coverage beyond the
original-only oracle; NaN, infinity and overflow are outside the contract.
PC Release and NXDK builds and all12 CTests pass. This is machine-code
comparison, not an XEMU live gameplay claim.

The next integration still needs the real498e80 geometry path and ordered
registered candidate ownership. The API does not automatically substitute
an empty actor list or bypass ray checks. Full death-start/playback and the
player/camera handoff remain open.

## Retained geometry adapter

`rf_geometry_death_clearance` now connects the shared clearance routine to
`rf_geometry_collision_ray` with flags1 and no hit-detail output. Each query
uses the retained moving-solid views followed by the static world. The
caller supplies ordered actor candidates; there is no hidden empty-list
fallback. The adapter allocates nothing and shares the existing serialized
tree scratch. It preserves the caller's allowed value on failure, latches
the first geometry error, and skips subsequent geometry queries after it.

The PC npc_motion_residency CTest now includes a real collision-tree floor
and a flat moving-door solid. It verifies forward/backward floor clearance,
a nearby actor blocking only its relevant direction, a closed door rejecting
clearance, translating that door away restoring clearance, removal of floor
support rejecting clearance, malformed mover views propagating an error,
and invalid actor input preserving the caller result. These fixtures invoke
actual geometry code; ray outcomes are not supplied by a stub.

PC Release and NXDK builds pass, and all12 CTests pass. The geometry fixtures
run on PC; the NXDK build alone does not establish native emulator behavior
for this new adapter. Prior16384-case original/PC/NXDK clearance comparisons
remain component evidence. Live registered actor-list construction, retained
extent/class fields and scene death-start/playback still require integration.

## Class74 provenance correction

The obstacle field is now named `class_word_74`, replacing the misleading
`class_flags_74`. This is a naming correction; its raw bit test is unchanged.
The original clearance instruction420ec9 tests byte[class294+74] with4.
The class parser41b910, however, writes a floating-point eye angle there:
41bd7c references595100, the string "$Min Relative Eye PHB:";5129d0 parses
three numbers into6c/70/74;41bd9b..41bdc1 multiply each by the binary32
radians factor at589428 (0.01745329238474369). Offset74 is the bank component.
Actual physics flags are at724/728 and must not be substituted here.

`python tools/verify_death_class_word.py` passes4096 supplied parsed-vector
fixtures through the original conversion instructions, comparing exact
float stores and untouched class bytes.317 resulting bank words have bit4
set. The initial harness lacked a mapped stack for an intervening push;
that setup issue was corrected before these results. This is conversion
block evidence, not a complete parser execution. PC/NXDK builds and all12
CTests pass after the field rename.

This appears to be an original offset/bit-test quirk; its intent is unknown.
Preserve the observed executable behavior. Campaign candidate construction
must retain the actual authored/default minimum eye bank bits. Existing
collision fixture words deliberately exercise the bit test and are not
claims about ordinary miner class values. Live integration remains open
until this metadata is read and retained faithfully.

## Authored eye-limit reader

`rf_entity_eye_limits_read` reads the optional paired minimum/maximum relative
eye PHB vectors and converts their binary32 degree values with the original
binary32 radians factor. The absent pair uses exact default words
bfc90fdb/0/0 and3fc90fdb/0/0, verified through41be17..41be34 and real42d840.
It uses the bounded decimal parser, avoiding NXDK strtod assertion stubs.
Unrelated Min/Max tags are ignored; malformed, duplicate and incomplete
pairs reject without publishing output. The first table sweep caught overly
broad matching of unrelated Min/Max tags, which was corrected.

`python tools/verify_eye_limits.py` passes63 installed classes and5 synthetic
cases against both compiled readers. Ten installed classes have authored
pairs; the rest take defaults. All successful results match the original
numeric conversion/default instruction blocks. The original whole parser
is not executed; parsed degree vectors are the supplied boundary. The
synthetic cases include a bank word with bit4 set and malformed/missing
pair rejection with output preservation. The NXDK emulation budget was
raised for larger class records; execution completes normally.

All63 installed classes initially have class74 bit4 clear. This is evidence
about initial metadata, not authorization to hardcode zero or omit the
candidate scan: runtime class changes and other input tables must preserve
the same raw-word behavior. Campaign class retention and candidate-owner
integration remain open. Both builds and all12 CTests pass.

## Retained campaign eye limits

`rf_entity_seed_class` now owns the24-byte eye-limit record. The seed loader
reads it while entity.tbl scratch is alive, propagates parse errors through
its existing cleanup path, and releases the table only after class setup.
Existing sizeof-based resident and peak accounting includes the added data;
class-array cleanup owns its lifetime without another allocation.

The extended eye-limit verifier checks retained values after closing both
archives, comparing them with direct reader output. L1S1 retains5 classes
(+120 bytes), L1S2 retains3 (+72), and L1S3 retains6 (+144). Their resident/
peak totals are104089/478729,52465/427105 and38399/413039 bytes on the PC
probe. Existing exact-peak success, peak-minus-one rejection with untouched
output, repeated cleanup and retained record checks all run in that probe.
Both builds and all12 CTests pass. Death candidate construction still needs
to bind this metadata to registered actor position and body/model extents.

Stock64MiB XEMU replay-20260911-174754 passes180 frames with base-memory
67108864 and no plugged memory after this retention change. Existing door,
NPC, player damage and pain/death-audio telemetry remains matched to PC;
player pain audio remains[5,3,3,3,49734,145,415139642,0,783005945]. This is
native regression evidence for campaign loading/ownership, not a claim that
the new death-clearance adapter was invoked by gameplay. No screenshot was
captured because this metadata change adds no new visual.

## Object radius provenance and model reader

Object78 is the model-origin radius, distinct from body sphere radius180.
The clearance field is renamed `model_radius_78` from `height_78`; geometry
and wire layout are unchanged. Original489fe0 calls5032d0, then40a000 on the
returned sphere center and adds the sphere radius before storing object78.
For model kind2,5032d0/5015f0/501610 use the first submesh at animated19c0.
504510 follows submesh90, then model8c, reading center1c and radius28. It does
not union all animated submeshes or use the physics collision spheres.

Original5696f0 reads LOD count/thresholds, then center into1c and radius28,
followed by the AABB. `rf_model_file_bound_sphere` reads that16-byte sphere
from the first SUBM payload at56+4*LOD-count, with section/entry bounds checks.
`rf_model_origin_radius` computes sqrt((x*x+y*y)+z*z)+radius using double
intermediates and a final float store; finite inputs and nonnegative radius
are required. Errors preserve output. The reader allocates no memory.

`python tools/verify_model_bound_sphere.py` passes all95 installed .v3c models.
It compares PC streamed sphere bytes with independent serialized offsets,
then executes original48a091..48a0c0 through the real kind2 pointer chain and
vector norm routine. Every computed radius matches PC and NXDK machine code.
Original file loading and native file I/O are not executed by that verifier.
Both builds and all12 CTests pass. Campaign radius retention remains open.
