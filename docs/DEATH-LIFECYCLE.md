> Correction: historical references below to corpse2cc as a sound handle/object are wrong; it is a type1 item handle. See the attached-item correction at the end.

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

After the rays, the original traverses the clutter list at5c95ec until the
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

## Retained NPC model radius

`campaign_npc_body` now retains the original object78 model radius separately
from its physics bounds radius. During per-class construction the loader
reads the first animated submesh sphere and computes its model-origin radius
once, then stores it in each constructed actor of that class. Errors use the
existing NPC cleanup path. The added4 bytes per actor slot are included in
the existing sizeof-based512KiB owner budget and the final body content hash.
There is no new allocation or per-frame file read. Both builds and all12
CTests pass. Player radius retention and the actual death-candidate binding
remain separate work.

Native stock64MiB replay-20260911-175724 passes180 frames. NPC_BODIES is
[78,78,191,48868,423508,1320800694], matching PC including the added radius
in the hash. Base memory is67108864 with no expansion;8544 pages are available
at completion. This exercises native file reading and retention during NPC
construction, not live death-clearance or animation playback.

## Registered scene clearance query

`rf_scene_death_clearance` now resolves registered NPC/player records and
calls the geometry adapter. It requires a fully initialized campaign scene.
The caller provides scratch capacity for every registered entity; scratch
may be partially written on failure, but allowed remains unchanged. Stale
target handles, inconsistent slots and unknown registered owner families
fail explicitly. No actor is silently dropped and the target is not excluded
from the candidate list, matching the original absence of a self check.

NPC target data uses retained published position, model78 radius, body180
radius and authored orientation. That orientation is valid for the current
stationary NPC scope; moving NPC publication remains open. Player data uses
the published attached pose and current body radius. Player eye limits and
model radius are now retained separately in28 bytes, accounted in
CAMPAIGN_PLAYER. They come from the same verified readers used for NPCs.
The query must not run during partial startup before the model is loaded.

Candidates are collected in registry-slot order. This is not original
factory-list reconstruction: order cannot affect this read-only any-blocker
decision with stable candidates, and there are no per-candidate callbacks.
The API does not mutate motion, damage, RNG, actor owners or geometry poses.
It does not invoke death-start or enable game-over behavior.

PC fixture coverage includes a registered player blocking an NPC in the
forward direction, backward clearance, exact copied candidate radius/raw
class word, stale generations, unknown owner families and insufficient scratch
capacity. These cases use the actual collision-tree floor. Both builds and
all12 CTests pass. Native query invocation remains a separate verification
step; the startup replay checks added player metadata loading and retention.

Native replay-20260911-180240 passes180 frames on67108864 bytes with no
expansion. CAMPAIGN_PLAYER is[16777471, 0, 8, 4328], including the added28
bytes; NPC body hash remains matched. This is startup/regression evidence;
the new scene query is not yet invoked by this native replay.


## Native registered clearance verification

The player binding now reads orientation from the physics body, which the
look code updates. The attached pose publishes position but does not maintain
its input matrix. The PC regression deliberately zeros that unused matrix
and checks a forward support miss using the actual body orientation.

The damage replay fixture invokes the registered query at frames1 and90 for
every registered actor in both directions, hashing results and copied
candidates in registry order. It allocates1580 temporary scratch bytes for79
actors and releases them after each pass; production queries remain caller-
scratch based. This is a read-only diagnostic, not death-start dispatch.

Stock64MiB XEMU replay-20260911-180931 passes180 frames. DEATH_CLEARANCE is
[2,316,255,61,79,1166757824,0,1580], exactly matching PC: two passes,316
queries,255 allowed,61 blocked,79 candidates, matching hash, zero status.
Both platform builds and all12 CTests pass. Complete death-start, animation
playback and moving-NPC orientation publication remain open.


## Complete original dying-update boundary trace

`python tools/verify_dying_update_original.py` executes full41ee40 across4096
cases with real vector initialization/scale/add and supplied owner/effect
boundaries. It verifies exact call order, arguments and whole actor memory.
There are3100 finalizations,1047 burn clears and100 damage calls. This is
original-executable evidence, not a PC/NXDK implementation comparison.

The flags810 bit80 path first calls42e3c0(handle), then releases a nonzero
burn13d8 through42ed20(burn,0) and clears it. Otherwise action824=-1 or a
zero low byte from428d10 permits finalization. Weapon41a830 must return
low byte exactly1 to invoke41ae70, even while the death animation continues.
Class728 bit20 plus a present player checks timer4b8; only low byte1 permits
the segment test from position to position+forward*model_radius78. Radius
above6 uses collision radius2.5 and camera gain1.25; radius6 or below uses
1.5 and1. Collision506ae0 must return low byte1 to damage the player for1600;
camera shake follows even on a miss. Finalization418f80 then precedes the
masako_endgame name check and optional UID118a/47c3 effects.

The harness supplies playback, weapon, timer, collision, damage, camera,
finalization and event boundaries without mutating their owners. Their
actual implementations, reentrant mutation, nonfinite geometry and native
Xbox integration remain outside this evidence. The next runtime step must
preserve these effects and order, rather than treating animation completion
as sufficient to remove the actor.


## Shared dying-update orchestration

`rf_entity_dying_update` reconstructs complete41ee40 control flow with an
explicit backend for playback, weapon, timer, collision, damage, camera,
finalization and endgame effects. It preserves the previously traced call
order and low-byte comparisons, including shake after a collision miss.
Its state is52 bytes, with a borrowed20-byte player view and16-byte backend
on Xbox; the routine allocates nothing. Burn cleanup commits only after its
release callback. The state must remain owned after finalization until the
following name/event work completes. Live entity retirement must respect that
lifetime when the backend is attached.

`python tools/verify_dying_update.py` compares all4096 original cases with PC
and NXDK machine code: exact operation arguments/order, segment coordinates
and radius, and complete state bytes. Both builds and all12 CTests pass.
These cases use stable owners, finite axis-aligned segments, four model
radii including6, and varied low-byte query responses. Actual backend effects,
reentrant mutation and native scene dispatch are not verified by this test.
The runtime must still connect death-start and finalization before enabling
this update for live actors.


## Finalization lifetime and corrected empty call

Direct inspection of42e3c0 proves its first instruction is C3 (ret). The
original dying-update verifier now asserts that byte and executes the real
function instead of supplying it. The shared backend no longer exposes the
misleading REMOVE operation. No deletion or other effect belongs at that
call. Both builds, all12 CTests and4096 original/PC/NXDK cases pass again.

Ghidra exports of418f80,48ab40,4a6d50 and416940 identify the next ownership
work; the following is static evidence, not complete execution verification.
Finalization418f80 starts with48ab40, which only ORs object7c with2 and does
not free storage. This matches the independently documented trigger path.
It then handles attached actors, invokes4a6d50 on a resolved player, and can
construct a corpse through416940. Burn ownership may transfer to that corpse
through42f510 and corpse2d0; otherwise the finalizer releases the burn.
Therefore replacing FINALIZE with immediate registry removal/free would lose
these effects and invalidate the following endgame-name read.

The ordinary SP branch of4a6d50 calls4a6e00 when player14 is not-1, then
sets player14=-1 and clears playerfb0. Other mode branches are distinct.
Recover4a6e00 camera/owner effects and the deferred object deletion pass,
then complete corpse construction and burn transfer before live binding.


## Ordinary single-player detach

`rf_player_detach_sp` reconstructs full4a6d50 with globals64ecb9 and6fc4d8
both zero. Its only writes are player14=-1 and bytefb0=0. The conditional
4a6e00 call has no effect in this mode: that helper records a multiplayer
handle/client-byte history ring only when64ecb9 is nonzero. It does not
provide a single-player camera handoff. This resolves the preceding static
follow-up without inventing a camera operation. Other mode branches remain
separate work.

`python tools/verify_player_detach.py` passes4096 original/PC/NXDK cases,
executing real4a6e00 and original dead/dying queries without replaced callees.
It compares whole original player storage, preserves adjacent bytes atfb0,
and proves both retained actor bytes and registry entries remain unchanged.
After detach the player link reports dead/not dying even while the actor
still exists in the registry. This distinction must survive finalization
integration: detachment is not actor destruction. Both builds and all12
CTests pass; this helper is not yet called by live finalization.


## Burn retargeting for corpse inheritance

`rf_burn_retarget` reconstructs42f510 after general-object40a0e0 resolution.
A missing target releases the burn through the backend. A valid target first
commits record.target, refreshes attachment indices, then changes all four
emitter owner fields, sets the fading byte to1, clears elapsed and ORs
object29c with200. Other burn bytes, including padding/voice/source/ring
links, survive. Unlike42e910 creation, this function ignores the original
bone helper boolean and retains partial matches. RF_NOT_FOUND from the shared
bone resolver therefore does not abort transfer or reset every index.

`python tools/verify_burn_retarget.py` compares full42f510 with PC/NXDK across
4096 cases using real handle lookup and supplied bone/release boundaries.
It verifies2071 missing-target releases and1378 partial-bone transfers,
normalized burn bytes, whole target/emitter storage, and attachment callback
observation of the committed target. Both builds and all12 CTests pass.
No allocation is introduced. The caller must still resolve live emitter
owner fields and attach the verified bone/release implementations. Corpse
creation and transfer of the actor/corpse burn-owner fields remain separate.


## Burn retargeting over retained resources

`rf_burn_retarget_resolved` now validates active burn membership and four
distinct active emitter tokens, invokes the real bone-name resolver, and
updates emitter runtime.owner plus bounds.owner. Both fields represent
original emitter+4 and must agree after transfer. Already-emitted particles
retain their owners and list membership. No resources are allocated and
voice ownership is unchanged on success. A missing target instead invokes
`rf_burn_release_resolved`, detaching surviving particles, returning emitter
slots, stopping the voice and clearing the burn owner.

The new burn_retarget_resources CTest uses actual particle/emitter pools,
a partial head/spine skeleton and particles already in flight. It checks
that duplicate/inactive emitter references fail before mutation; successful
retargeting preserves every particle byte and list; and missing-target
cleanup leaves particles alive, with emitter tokens cleared, while all four
emitters return to the pool. Both builds and all13 CTests pass, as does the
4096-case original/PC/NXDK core retarget verifier. The resolved adapter has
PC resource coverage, not native XEMU invocation yet. Live corpse ownership
and dispatch remain open.


## Native burn resource transfer/release

The shared burn_retarget_fixture now runs in PC CTest and inside Xbox replay
startup. Its444496-byte test/snapshot storage is allocated temporarily and
freed before campaign loading, including failure paths. It adds no permanent
particle/emitter pools to campaign memory. The fixture performs the resolved
transfer and missing-target release against actual pool implementations;
voice and owner callbacks record calls without playing a test sound.

Stock64MiB XEMU replay-20260911-183144 passes180 frames and reads native
telemetry[0,444496,4,88,0,4,2,1], exactly matching PC: success, temporary bytes,
four emitters retargeted to88, zero emitters after release, four particles
still alive, two cleanup callbacks and completed checks. The fixture also
checks partial bones, both ownership copies, preservation of particle/list
bytes on transfer and duplicate/inactive-token rejection. Both builds and
all13 CTests pass. This verifies native resource handling in a synthetic
fixture, not authored corpse creation or visible burning-corpse gameplay.


## Corpse retention policy

`rf_corpse_retention_apply` reconstructs constructor416da3..416dc6 using
416f20 count,416f80 oldest selection and4174f0 fade marking. A corpse is
excluded by flags29c bit1 (already fading), bits42, or object7c bit4000.
The helper keeps five eligible corpses and marks each oldest excess corpse
with fade298=1 and flags29c bit1. Creation time294 uses a strict less-than
comparison, so ties retain list order. Protected bodies do not count toward
the five-body threshold. This is not a total corpse or memory allocation cap,
and fade marking does not immediately free an object.

The shared view uses a borrowed null-terminated list,20 bytes per node on
Xbox. A caller-supplied visit bound and finite timestamps are checked before
mutation; cycles/overlong lists and nonfinite times reject without changing
output or nodes. No allocation or resource destruction occurs.

`python tools/verify_corpse_retention.py` executes the actual constructor
retention block and all real callees without replacements, then compares
PC/NXDK final state over4096 cases with0..32 corpses. It verifies21927 fade
marks, including27 in the32-eligible case, protection/already-fading flags
and tied/negative finite timestamps. Three additional NXDK preflight guards
pass. Both builds and all13 CTests pass. Full corpse creation, live list
ownership, fade progression and eventual resource deletion remain open.


## Corpse fade progression and continuation gate

`rf_corpse_fade_step` reconstructs417290 through4172ea, including the early
return for negative health. Negative health marks object7c bit2 and returns
continue_tick=0. Otherwise, flags29c bit1 decrements fade298 by frame delta;
a remainder <=0 marks bit2 but still returns continue_tick=1. Fade is not
clamped. The original FST stores binary32 but compares its remaining x87
value; the helper preserves that order using a double intermediate.

`python tools/verify_corpse_fade.py` passes8192 original/PC/NXDK cases with
real4174e0 and48ab40, no replaced callees. It checks signed zero, subnormals,
zero crossings, unused NaNs and finite active inputs:2088 early returns,4471
deletion marks,2383 marked corpses that must continue the rest of the tick.
Three PC/NXDK guards preserve state/output for invalid active inputs. Both
builds and all13 CTests pass. This helper is not the complete corpse tick.

Static examination of the remaining417290 body identifies timer2ac expiry
and linked emitter disable, class-dependent2b0 decay, a one-shot animation
transition under flags29c8, sound following through459a20/48ac70/48a230, and
normal model advancement503360. These must follow the continuation gate
even on a fade-expiry frame. Render alpha conversion and deferred object
resource destruction also remain unimplemented for live corpses.


## Complete original corpse-update trace

`python tools/verify_corpse_update_original.py` executes full417290 across
4096 cases with real timer expiry/invalidation, deletion/fade helpers and
vector initialization/copy. Model, pose and sound effect boundaries are
supplied; all actor/emitter/sound bytes and callback arguments/order are
checked. Coverage includes953 animation transitions,481 model/motion changes
during reset,1651 timer-triggered emitter shutdowns and699 sound follows.
This is original-executable evidence, not a shared runtime comparison.

After the fade prefix, timer2ac expiry invalidates that timer and clears
only the low enabled byte140 on every linked emitter. Positive2b0 then
decreases by dt * binary32(0.0020000000949949026) * class-table scalar, with
no zero clamp. A nonnegative motion2b8 plus flags29c8 resets the model,
rereads model/motion, plays at1 with flag1, queries duration, rounds that
x87 result to binary32 and advances the model by that duration then0.3.
It clears flags29c8 before calling4164c0. The reset callback mutation cases
prove model/motion must not be cached across that call.

Sound2cc=-1 skips lookup; a resolved459a20 record receives the48ac70 follow
point at sound+f0, then48a230 is invoked. Finally, a nonzero current model
advances by frame delta with the corpse position/orientation pointers.
Health-negative early return skips all this; fade expiry does not. Shared
full-update orchestration and actual model/sound effect binding remain next.


## Shared complete corpse update

`rf_corpse_update` now composes the verified fade gate, timer expiry/clear,
linked emitter low-byte shutdown, class-dependent2b0 decay, transition
playback/pose work, sound following and final model advance. It rereads
model/motion/flags at the original callback boundaries and continues after
fade-expiry deletion marking. Model duration is rounded to binary32 before
the transition seek, followed by0.3; final advance uses the current model
and borrowed position/basis. The sound view receives its follow point before
the backend move call.

The state is88 bytes, emitter links8 bytes and backend36 bytes on Xbox.
All are caller-owned; the routine allocates nothing. An explicit visit bound
protects emitter traversal. Invalid finite/timer/backend inputs report errors;
state changes or effects already performed are not rolled back. Actual model,
pose and sound effects remain backend responsibilities.

`python tools/verify_corpse_update.py` compares4096 full original traces to
shared PC and NXDK machine code, including481 model/motion mutations during
reset, timer/emitter writes, decay, exact animation arguments and sound
follow state. Both builds and all13 CTests pass. Native scene invocation,
corpse creation/rendering and deferred resource deletion remain open; this
is complete update orchestration, not complete corpse gameplay.


## Corpse animated sphere refresh

`rf_model_corpse_spheres_refresh` reconstructs4164c0 with already evaluated
animated bone matrices. It queries each model collision sphere, copies only
its center into the corresponding retained physics sphere, rebuilds bounds
over the entire physics sphere list and copies the resulting radius to
object78. The original writes the model radius into temporary stack storage;
using it to overwrite the physics sphere radius would discard class tuning.
Extra physics spheres remain intact and still participate in bounds.
The bounds position is physics+5c (object+e4), not object render position3c.

`python tools/verify_corpse_spheres.py` executes full4164c0 with real animated
503250/503270, list helpers and4a0cb0, no replaced callees. All1024 cases
match PC/NXDK centers, bounds and object radius;6117 sphere payloads retain
radii and other fields. Coverage includes0..8 model spheres, up to12 physics
spheres and four cached bones plus unparented spheres. Both builds and all13
CTests pass. No allocation is introduced; later query failures can leave
earlier centers updated while output bounds/radius remain unchanged.
Uncached bone evaluation and live corpse model ownership are still separate.

## Sound-follow point

`rf_model_sound_follow_point` reconstructs48ac70 with evaluated skeletal
poses. Object26c=-1 copies position3c directly, without requiring a model or
orientation. Otherwise503230 supplies the cached bone position and4fb9d0
rotates and translates it by the object pose. Original4faa90 sums Z, Y, X
products in that order, rounds to binary32, then40a030 adds translation.
This differs from the existing5034f0 tag-placement arithmetic ordering.
The original attachment result is AL (zero/one); the shared helper returns
RF status, while the supplied index identifies attachment versus fallback.

`python tools/verify_sound_follow.py` executes full48ac70 and its real
callees without hooks, comparing4096 outputs bit-for-bit against PC and
compiled NXDK code. Coverage includes four cached bones, arbitrary poses,
identity orientations, absent-model fallback and preservation of raw fallback
position bits. Six additional invalid-index/nonfinite guards preserve output.
Both builds and all13 CTests pass. The helper allocates nothing. This does
not yet bind live corpse sound owners or implement lazy/virtual bone queries,
and the compiled Xbox comparison is not a native XEMU gameplay test.


## Corpse ownership and complete original deletion

Static constructor416940 inspection distinguishes two model paths. Class+18
is the replacement model string. The zero-initialized globals5caed4/5caed8
are empty-string comparison operands:5001d0 tests equality and500290 tests
inequality. With an empty replacement string the constructor sets source
object7c bit400 before allocation, then reuses source model80 for the corpse.
With a replacement string it loads a separate model through502880(name,1,-1).
The source model pointer itself is not cleared in this constructor. The shared
model destructor489fc0 skips release whenever object7c bit400 is set; otherwise
it calls502b10 for a nonnull model. Allocation failure occurs after the early
source flag/deletion marking; do not silently assume those changes roll back.
These construction branches still need execution-level verification and live
resource binding.

The allocator487100 rejects type7 when global70e430 exceeds29: the original
has a30-corpse allocation ceiling in addition to the five-eligible-body fade
policy. The corpse pool at708748 has318-byte slots;48b870/48b8f0 manage its
free chain and occupancy counters. Protected/fading bodies still occupy slots.
This ceiling is statically recovered, not yet an integrated Xbox pool limit.

`python tools/verify_corpse_delete_original.py` executes complete486670 for
type7, with real416ff0,489fc0,4867b0,48b8f0 and48ab40. Resource backends are
supplied, while actual list, flag, registry and pool writes execute original
code. All1024 cases pass, including326 model releases and2046 emitter releases.
The verified order is collision-pair retirement; corpse string cleanup; sound
lookup/deferred deletion marking; optional burn release; corpse-list unlink;
physics cleanup; conditional model release; linked emitter cleanup; object
string cleanup; object-list unlink and corpse-pool return; registry slot clear
and free-slot insertion. Each resource callback still sees the registered
object. Emitter next is saved before release: the harness poisons freed emitter
storage to verify traversal does not read it afterward. A missing sound leaves
the corpse sound id unchanged; a found sound is marked object7c bit2 and the
corpse sound id becomes-1. The burn release backend remains responsible for
any owner-field clearing. Model80 is not nulled by489fc0.

This is original-executable evidence, not shared C/C++ deletion implementation
or native XEMU gameplay. Next: bind this ownership order to the corpse allocator,
registered model/physics resources and existing burn/sound owners, then integrate
creation/update/deletion as one lifecycle.


## Shared corpse deletion and registry lifetime

`rf_corpse_delete` implements the traced486670 type7 cleanup order using
caller-owned update/deletion state, intrusive corpse/object lists and the
existing shared object registry. It marks a found sound object for deferred
deletion, releases burn ownership through the backend, removes the corpse-list
entry, dispatches physics/model/emitter cleanup, removes the object-list entry,
returns storage through RECYCLE and finally removes the registry handle.
Every resource callback still sees the registered owner. A lifecycle field
blocks repeated/reentrant deletion; no owner/state reads follow RECYCLE.
The original model-release flag400 gate and missing-sound-id behavior remain.

Preflight checks live registration, nonzero distinct list counts, immediate
list consistency and bounded emitter traversal before any effects. Backends
must preserve registry/list ownership and perform infallible cleanup; the
current emitter can be destroyed because its next pointer is saved first.
RECYCLE must not immediately reuse/register the storage. The function allocates
nothing. The deletion view is40 bytes and backend12 bytes on Xbox, in addition
to the existing88-byte update state and external resource storage.

`python tools/verify_corpse_delete.py` compares1024 complete original traces
with PC/NXDK code, including shared list/count changes and real registry slot
removal. Four extra guards cover stale handles, deleting state, broken links
and cyclic emitters. PC callbacks attempt recursive deletion; emitter callbacks
poison released links. NXDK guest-memory hooks reject owner reads after pool
return. Both builds and all13 CTests pass. Resource/pool operations still use
supplied backends in these tests; this is not native XEMU corpse gameplay.
Live30-slot allocation, concrete model/physics/burn/sound cleanup binding and
creation-to-deletion scene ownership remain open.


## Shared bounded corpse slot allocation

`rf_corpse_pool_init/acquire/release` reconstruct48b7b0/48b870/48b8f0 slot
ordering under487100's type7 cap30. Fresh allocation uses indices0..29;
release pushes a slot onto the free-list head. A full pool returnsRF_NOT_FOUND
without changing output or metadata. Releasing an inactive/out-of-range slot
returnsRF_RANGE. Peak occupancy is retained. Metadata is136 bytes, with free
links and occupancy separate from caller-owned corpse records; no heap is used.
The allocator does not initialize payload, construct actors or publish handles.
This preserves reuse behavior without placing original pointer words into the
reconstructed record's fields. Live creation must explicitly initialize owners.

`python tools/verify_corpse_pool.py` compares5377 operations to original pool
code and shared PC/NXDK code:223 saturation checks,2535 slot reuses, peak30,
final live0. The original487100 gate rejects saturation even with its global
heap fallback enabled. Original payload bytes remain unchanged except the free
link words. Repeated full drain/refill, random release order and duplicate or
invalid release guards pass. The shared implementation intentionally exposes
only the bounded type7 path, not the generic pool's heap-overflow mechanism.

The corpse deletion probes now invoke actual shared pool release at RECYCLE
on PC and compiled NXDK, verify occupancy13->12 and slot7 becoming free, while
registry ownership remains until the later removal. PC also rejects a second
release. All1024 original cleanup traces and four guards still pass; both
builds and all13 CTests pass. These are compiled-code harnesses, not native
XEMU corpse gameplay. Live corpse records, constructor/model transfer and
concrete resource cleanup binding remain open.


## Full original corpse construction trace

`python tools/verify_corpse_create_original.py` now executes complete416940
across1024 cases:848 successful constructions,142 allocation failures and34
null sources. Allocation, model loading, pose refresh, emitter creation,
collision registration, string assignment and temporary-list reserve/free are
supplied backends. Class predicates, string comparisons/search, physical sphere
copies, timers, object field writes and list insertion execute original code.
The harness checks descriptor sources, source-object mutation, class/transition
flags, animation identifiers, model inheritance, timer behavior, list links and
334 extra-model transfers. Eight example call traces/write sets are retained.
There is one corpse in each list fixture; multi-corpse retention is covered by
the separate retention verifier, not this constructor fixture.

Confirmed details for the shared constructor:
- Empty class replacement-model string sets source7c400 before allocation and
  reuses source80. All nonnull sources are marked7c2 before allocation. Failure
  does not roll those marks back. The source model pointer is not cleared.
- A named replacement is loaded with502880(name,1,-1).38 successful constructor
  cases deliberately receive a null replacement model; the constructor does
  not convert that into allocation failure.
- The physical descriptor retains source8c at+0c, source98 at+14, caller position
  at+3c, caller basis at+48, source180 at+84 and cloned24-byte spheres at+88.
  Flags+94 are33 or73 according to class72480000. All0..4 spheres copy exactly.
- Transition motion requires nonnull source model, class model kind2, a found
  action and no class724200000. Seek argument must equal1 to set corpse29c8.
  Pose refresh follows transition assignment; object78 receives refreshed180.
- class724200000 with a valid sourcea44 selects503390(model,motion,1) and bit4.
  corpse_drop/corpse_carry lookup results populate2bc/2c0; substring search in
  the supplied death name gives direction0 for forward/front,1 for back,2 else.
- Class emitter index0 creates an emitter without setting a deadline; positive
  indices set the deadline even when emitter creation fails. The delay uses
  truncation of class lifetime*1000+0.5, then the real timer helper.
- Source810200000 transfers nonnull1410 into corpse2c8 and clears source1410.
  The rest of the source bytes are unchanged apart from7c.
- Sound id2cc is untouched by416940, and the inspected487100/486da0 allocation
  path does not initialize it either. Do not infer a forced-1 default. Its
  incoming storage value and subsequent sound-owner publication require care.

This is original executable evidence, not shared constructor code or native
XEMU gameplay. Runtime source/build outputs are unchanged by this evidence
commit. Next implement the constructor against the retained pool and resource
owners, using these verified ordering and failure semantics.


## Shared corpse constructor: PC field verification

`rf_corpse_create` now reconstructs416940 around a caller-owned rf_corpse
record, shared update/deletion state and explicit allocation/resource backends.
The allocator receives a copied physics seed through caller-provided scratch;
it must own that data before returning. Seed fields not represented in the
view correspond to zero-initialized original descriptor fields. No heap use
occurs inside the constructor. Original temporary-vector allocation is replaced
by bounded scratch; allocation backends still own the actual physics resources.

The constructor applies source deletion/model-retention flags, model selection,
motion/pose setup, timer/emitter setup, corpse-list insertion and oldest-eligible
retention, motion labels, extra-model transfer and final collision/source
callbacks. The owner contains its independent creation time, velocity fields,
model/physics radii and neutral presentation storage. Fields not written by the
original constructor, notably the incoming sound id, are preserved. Allocation
failure returns a null result with source flags retained; replacement-model
failure still allows construction. Guard failures after allocation expose the
allocated result so the caller can clean up; effects are not rolled back.

`python tools/verify_corpse_create.py` compares1024 complete original cases to
shared PC output:848 successful owners,142 allocation failures,34 null sources,
334 extra-model transfers and38 null replacement models. It checks scalar
owner/update fields, source ownership, copied sphere seed, links and velocity
reset. Both PC/NXDK builds and all13 CTests pass. Backend call-trace parity,
compiled NXDK execution comparison, constructor retention with several bodies,
and guard/cleanup failure coverage remain to be added. This is not live scene
creation: the concrete allocator/model/physics/emitter/sound owners are still
external backends, and live dispatch remains disabled.


## Xbox constructor fields and integrated retention verification

`python tools/verify_corpse_create.py` now also executes the compiled NXDK
rf_corpse_create with supplied resource callbacks and compares the same1024
full original cases to PC and Xbox owner/source outputs. Copied physics seed
position/basis, sphere bytes/count, flags and original scalar sources are
checked at the allocator callback. This is Xbox machine-code execution in
Unicorn, not native XEMU invocation.

`python tools/verify_corpse_create_list.py` adds1024 full-constructor runs
with0..29 existing corpses, totaling14796 existing owners. Complete original
416940 performs retention through its real416f20/416f80/4174f0 callees; PC
and NXDK creation must match every existing body's flags/fade and the new
owner's fields. Cases cover equal timestamps, newer existing bodies, mixed
protected/already-fading flags, null sources and allocation failures. Results:
6339 existing corpses begin fading, and70 new corpses must themselves fade.
The caller's list head/tail and new-node neighbours are checked too. All13
CTests pass after rebuilding the PC probe.

No gameplay source behavior changed in this verification step. The constructor
remains disconnected from the live scene while full resource-call ordering,
guard cleanup and concrete model/physics/emitter/sound ownership are completed.


## Constructor resource calls and partial-owner guards

The constructor verifier now compares7431 normalized resource calls across
1024 original/PC/NXDK cases. It checks allocation type/source handle, snapshot
and pose owners, model-load mode, motion names/order, playback model/motion/rate,
owned-name assignment, emitter handle/argument, collision registration and final
source effects. Actual callback source/owner pointers and string contents are
validated before normalizing addresses/names. Source42dc00 executes original
code while its entry is recorded. Temporary vector reserve/free are explicitly
excluded because the shared constructor uses caller-provided bounded scratch.
The1024 multiple-body cases still match all14796 prior owners and their fades.

`python tools/verify_corpse_create_guards.py` verifies five PC/NXDK rejection
paths: insufficient scratch, invalid clock, nonfinite creation time, broken
list head and a full30-body list. None dispatches resource callbacks. Full-list
failure retains source deletion/model-retention marks, while preflight failures
leave those marks untouched. Three invalid motion callback results exercise
errors after allocation. All expose the partial owner; the death-motion error
occurs before corpse-list insertion, while drop/carry errors occur afterward.
No later source-effect callback is dispatched after those errors.

This establishes the error boundary; it does not implement concrete resource
unwind. A live allocator must track construction progress and release acquired
resources appropriately. The normal corpse deleter requires a valid corpse-list
entry and must not be called blindly for the pre-insertion partial owner.
Actual resource binding and cleanup remain open. These are compiled-code tests,
not native XEMU gameplay; no game-runtime behavior changed in this step. All13
CTests pass, and the updated PC probe compiles successfully.


## Owned corpse physics body

rf_corpse_body_open reconstructs the 486da0 -> 49ec90/49f010 path for
corpse constructor seeds (flags 0x33/0x73, no geometric model). Source
word_0c and word_14 carry binary32 response coefficient and mass. Elasticity,
friction and density come from material index 0, not the source class material.
Nonpositive mass with source spheres uses the reconstructed mass/tensor
calculation. An empty list gets one centered fallback sphere and an identity
tensor; nonpositive mass then becomes density * radius * radius. Positive
inherited mass with existing spheres retains the zero seed tensor.

The adapter owns one copied sphere allocation and uses the existing physics
body closer. It rejects nonempty owners and insufficient budgets before
preparation. Accounted Xbox storage is 324 body bytes plus 24 per sphere,
excluding allocator overhead. The fallback sphere's undefined original opaque
word is deliberately zeroed.

python tools/verify_corpse_body.py compares all 308 represented state bytes and
owned sphere records against complete original 49ec90/49f010 execution across
640 cases: zero to eight spheres, inherited/generated mass, both corpse flags,
and two orientations. PC and compiled NXDK match. Xbox tests also cover 640
allocation failures, 640 short budgets, reopening rejection, source overwrite
and repeated close. Original heap functions are supplied by the harness;
this is Unicorn execution, not a native XEMU gameplay result.

The new harness initially used an invalid empty-vector pointer and an ambiguous
map-symbol lookup that matched apu_sge_free instead of free. Both are fixed;
the existing physics-body verifier now also requires the symbol's leading
whitespace. Its 480 original/PC/NXDK cases, 200 allocation failures and 62
nonfinite guards pass against the current binary. PC and Xbox builds succeed,
and all 13 CTests pass. Live corpse allocation/model ownership, partial-owner
cleanup and finalization dispatch remain open; this adapter is not yet bound
to the scene.


## Concrete corpse/body storage

rf_corpse_owners combines the existing 30-slot allocation metadata with actual
corpse records and owned physics bodies. Fresh initialization requires a budget
covering the entire caller-owned container. Acquisition reserves a slot and
opens its copied sphere allocation; failed body preparation returns the slot
without publishing an index or charging sphere bytes. Recycle closes that body
and returns its slot in LIFO order. Corpse payload is deliberately retained,
matching the original pool's storage behavior. The allocator callback still
must initialize/register the base owner; recycling requires all other resources
and list memberships to have already been retired. Initialized pool internals
must remain intact, and caller outputs must not alias owner storage.

Xbox fixed storage is 18144 bytes for metadata, 30 corpse/body records and
accounting. Additional live sphere payload costs 24 bytes per sphere; allocator
overhead is excluded. This accounts for embedded bodies once, rather than
charging their size again on each acquisition. No scene reserves this container
yet, so this is a storage implementation, not a claim of live corpse support.

python tools/verify_corpse_owners.py runs 64 fill/drain cycles and 1921 successful
acquisitions in both PC and compiled NXDK code. It checks full-pool rejection,
slot order, retained corpse payload, independent sphere ownership, exact/short
budgets, repeated-release rejection and reuse after failure. The Xbox harness
supplies malloc/free, checks every live allocation and injects allocation
failure. One-sphere peak accounting is 18864 bytes and returns to 18144 after
all releases. The 640 original/PC/NXDK body comparisons still pass. Both builds
succeed and all 13 CTests pass; existing unrelated PC compiler warnings remain.
No native XEMU run or visual change is claimed for this step. Base registration,
model ownership and cleanup after later constructor effects remain open.


## Registered corpse base allocation

rf_corpse_base_acquire now obtains an actual owned body/slot, inserts the corpse
in the shared object registry, and appends its object link. Its corpse link
remains unlinked until416940 reaches that construction stage. This implements
the represented type7 base fields for the no-model descriptor and flags0 case,
after the caller has accepted room placement. Health becomes100, object flags
become06400000, model is cleared, attachment index becomes-1, and position,
basis, physics flags and bounds radius are copied into the base owner.

Full original486da0 execution establishes a distinction at radius zero:
object/model radius becomes1 for seed radius<=0, but the physics descriptor is
changed only for radius<0. The shared adapter preserves this distinction and
normalizes a local seed copy. Original base allocation leaves the corpse sound
word2cc untouched; shared allocation likewise preserves it and the remaining
constructor tail. It does not invent a sound-id reset for reused storage.

python tools/verify_corpse_base_original.py executes complete486da0 including
real487100,48b870, registry writes and49ec90/49f010. Hooks supply heap, parent
lookup, string assignment and room attachment; room search is disabled and
there is no model descriptor. The room hook uses the verified ret4 convention.
Across256 cases it verifies empty/nonempty object list insertion, registry
slot removal, generation wrap, base flags, radius normalization and retained
sound. Sphere counts0/1/2/4 and mass0/3 exercise both body preparation paths.

python tools/verify_corpse_base.py compares the represented base fields,
position/basis, all308 body state bytes and sphere records with PC and compiled
NXDK. It additionally verifies256 exhausted-registry cases and256 injected
heap failures without a published index, registered object or leaked body.
For recoverable shared allocation errors, body acquisition precedes registry
insertion so no handle generation is consumed. This failure behavior is a
bounded-resource adaptation, not a claim about original out-of-memory behavior.

Both builds and all13 CTests pass. This is compiled-code verification, not
native XEMU gameplay. Live room attachment, owned names, inherited parent
metadata, models, post-construction cleanup and finalization dispatch remain
open. Bare pool recycle must not be used to destroy a registered live corpse;
normal final deletion must preserve the existing verified deletion order.


## Retained room binding

The base verifier now executes48a160 directly, both in487100's initial null-room
assignment and486da0's final room attachment. Instructions48a166..48a17b copy
object3c into the query-position cache at object4, then assign the room token
at object0. This does not allocate or link a room-list node.

rf_corpse_owned now retains an rf_entity_room_state beside the body. Base
acquisition accepts the caller's resolved room token and records it with the
query position and final object flags. The256 original/PC/NXDK cases exercise
room tokens0/1/2 and compare all20 represented room-state bytes in addition
to the prior base/body outputs. The original room function is no longer a
supplied hook. Actual containing-room lookup and subsequent moving-corpse
refresh are still caller work; refresh must synchronize flags with the corpse
owner, as in the existing actor room adapter.

This adds20 bytes per slot. Current Xbox owner-pool fixed storage is18744
bytes, and the one-sphere-per-body test peaks at19464 bytes. Earlier18144-byte
figures above describe the prior layout. The64 fill/drain cycles and1921
acquisitions continue to pass with the expanded records. No native XEMU or
visual change is claimed; names, models and full cleanup remain open.
Both builds succeed and all13 CTests pass for this layout.


## Owned corpse names

rf_corpse_owned now retains separate object and death-name allocations. The
bounded rf_corpse_name_assign adapter follows4ffa80,4ff380 and4ff280: exact
self-assignment does nothing, equal-length assignment reuses the allocation,
length changes free before replacing, and empty/null names retain no heap
allocation. Borrowed source text is copied including its terminator. Interior
source aliases into the destination allocation are outside the API contract.

Name bytes participate in the same pool budget as sphere bytes. Insufficient
final-live budget preserves the old name. If replacement allocation fails after
the old buffer is freed, the shared name becomes empty with corrected accounting;
this is a recoverable error instead of the original's unchecked null allocation.
Clearing a name requires no allocation. Recycle rejects retained names so the
two original deletion boundaries must explicitly release them before slot reuse.
The constructor/deleter resource callbacks are not yet connected to this adapter.

python tools/verify_corpse_names.py executes complete original4ffa80 with its
real4ff380/4ff280 and string copy, supplying only malloc/free. Across1024
assignments to the two fields, PC and compiled NXDK match lengths, nullness,
text and retained-byte totals. NXDK also matches exact allocation/free sizes
and order. Tests cover empty/null/self/equal-length/replacement assignments,
source-buffer overwrite, budget rejection, failed replacement allocation and
recycle rejection before names are released. No original OOM equivalence or
native XEMU gameplay is claimed.

The two eight-byte Xbox name records add480 fixed bytes across30 slots. Current
fixed owner storage is19224 bytes, with a19944-byte one-sphere-per-slot peak
when names are empty. Name payloads add their length plus one terminator each.
The64 fill/drain cycles and1921 acquisitions pass with this layout. Names are
owned storage now; binding them to real constructor and deletion effects,
models, live room lookup and complete failure cleanup remain open.
The256 registered-base comparisons, both builds and all13 CTests pass.


## Concrete deletion resource bridge

rf_corpse_owned_delete now connects the registered owner container to the
existing verified rf_corpse_delete sequence. It frees the death name at STRING,
closes and uncharges sphere storage at PHYSICS, frees the object name at
OBJECT_STRING, and returns the pool slot at RECYCLE. Embedded body records
remain included in fixed storage. External backends handle only collision
pairs, burn, model, emitter and sound resources; they must not also free the
intercepted resources or mutate owner accounting, registry or lists.

The bridge validates the selected live slot and its registered/update owner
references before dispatch. It does not access the corpse after recycling.
This applies to fully constructed owners; partial-construction unwind remains
separate because a corpse link may not yet exist. Required name clearing is
allocation-free, and the intact-owner budget invariant makes those callbacks
infallible during deletion.

The PC fixture uses actual base registration, owned body and both name
allocations. It checks256 deletion variants, resource visibility at forwarded
callbacks, reentrant rejection, model retention, saved-next emitter traversal,
registry removal, repeated rejection and restoration of fixed memory accounting.
It is now registered as the corpse_owned_deletion CTest.

The compiled NXDK harness records actual heap frees alongside external effects,
then watches the pool live-count write that recycles the slot. A memory hook
rejects any subsequent read of that owner's storage. It checks handle removal
only after recycling, poisoned released emitter records, stale-handle rejection,
sound marking and exact final accounting. Model/burn/emitter/pair/sound resource
backends are supplied. The original1024-case complete deletion verifier also
passes, independently establishing the ordering used by this bridge. No live
scene dispatch or native XEMU gameplay is claimed.
The256 PC/NXDK owned-deletion cases, both builds and all14 CTests pass.


## Owned constructor bridge

rf_corpse_owned_create binds416940 to the concrete registered base allocator
and owned death name. rf_corpse_create_ownership supplies the pool, registry,
object list/count, resolved room and material0 coefficients. External allocate
is ignored; model, motion, snapshot/pose/play/collision/source effects and emitter
callbacks retain their previous contracts. The NAME boundary is handled by
rf_corpse_name_assign and is no longer forwarded externally.

The shared constructor body now has a private optional checked-name assignment.
The existing rf_corpse_create entry keeps its original infallible callback API
and behavior. The owned entry returns a name-allocation error immediately at
that boundary, before emitter creation, corpse-list insertion, retention,
collision registration or final source effects. It exposes the registered
partial owner. Base allocation failure returns its resource status with no
owner. Source deletion/model-retention marks remain set in both cases.

The PC corpse_owned_creation CTest runs256 real owned construction/deletion
cycles, forwarding only the remaining external backends. It also exercises a
body budget one byte short and a budget that fits the body but not its death
name. The latter leaves an unlinked partial corpse with its registered body,
and no later effects run. Test teardown is explicit fixture cleanup; general
partial-owner unwind is still not implemented, and normal deletion must not
be blindly used before corpse-list insertion.

The1024 full original/PC/NXDK constructor comparisons still pass, including
7431 resource calls. The owned lifecycle harness checks actual base, name and
list ownership, forwarded construction events, deletion frees, registry-after-
recycle ordering and final accounting. Model/motion/emitter and other effects
remain supplied test backends. This does not enable live scene death or claim
native XEMU gameplay. Real resource backends, room lookup and partial cleanup
remain open before finalization can use this path.
The256 owned PC/NXDK cycles, both builds and all15 CTests pass.


## Staged construction recovery

Owned construction now records progress outside the original corpse payload:
base, allocated, model assigned, tail initialized, corpse linked, complete.
The existing generic constructor receives no progress callback, preserving its
original output and callback order. rf_corpse_owned_abort accepts only an active
incomplete owned construction. It validates the registered owner, object link,
phase-appropriate corpse link and bounded emitter traversal before effects.

Recovery clears the death name, releases any newly initialized burn owner,
unlinks the corpse only if insertion occurred, closes physics, releases the
assigned model and acquired emitters, clears the object name, unlinks the base
object, recycles storage and finally removes the registry handle. The same
resource backend contract applies: model/burn/emitter cleanup is infallible
and must preserve the remaining owner/list/registry state until recycle.

An early invalid death-motion result can leave an old burn token in reused
storage because416940 has not reset that field yet. Recovery checks the stage
and does not release that stale token. Incomplete construction has not assigned
a corpse-follow sound or reached collision registration, so it does not mark
that incoming sound or retire nonexistent pairs. Existing source deletion/
model-retention marks and fades applied to other corpses are not rolled back.
This is port error recovery, not a recovered original gameplay function.

The corpse_owned_abort CTest and compiled NXDK harness cover256 cases spanning
invalid death-motion lookup, failed name budget, invalid drop/carry lookup and
rejection of abort on a completed owner. Acquired model/emitter callbacks,
exact heap frees, registry-after-recycle ordering and final accounting are
checked. Stale handles and repeated/reentrant cleanup reject; old burn/sound
fields are preserved when not acquired. The Xbox memory hook rejects owner
reads after recycling. The earlier PC name-budget test now uses this real
abort API instead of manual fixture teardown.

Current Xbox fixed owner storage is19344 bytes (640 per slot plus metadata),
120 bytes more than the prior layout. Names and sphere payload remain charged
separately. The1024 original/PC/NXDK constructor cases and7431 resource calls
still match. Both builds and all16 CTests pass. Model/burn/emitter resources
remain supplied test backends; real resource integration and room lookup are
still required before live scene death or native XEMU gameplay is enabled.


## Independent mutable pose handoff

Current NPC pose records borrow matrices/stamps from level-owned contiguous
arrays. A corpse must not depend on those mutable actor cache slices after
actor retirement or reuse. rf_entity_pose_take now moves playback/controller
ownership to rf_entity_owned_pose while copying only matrices and cache stamps
into one independent allocation. Shared animation reference counts do not
increase or decrease during the move. The source becomes consumed (skeleton
UINT32_MAX, initialized empty playback and invalidated cache stamps), retaining
its original backing pointers so level teardown can still free its arrays.

rf_entity_owned_pose_close releases the moved active motion references exactly
once through the existing pose-release semantics and then frees the new cache.
Other actors sharing registrations retain their references. Invalid reference
state leaves the owner intact for recovery; empty close is repeatable. Allocation
and budget failures preserve source, destination and counters. Immutable bones,
geometry, materials and motion catalogs remain level-owned and must outlive
the moved pose. Their memory is not duplicated by this helper.

This is a port storage adaptation for the previously verified source-to-corpse
model-instance transfer. It is not complete reconstruction of502880/502b10,
and it does not yet connect model tokens or retirement dispatch in the scene.
The existing level arrays remain resident and separately accounted. New Xbox
storage is308 owner bytes plus50 bytes per bone, at most2808 for50 bones.

The owned_model_pose CTest and tools/verify_owned_pose.py exercise150 transfers
(1..50 bones,0/8/16 active clips) on PC and compiled NXDK. Checks include exact
playback/controller/cache copying, overwritten old backing buffers, unchanged
references during transfer, one decrement on close, preserved other-owner
references,150 short budgets and150 injected Xbox allocation failures. Invalid
close references preserve the destination; repeated close performs no free.
No native XEMU run or visual change is claimed for this storage step.

Validation: full Release PC build and all17 CTests pass; NXDK build is
current. The compiled Xbox pose verifier passes150 transfers,150 short-budget
cases and150 injected allocation failures. Xbox executable SHA256:
41e9e19bcb2ff4cd7cd43406ad2b41a4e4b23add0d66f191fb45380ad825adb3.


## Retained authored corpse model and emitter metadata

rf_entity_corpse_config_read now retains selected-class replacement model,
emitter name and emitter lifetime; rf_entity_seeds_open copies this into each
owned class record before closing its table scratch. This supplies previously
missing inputs for live416940 binding, rather than assuming every class keeps
its source model. It does not load a replacement model or instantiate emitters.

Static original evidence:41bb32 tests "$Corpse V3D Filename:" and41bb53..41bb5b
assigns the empty string when absent.41bb60 initializes emitter ID to-1;
41bb6a initializes lifetime to binary32 -1.41bb63 tests "$Corpse Emitter:";
41bb93 resolves its quoted name through497550. The nested41bba1..41bbb6 reads
"$Corpse Emitter Lifetime:" only when the emitter field exists. The port keeps
the name instead of inventing an emitter ID; runtime resolution remains open.
The reader requires unique, ordered corpse fields, quoted names up to63 bytes
and finite lifetime. These bounded parsing rules are port validation, not a
claim to reproduce the full original parser's behavior on malformed text.

The installed table has63 classes and two authored replacement models:
Stationary Turret uses sentry_turret01_dam.v3d; Stationary Turret_Plain uses
sentry_turret01_dam_plain.v3d. No installed corpse emitter declarations occur.
Other classes default to an empty replacement model and emitter, lifetime-1.

python tools/verify_corpse_config.py passes81 PC/NXDK cases: all63 installed
classes plus18 cases covering defaults, populated/empty names, field ordering,
duplicates, name limits, comments, class boundaries and invalid/nonfinite input.
Failure preserves the destination. The original parser itself is not executed;
tag/default evidence above is static disassembly. Three retained PC level
checks (L1S1/L1S2/L1S3) compare metadata after archive closure and exercise exact
and insufficient budgets. New storage is132 bytes per distinct class, charged
through the existing sizeof-based class allocation:660/396/792 bytes for those
levels. Full PC and NXDK builds and all17 CTests pass. No new XEMU gameplay or
visual result is claimed. Finalization418f80 and live model/emitter bindings
remain incomplete.


## Full original ordinary-SP finalizer execution

python tools/inspect_death_finalizer.py now executes complete418f80 through
return in1024 controlled ordinary-SP cases. Both network globals are zero.
This is original executable evidence for the next C reconstruction, not an
implementation or live integration of the finalizer. Original classification,
actor and object handle lookup, indexed attachment-list reads, player detach,
string construction/copy/free and support orientation math execute unchanged.
Heap allocation, damage, attachment mutations, explosion, region/probe/area,
corpse construction, drop, burn effects and the final predicate are supplied
boundaries. Callback traces expose those substitutions explicitly.

Verified sequence and conditions:
-48ab40 marks object7c bit2 before any remaining effects; no registry removal.
-Type1 actors damage matching attached player actors by10000 with kind3.
-Flag7d0 bit100000 damages a resolved parent by1000 with kind-1. A resolved
 parent independently triggers4279d0. For each retained child, flag100000
 clears health before427380(child,1); player detach follows child handling.
-Class6f8 !=-1 clears action824 before419420 (death explosion/effect).
-Flag810 bit80 skips corpse creation. Otherwise an action other than-1 or a
 nonempty replacement corpse model is required. A valid action mapping supplies
 the copied death-name string; no mapping leaves it empty.
-Region flag2 suppresses creation. Movement kind10 bypasses the support probe.
 Other kinds probe from position+(0,.5,0) to position-(0,1.5,0). In ordinary SP
 a miss (fraction>=1) still permits creation. A hit whose handle resolves to an
 object suppresses creation. A static hit permits it regardless of alignment.
-Static alignment additionally needs normal.y>.5, a nonnull face, and face
 area>1. Real40caf0/4fab70/409f40 update the three basis vectors; real417d80
 copies the68-byte contact record into actor1b4. The fixture checks exact basis
 and support bytes for its axis-aligned normals, including the strict boundaries.
-Creation gets(source,name,position,basis,0,0). On success flag810 bit4000000
 invokes4174f0 and then clears bit200 from the current flags (reread after the
 callback). A source burn then retargets to the new corpse handle. Corpse2d0
 receives the current source13d8 after that callback, not a cached token.
-Successful burn transfer deliberately leaves source13d8 intact. Without a
 transfer, the tail releases a remaining burn and clears source13d8. The final
42a8e0 predicate executes even in ordinary SP; its multiplayer action does not.

Counts:118 constructor calls,43 drops,68 burn transfers,614 burn releases,
137 support probes,53 resolved-object rejections,11 surface alignments,
768 attached-actor damage calls and512 explosion callbacks. The harness checks
whole-source preservation outside the identified write locations, child health
at callback entry, player detach byte preservation, temporary string frees,
creation failure cleanup decisions, and post-callback flags/burn rereads.
The probe initially sets fraction+18=1, handle+30=-1 and word+38=0; face+3c is
not initialized there. The supplied probe fills every result byte; a future
port query must not treat the uninitialized original bytes as semantic defaults.

Report: artifacts/death-finalizer-original.json. This establishes execution
coverage beyond the earlier static export. It does not verify the supplied
resource effects, all attachment mutation/reentrancy scenarios, arbitrary
surface geometry, multiplayer branches, or a C/PC/NXDK finalizer implementation.


## Shared ordinary-SP death finalizer

rf_entity_finalize_sp now reconstructs full418f80 ordinary-SP orchestration in
shared C. It marks source deletion, handles attached player/parent/child actors,
detaches a resolved player, invokes the class death effect, gates and places a
corpse, applies the optional drop and transfers or releases burn ownership.
Resource operations remain explicit backend boundaries. No live scene binding
or immediate source registry removal is introduced. The existing dying-update
caller must keep the actor alive for its subsequent endgame lookup.

The backend maps original counts/handles, typed actor resolution, action-name
mapping, region flags, support probe/face area, resource effects and416940
construction. A returned corpse uses its real shared deletion handle/burn
fields. Backend calls reread flags and source burn after mutation. Child health
is zeroed before the detach callback where required. Counts are queried again
while iterating, retaining the original indexed-list traversal semantics.
Callers must supply valid bounded lists whose traversal terminates.

Static-hit alignment is implemented in C, including cross products, original
zero-length normalization fallback, new forward/up vectors and a68-byte support
record copy. A miss still permits an ordinary-SP corpse. Resolved-object hits,
region flag2, disabled-corpse bit80 and missing action/replacement model retain
the verified gates. The local death-name copy is bounded to63 bytes; excess
length or nonfinite geometry/query arithmetic returns RF_RANGE. These are
explicit port validation limits. Errors after earlier resource effects do not
roll them back. The probe must fill its complete record when reporting a hit;
unused initial stack bytes are normalized in the port, not treated as original
semantic defaults.

python tools/verify_death_finalizer.py passes1536 original/PC/NXDK cases with
exact state and normalized resource-call traces. The original harness now adds
512 varied surface-normal/basis cases, including a zero-cross-product fallback.
All523 aligned cases agree byte-for-byte on resulting basis and support record.
Across the full suite there are630 constructor calls,262 drops,362 burn
transfers,662 releases,649 probes,53 resolved-object rejections,896 attached
actor damage calls and512 class death effects. A region callback overwrites
the authored action name after selection; construction still receives the
pre-query name in all three implementations. Drop/retarget callbacks also
mutate live flags/burn to verify post-callback reads.

Both full Release PC and NXDK builds succeed; all17 CTests pass. Compiled Xbox
SHA256:6b871e1bc0acc80887a00a776a016ddf8c8ad8309e541a889b81301db2173208.
Report: artifacts/death-finalizer-verification.json. This proves orchestration
with supplied resource backends, not their complete implementation. Actual
model, emitter, geometry, attachment/explosion bindings and deferred deletion
still need integration before enabling live finalization. Multiplayer branches,
all list mutation/reentrancy scenarios and native XEMU gameplay remain outside
this verification scope.


## Finalizer callback bound to concrete corpse creation

rf_entity_finalize_create_owned now implements the finalizer create callback
using rf_corpse_owned_create and staged rf_corpse_owned_abort. Its zero-initialized
binding contains the retained source constructor fields, ownership context,
request timing/scratch, corpse list/count and construction/deletion backends.
Finalizer and constructor handles must match. The callback copies current
position/basis and death name into a local request, forces both original zero
flags, synchronizes source object/810 flags before and after construction, and
leaves the request template unchanged. Other finalizer callbacks may wrap this
binding in a larger context. This adapter is not yet wired to the scene.

Only a successful complete corpse is returned to the finalizer. A failed
partial constructor is cleaned through the staged abort API before NULL is
returned, preventing burn transfer to an incomplete corpse. status preserves
the construction error; cleanup_status preserves a failed abort and partial
retains that unresolved owner for explicit recovery. Reusing a binding with an
unresolved partial is rejected without dereferencing or replacing that owner.
Earlier source deletion/ownership flags are not rolled back. Model, emitter
and other resource release effects still use supplied backends.

python tools/verify_finalize_owned_create.py passes256 PC/NXDK creation/deletion
cycles through the adapter, plus two memory-budget failures before body
allocation and during owned name allocation. Both failures restore fixed pool
accounting, object/corpse counts and registry capacity. The latter executes
actual staged abort, body free, recycle and registry removal. Native memory
hooks reject owner reads after recycle. Existing release ordering, stale-handle
rejection, emitter poisoning and repeat deletion checks remain active. Transform
copying, source flag synchronization and unchanged request templates are checked.
The Xbox harness additionally confirms unresolved-partial reuse is rejected.
These tests do not inject an abort failure or exercise live NPC finalization.

Full PC/NXDK builds and all18 CTests pass, including finalizer_owned_creation.
Xbox SHA256:37cc742975c0d440b8612e8efc435dc71c86ee963c1ad1d175d065983d49cdcc.
Report: artifacts/finalize-owned-create-verification.json. Real model token/pose
transfer, scene ownership, burn/player/query bindings and deferred actor deletion
remain open before native gameplay can use the completed-death path.


## Original model-instance release ordering

Full502b10 delegates to5028f0. python tools/inspect_model_release.py executes
both wrappers, the scalar/vector deleting destructors and actual504800 pool
recycle across144 combinations. Types0/1/2/3/4/ffffffff, absent/present payload,
absent/present auxiliary array and counts0..5 are covered. The type-specific
51b070/54b570 payload destructors and54a8a0 auxiliary element destructor are
supplied boundaries; allocator free is recorded and poisons released storage.
This does not prove complete resource destruction or a live model backend.

Type2 with nonnull word4 invokes504600(payload,1), which runs51b070 then frees
the payload. Type3 uses504620/54b570 similarly. Only those successful dispatches
clear model word4, after the deleting wrapper returns. Other types leave word4
unchanged even if nonzero. A callback deliberately overwrites word4 to confirm
the outer clear occurs after it, while free still uses the original payload.

Nonnull model word50 invokes504480(array,3). The count is stored atarray-4;
real57377d walks200-byte records in reverse order through54a8a0, then the
wrapper freesarray-4 even for count0. The pointer at model50 is not cleared.
Its semantic role remains unidentified: auxiliary array is a structural name,
not a claim that these are animation attachments. There are180 verified element
calls and24 submodel releases in this matrix.

Finally504800 recycles the model through pool0173c3f0. It sets pool+157c0 to
the released model, overwrites the model's first word with the previous free
head, increments pool+157cc and decrements pool+157d8. Other inspected model
bytes survive. A memory hook rejects any subsequent original read from the
recycled model. This proves why a future port model resolver must invalidate
ownership before reuse and must not inspect the owner after recycling it.

Static follow-up evidence:51b070 clears three action indices and drains active
motions through51c090 only when payload1d50 is nonzero, then unlinks its ring.
54b570 dispatches four optional array destructors at offsets14/18/1c/20.
54a8a0 conditionally frees payloads80/bc/c4 and destroys two inline34-byte
records. Those bodies still require execution verification and concrete port
bindings; rf_entity_owned_pose_close alone is not the full model destructor.
Report: artifacts/model-release-original.json. No C runtime or XEMU gameplay
change is claimed for this original-code ownership audit.


## Shared model release dispatch and identified material array

rf_model_release now reconstructs502b10/5028f0 resource dispatch in shared C.
Kind2/3 with a payload invokes its deleting backend, then clears the payload.
Other kinds preserve the token. A nonnull material-array token invokes its
release backend without clearing the token. The recycle callback runs last and
may invalidate the entire owner; the function performs no later owner read.
This is dispatch over resolved tokens, not yet the live model registry/backend.

The earlier unidentified200-byte auxiliary array is the model material array:
the existing54a7c0/503950 reconstruction in docs/MODEL-MATERIALS.md uses that
record and the same80/bc/c4 array fields.54a8a0 is its destructor. The original
harness now executes that complete body instead of substituting it. With flags
bit0 set, nonnull arrays80/bc/c4 are freed in order; otherwise they remain owned
elsewhere. Real57377d visits material records in reverse and frees the outer
array cookie afterward. Both inline34-byte destructor calls execute54a620,
which is a bare return; no shared texture-release effect occurs there.
The three owned material arrays already have a combined-storage port adapter
in rf_model_material_instance, while campaign textures remain level-owned.

python tools/verify_model_release.py passes144 original/PC/NXDK cases. Checks
include kind/payload gates, callback order, payload mutation followed by the
outer clear, unchanged material token, and a recycle callback that poisons all
owner bytes. Native memory hooks reject any owner read afterward. The original
reference also verifies actual material frees, array order, pool counters and
unchanged surrounding bytes; skeletal/static payload bodies remain supplied.
The C dispatcher still requires concrete payload/material/recycle backends.

Full PC and NXDK builds and all18 CTests pass. Xbox SHA256:
9c76ab4d31f41de8f88d737e3abd2e103079be64fc019f8722c0457f6da04797.
Report: artifacts/model-release-verification.json. Pose transfer, mutable
material ownership and live model token resolution remain to be connected;
this step does not add live corpse rendering or claim new XEMU gameplay.


## Complete original skeletal payload retirement

python tools/inspect_skeletal_release.py executes full51b070 with real51c090
slot removal and539d70 reference decrement: no callee substitutions. Across680
cases it covers active counts0..16, unique/duplicate motion IDs, zero/positive
reference counts, rings of1..4 owners and every removal position. Whole victim
and surviving owner records, all motion counters and the global head are checked.
There are2720 actual active-slot removals.

A null payload1d50 skips all writes, including list unlink. Otherwise1cfc/1d00/
1d48 become-1 before repeatedly removing the first active motion. Reference
counts saturate at zero; the original shifted unused slot tail is retained.
Then the owner is removed from the circular list headed by0181bdb8. Removing
its head advances to the next owner, or clears the head for a singleton. Both
victim links1d54/1d58 become zero and neighboring links are repaired. Descriptor
1d50 is not cleared. The payload destructor itself does not free storage;
504600 performs that afterward. Do not treat this original destructor as a
repeatable close operation merely because links become zero.

The existing rf_motion_remove_slot already reconstructs the slot/refcount
helper (see docs/CAMERA.md). rf_entity_owned_pose_close is a port storage
adaptation that releases valid unique active references and frees its cache;
it is not the original global registration-list retirement. Live corpse pose
transfer must also preserve correct model registration/update traversal before
source actor slots are reused. The explicit per-level arrays may implement
that ownership differently, but leaving transferred poses unreachable by
updates would not reproduce the original behavior.
Report: artifacts/skeletal-release-original.json. This is original-code evidence,
not a new PC/NXDK skeletal registry or live corpse scene binding.


## Shared skeletal retirement

rf_model_skeletal_retire now reconstructs51b070 over a borrowed active-slot
state, loaded token and circular registration node. A zero loaded token skips
other inputs. For a loaded node it validates bounded ring membership/coherence,
slot count, resource IDs and nonnegative reference counts before mutation.
Selected slots are then reset to-1, active entries drain through the existing
rf_motion_remove_slot, and the victim is unlinked with the original head-update
semantics. Duplicates and zero references follow original saturating decrement;
unused slot tails remain intact. The loaded token is retained. This routine
does not free pose caches or the enclosing model; those follow retirement.

The ring/node/head, borrowed active state and resource counters must be valid,
disjoint and exclusively owned for this operation. An unlinked loaded node
cannot be retired again. Pointer validity is a caller contract; structural
checks do not make arbitrary addresses safe. No runtime registry population or
scene pose transfer is introduced by this API alone.

python tools/verify_skeletal_release.py passes680 original/PC/NXDK cases with
exact active-slot bytes, counters and normalized links/head. On compiled Xbox,
340 insufficient ring limits,320 invalid motion IDs and340 repeat retirements
preserve the owners/counters and reject the operation. Original51b070 and its
slot/reference callees execute without substitutions. Both full builds and
all18 CTests pass. Xbox SHA256:
b44f8b7fe4b2a45cb348abc71cad5432de0a2dc766c015e78b7858c34d02ba97.
Report: artifacts/skeletal-release-verification.json. Live registry, owned pose
cache release and material ownership integration remain open.


## Shared skeletal registration

rf_model_skeletal_register reconstructs the51ae90 constructor's ring-publication
tail for an already initialized loaded owner. Empty lists become a singleton.
Otherwise the new node is inserted between the old tail/head and becomes the
new head. Borrowed pose/active state is unchanged. A bounded preflight rejects
unloaded/uninitialized nodes, nonempty node links, duplicate membership and
broken rings; the visit limit bounds total ownership after publication. This
is not the complete constructor or a substitute for model/resource loading.

python tools/verify_skeletal_registration.py runs full original51ae90 without
substituted callees, then compares registration head and links with PC/NXDK for
0..32 existing owners. All33 cases agree.33 insufficient-limit and33 duplicate
attempts preserve node/head/pose state on compiled Xbox. The original constructor
also initializes other model data, which is deliberately outside this helper's
comparison. Both builds and all18 CTests pass. Xbox SHA256:
12e173e8360156627683ae61b5c582498d8954ba4987696b6f7fe43d49af8eea.
Report: artifacts/skeletal-registration-verification.json. Shared register and
retire operations are now available; live registry population, pose transfer
and material/resource lifecycle integration remain open.


## Live campaign skeletal registry

Campaign startup now publishes each loaded NPC pose after initial playback is
initialized. Registration storage is separate from NPC physics owners, with
one20-byte entry per authored pose slot on PC32/Xbox and a32KiB allocation
limit. Each entry borrows the pose and its active motion list. Level teardown
retires registrations before releasing physics, pose caches and shared playback
resources. Retirement drains motion references; the subsequent pose release
resets cache stamps with an empty active list. Partial startup also retires
already-published entries. NPC_MODELS reports published, peak allocated bytes,
retired and errors, retaining cleanup results after the table is freed.

python tools/verify_campaign_model_registry.py passes the L1S1/L1S2/L1S3 PC
replays:78/38/25 loaded owners publish and retire with zero errors, using
1560/780/560 bytes for78/39/28 authored slots. All18 CTests pass.
Report: artifacts/campaign-model-registry/report.json. The registry currently
borrows level poses; corpse transfer, render/room lookup redirection and model
material lifetime integration remain open. This is not live death completion.

Native XEMU replay-20260911-214300 passes180 frames with stock67108864-byte
RAM, door/damage/audio fixtures and direct guest-memory NPC_MODELS comparison:
[78,1560,78,0], matching PC. Existing body, animation, damage and audio checks
also pass. The harness now checks registry lifecycle on campaign replays.


## Registered pose ownership handoff

rf_entity_registered_pose_take composes the port-owned pose transfer with an
existing skeletal registration. It verifies loaded status, source active-list
identity and local ring reciprocity before allocation. On success the published
pose pointer and active-list pointer both switch to owned storage, preserving
links and model identity. Failure preserves the registration and source. This
is a port storage adaptation, not a new original-executable reconstruction.
The caller maintains the complete valid ring and must retire the node before
closing its owned pose. Immutable level resources must outlive both operations.

python tools/verify_owned_pose.py --registered passes150 PC/NXDK cases over
1..50 bones and0/8/16 clips. Compiled Xbox additionally passes150 allocation
failures and150 short budgets. Tests poison former caches, verify pointer/link
identity and check retirement plus repeated owned-close releases each reference
once, preserving other borrowers. The original unregistered150-case verifier
also passes. Both builds and all18 CTests pass. No live corpse/XEMU claim:
scene rendering, residency, eye and pain consumers still use actor pose slots;
redirect their ownership before activating the corpse-creation binding.
Report: artifacts/registered-pose-verification.json.


## Live registered pose consumers

Campaign playback, residency, eye placement, pain reactions and drawing now
resolve their pose through campaign_model_pose. The lookup checks registration
state and active-list identity, skips unloaded authored slots and rejects
inconsistent loaded owners. Initial construction/digests and level-array cleanup
retain their source-array access. This removes live pose-storage dependence
on the original actor slot; transforms, appearance and room indices are still
actor-owned and require corpse bindings before runtime ownership can change.

The residency unit fixture now publishes its model. Its eye test transfers
registered ownership, poisons the former matrices, verifies the same eye position,
then retires and closes the moved pose. All18 CTests and three campaign registry
replays pass. Both PC and NXDK builds pass. Live corpse creation is still open.

Native XEMU replay-20260911-215023 passes180 frames on stock64MiB with
door/damage/audio fixtures and matching PC telemetry, including model registry
[78,1560,78,0]. This verifies the redirected consumers on untransferred campaign
poses; the transferred-eye fixture is PC-only.


## Model-owned render placement

Campaign model owners now retain position, basis, appearance and room in addition
to pose registration. campaign_model_place copies finite transforms and retains
material/room indices without borrowing source metadata. Drawing uses those
values; animation visibility/LOD uses the same model position and room. The
actor tick still publishes its position into the model, until corpse ownership
switches the update path. Appearance refers to shared level materials; model
retirement must not free those textures. Moving room refresh remains open.

Each model slot is now76 bytes (was20), bounded by the existing32KiB cap. The
separate4-byte-per-slot render room array is removed. Opening-level ownership
is5928 bytes, a net4056-byte increase after removing312 room-array bytes.
L1S2/L1S3 use2964/2128 bytes. All three PC registry replays and all18 CTests
pass. Placement tests overwrite source arrays and reject a NaN update without
mutating the retained owner. Comparing the door/damage PC replay to its previous
run preserves NPC playback/gate/eye/body telemetry and rendered vertex hash;
render scratch telemetry falls445192 to444880 bytes. No visible change expected.

Native XEMU replay-20260911-215423 passes180 frames on stock67108864-byte
RAM with matching door/damage/audio and registry telemetry [78,5928,78,0].
This verifies model-owned placement for existing actors, not live corpses.


## Campaign model handoff and actor separation

rf_scene_model_detach now owns the campaign transfer allocation: one empty
308-byte owner plus the existing bounded matrix/stamp allocation. It limits
transfers to30 models and96KiB aggregate owner/cache bytes, excluding allocator
overhead. Budget failure, allocation failure, duplicate transfer and nonfinite
placement preserve the registered source. Successful transfer preserves model
identity/appearance, copies accepted corpse placement and consumes the source
pose. Model slots grow76 to80 bytes to retain the owned-pose pointer.

Actor playback/snapshot, eye and pain paths now use campaign_actor_pose and lose
access after transfer. Model rendering/residency retains campaign_model_pose.
Level teardown retires the registration, drains references, closes the moved
pose cache and frees its owner, updating aggregate accounting. Individual
corpse model retirement is still needed; the function is not called by live
death dispatch yet. Transferred models must receive the corpse update path
before enabling gameplay handoff. This is port storage integration, not a new
claim of original-executable behavior.

The PC fixture verifies preserved model pose/placement after poisoning source
storage, blocked actor access, count/budget rejection and zero accounting after
teardown. tools/verify_campaign_model_detach.py executes compiled NXDK detach
and campaign cleanup for150 combinations of1..50 bones and0/8/16 clips, with
300 injected allocation failures,150 capacity rejections and150 repeats. It
checks exact ring/pointer ownership, reference counts and both freed allocations.
Report: artifacts/campaign-model-detach.json. Both builds and all18 CTests pass.

Three PC level replays pass with6240/3120/2240 registry bytes. Native XEMU
replay-20260911-220054 passes180 door/damage/audio frames on stock64MiB with
[78,6240,78,0] registry telemetry. Native campaign replay contains no transferred
model; detached ownership itself is tested in the PC fixture and NXDK harness.


## Individual campaign model retirement

rf_scene_model_retire retires one registered slot while retaining the registry
table and shared textures. It validates active-list identity and owned-cache
shape/accounting before draining references. Owned storage is closed/freed,
aggregate bytes/count decrease and the slot becomes unloaded with no pose.
Repeated retirement of an empty slot is a no-op. Actor/model lookups then return
no pose, so drawing skips it. Level cleanup uses the same retirement boundary.
The cleanup symbol is retained for compiled-code verification.

The PC fixture checks individual retirement, repeat and subsequent level close.
The compiled NXDK harness runs150 detach/individual-retire/level-close cases
with300 allocation failures,150 capacity and repeat-transfer checks,150 bad
accounting rejections,100 negative-reference rejections and150 repeat retires.
Failed retire checks preserve the model owner; successful retirement frees both
allocations and drains references once, preserving other borrowers.
Report: artifacts/campaign-model-retire.json, generated by
python tools/verify_campaign_model_detach.py. Both builds, all18 CTests and the
three PC campaign registry replays pass. No new XEMU run or live corpse dispatch
claim; those remain separate integration work.


## Owned constructor model binding

rf_corpse_owned_create_bound adds a fallible port ownership callback immediately
after the original constructor assigns its model and records stage MODEL,
before death-motion lookup and subsequent effects. Failure returns the partial
owner for rf_corpse_owned_abort, including its assigned model token. The cleanup
backend must be able to retire that token even if copying pose storage failed.
The existing rf_corpse_owned_create API delegates with no callback, preserving
its original interface and behavior. This extra callback is port integration.

rf_scene_corpse_bind_model resolves campaign model tokens as slot+1, transfers
the source skeletal pose and publishes accepted corpse position/basis/room.
Zero model is accepted. Replacement models and other model kinds are rejected
until their loaders are connected. Shared textures remain level-owned.

The PC scene fixture now runs real owned corpse construction with this binding,
then normal owned deletion whose MODEL callback retires the campaign model.
It also checks failed handoff at stage MODEL and failed name allocation after
successful handoff at stage TAIL; both abort through actual resource cleanup
with empty pool/object lists and zero owned-pose bytes. Motion/collision/source
effects are supplied fixture boundaries, not recovered live gameplay.
Both builds and all18 CTests pass. The compiled Xbox campaign binding passes
150 pose sizes/clip counts,300 allocation failures and capacity/repeat/retirement
checks via tools/verify_campaign_model_detach.py --bind. That native harness
supplies the corpse record directly; complete constructor-to-campaign binding
is currently PC fixture evidence. Report: artifacts/campaign-corpse-model-bind.json.
Live death dispatch and remaining resource effects are still open.

The existing tools/verify_corpse_owned_create.py also passes256 PC/NXDK
construction/deletion cycles and the PC allocation-boundary checks after this
API extension, including poisoned cleanup storage and no post-recycle reads.


## Original action-name lookup for corpse motions

Original428fe0 first rejects NULL query or non-skeletal/absent actor models
through40a1e0. It scans45 actor action slots: signed declaration index at
actor+a58+i*16. Nonnegative indices address class+760 declaration records
with52-byte stride.5001d0 compares the record string with the query; under
C locale its actual57c130 callee folds ASCII uppercase only. First matching
actor slot is returned, not the declaration index or model motion ID. An empty
declaration matches an empty non-NULL query. Missing entries are skipped.

rf_entity_action_name_lookup reconstructs this search over caller-resolved
45 name pointers. NULL entry means missing; empty string means an available
empty declaration. It preserves order and returns-1 for absent model, other
model kinds or NULL query. This is not an animation filename search; the
existing compiled-clip indices cannot alone establish declaration availability.
Retaining authored declaration mappings and binding the corpse motion callback
remain open, rather than assuming the fixed parser label list is equivalent.

python tools/verify_action_name.py passes814 original/PC/NXDK comparisons:
all45 positions, duplicate matches, reversed declaration storage, empty names,
NULL query/model, non-skeletal kind,63-byte names, case variants and non-ASCII
bytes. Full original428fe0/40a1e0/5001d0/57c130 execute without substituted
callees; the harness explicitly sets original C locale. Original SHA256:
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.
Report: artifacts/action-name-verification.json. No live corpse/XEMU claim.
Both complete builds and all18 CTests pass after the lookup addition.


## Retained declaration availability and corpse motion callback

The original422360 action initialization loop keeps the
419a00 declaration index in actor+a58 separately from the51cc10 clip result
in actor+a54.419a00 returns a matched declaration even when its motion filename
is empty. Thus clip index-1 is not proof that the name lookup should fail.

Base and weapon-group action loaders now retain two32-bit declaration masks,
set when a canonical action declaration is read, before checking its motion
filename. This adds8 bytes per retained class/group and no heap allocation
per action. Names reuse the45 verified initializer labels. The canonical mask
lookup feeds rf_entity_action_name_lookup; it does not claim to retain arbitrary
noncanonical declarations or weapon-switched original declaration indices.

rf_scene_corpse_motion resolves the current base class mask, rejects unbound
class/weapon mappings and returns the original action slot. The PC owned-corpse
fixture now calls it for death_generic, corpse_drop and corpse_carry, including
empty-filename declarations. Model pose/sound/collision effects remain supplied
fixtures. Live death dispatch and those resource effects still need integration.

tools/verify_action_declarations.py passes90 original419a00 declaration
selections with reversed table order and empty/nonempty filenames, and compares
the resulting canonical actor slot with NXDK mask lookup. Real500150/string
comparison executes; only4ffa20 string assignment is supplied/observed. It also
reruns the814-case original/PC/NXDK name verifier.
Report: artifacts/action-declaration-verification.json.
tools/verify_base_action_sets.py now independently checks retained masks against
authored declarations for base/weapon groups across L1S1/L1S2/L1S3, in addition
to its existing clip, cache, sound and budget checks; all pass.
Both complete builds and all18 CTests pass; no new native XEMU run is claimed.


## Corpse pose refresh through retained model spheres

Each shared render model now retains up to8 immutable collision-sphere records
and their count, loaded once with geometry. The cache costs388 bytes per model,
charged through existing model-owner budget accounting. Oversized sphere sets
are rejected rather than silently truncated; this remains a port bound. Model
archives are not read by corpse pose refresh.

rf_scene_corpse_pose resolves an owned, transferred model token and invokes
the verified4164c0 refresh using its cached matrices/sphere definitions and the
corpse body position. It updates body bounds and both represented radius fields
(original180 and78), preserving class-adjusted physical sphere radii and extra
physics spheres. The owned-constructor PC fixture now uses this actual POSE
effect, including subsequent failure cleanup. A model radius99 with physical
radius1 and posed centerX1 produces bounds/object radius2, retaining radius1.

All18 CTests and both full builds pass. The existing original/PC/NXDK
1024-case sphere verifier passes on the rebuilt Xbox binary. The skeleton
probe compares each retained sphere with its serialized source, checks the
end-of-list sentinel and exact/short allocation budgets. Three levels pass:
L1S1 five models,7 sphere records,316976 render-owner bytes; L1S2 two models,
6 records,181000 bytes; L1S3 four models,6 records,208264 bytes.
Report: artifacts/corpse-sphere-cache.json. The complete constructor/pose
connection is PC fixture evidence; live death dispatch remains open.

Native XEMU replay-20260911-222939 passes180 door/damage/audio frames on
stock67108864-byte RAM with matching PC telemetry. This covers campaign loading
of the new cache; no transferred corpse is spawned in that replay.


## Corpse sound-follow adapter

rf_scene_corpse_follow_point resolves the transferred skeletal model owner and
uses the corpse object position/basis with the verified48ac70 helper. The
no-attachment sentinel copies the corpse position without inspecting a model.
Invalid model/attachment queries preserve the destination. This allocates no
additional storage and does not read the former actor pose.

The full owned-constructor PC fixture verifies a changed retained bone and
a rotated corpse basis independently of the registered render transform.
tools/verify_campaign_model_detach.py --follow verifies150 compiled NXDK
handoffs with1..50 bones and0/8/16 active clips after poisoning source matrices,
plus invalid-attachment and absent-model fallback behavior and existing exact
retirement/heap checks. Report: artifacts/campaign-corpse-follow.json.
The original sound-follow verifier passes4096 original/PC/NXDK comparisons
and6 guards; both builds and all18 CTests pass. No new XEMU run is claimed.
Live death dispatch, sound ownership and moving a playing corpse sound remain
open; this adapter currently provides the attachment position only.


## Finalizer model-handoff boundary

rf_entity_finalize_create_owned_bound connects the finalizer creation adapter
to the existing fallible owned-model boundary. The original public adapter
remains a NULL-boundary wrapper, preserving its binding layout and callers.
The new entry retains finalizer position/basis, zero protection/seek flags,
source-flag synchronization and automatic partial-construction abort.

The campaign PC fixture now exercises both constructor and finalizer entry
paths with the real model transfer, action lookup, cached sphere refresh and
model retirement. Success preserves the detached pose; model-capacity failure
and post-transfer name-allocation failure both automatically retire the model
and leave no corpse/body/name owner or unresolved partial binding. All18
CTests and complete PC/NXDK builds pass. The finalizer path is fixture-bound;
this does not yet enable live actor death dispatch or corpse update playback.

The rebuilt compiled-NXDK finalizer adapter regression also passes256 owned
create/delete cycles and two budget-failure boundaries with supplied external
resource callbacks. Report: artifacts/finalize-owned-create-verification.json.
This preserves the existing NULL-boundary adapter coverage; the real scene
model-bound finalizer connection above is exercised by the PC fixture.


## Transferred corpse model reset

rf_scene_corpse_reset resolves a detached model token and its retained playback
resources, then invokes rf_motion_stop_looping (5033f0/501ca0/51c340). It clears
freeze state and zeroes only loop-byte1 weights, preserving active slots and
references for subsequent advancement. No allocation or actor access occurs.
The constructor/finalizer PC fixture tests mixed loop/nonloop playback after
transfer and verifies both references retire exactly once on corpse deletion.
Both builds and all18 CTests pass; the original motion-stop comparison passes
1800 cases. This is an adapter test, not live corpse update dispatch. Duration,
start/advance, sound ownership and the complete update binding remain open.


## Corpse motion start and duration adapters

rf_scene_corpse_play resolves a transferred model and model-local motion ID,
requires its clip through the existing bounded campaign residency owner, and
invokes the verified restart with weight1/freeze1 (5033b0).
rf_scene_corpse_duration uses the same owned model mapping and retained file
ticks through rf_motion_duration (5033e0), preserving output on invalid IDs.
Neither adapter consults actor action slots or copies animation payloads.

The integrated PC constructor/finalizer fixture supplies resident clip metadata
and verifies invalid IDs, duration, restart weight/freeze and unchanged active
reference counts, followed by exact retirement. This fixture tests an already
resident clip, not a real archive load or decoded motion advancement.
Both builds and all18 CTests pass. Existing original comparisons pass4096
duration cases (PC/NXDK) and6000 action-start cases. These compare underlying
primitives; the new scene adapters have PC fixture coverage. Per-frame model
advancement, transforms, owned sounds and live death dispatch remain open.


## Corpse skeletal advance dispatch boundary

tools/verify_corpse_advance_dispatch.py executes original503360/501ab0 and
observes entry to51ba80 across64 combinations of delta, auxiliary argument,
position/basis pointers and final flag. Deliberately unmapped nonzero transform
pointers prove these wrappers do not dereference them for type2. The skeletal
payload is forwarded as this; only delta, auxiliary and final flag are passed.
The check stops before51ba80 and does not claim playback or lazy pose timing.
Report: artifacts/corpse-advance-dispatch.json.

Consequently the pending skeletal corpse advance adapter must not copy its
position/basis arguments into model placement as a503360 side effect. Corpse
render placement and room membership require their separate owner update.
Playback51ba80 is already reconstructed by rf_motion_update; connecting its
transferred owner and determining when cached pose evaluation is demanded
remain the next integration work. Type3 advancement is outside this evidence.


## Transferred corpse playback advancement

rf_scene_corpse_advance resolves the transferred model and invokes the shared
51ba80 playback update with its retained per-model resources. It uses no actor
controller, does not alter world placement, allocate, or read clip archives.
Generation advances according to the verified playback rules; cached matrices
remain untouched until evaluation is requested. This is timing advancement,
not a complete render-ready corpse tick. Current sphere/follow helpers still
read cached matrices, so pose evaluation must be connected before live use.

The PC constructor/finalizer fixture advances a retained nonloop motion by
0.25 seconds to tick1200, checks generation, removes a zero-weight loop and
releases exactly that reference. Cached matrices/placement are unchanged, and
final deletion releases the remaining reference. Both builds and all18 CTests
pass. The underlying original playback comparison passes160 scenarios across
64 consecutive frames each (10240 updates); adapter coverage is PC fixture
evidence, not a native XEMU corpse tick. Live dispatch remains open.


## Explicit transferred corpse pose evaluation

rf_scene_corpse_evaluate resolves the transferred owner, requires each active
clip through bounded residency and invokes rf_entity_pose_evaluate with the
caller-provided pending root displacement. It does not advance playback again.
The shared generation cache controls recomputation; sampling failure may leave
a partial cache and must prevent subsequent sphere/sound/render consumption.
The adapter is explicit: existing cached sphere/follow queries do not silently
invoke evaluation. Live update orchestration still needs that connection.

The PC constructor/finalizer fixture retires its last motion, evaluates one
root with pending displacement4/5/6, checks the new generation stamp, then
checks physical sphere centerX5 and world sound point4/15/6. Re-evaluation in
the same generation leaves matrices and newly pending displacement intact.
Both builds and all18 CTests pass. This fixture exercises the empty-motion
evaluation path; authored active-clip evaluation through the corpse adapter
and native XEMU corpse visuals remain unverified.


## Authored corpse playback and pose harness

tools/verify_corpse_authored.py drives an opt-in --corpse-authored mode of the
PC residency test executable. It opens real levels/tables/meshes/motions,
registers the authored skeletal owners and selects one actor per skeleton
with death_generic. It starts a referenced clip, transfers the model, poisons
the former actor matrices, then runs reset/start/duration and120 consecutive
advance/evaluate/sound-follow queries. Model retirement and level teardown
must leave every playback resource reference zero.

L1S1/L1S2/L1S3 pass: each tests25-bone miner and Envirosuit_Guard,120 frames
each,78 changing poses each,2.633334-second death clips. Across the three
levels this is720 evaluated frames and468 changing-pose transitions. Bounded
clip residency totals12404/12316/12364 bytes respectively, including tables;
this is clip-cache accounting, not total game memory. Report:
artifacts/corpse-authored.json. Full PC build and all18 CTests pass.

This proves authored clip decoding through the transferred scene adapters.
It does not compare those frames against the original, instantiate the full
owned corpse constructor, render a corpse, or exercise XEMU/live death dispatch.
No runtime source changed in this harness-only commit.


## Scene corpse update orchestration adapter

rf_scene_corpse_update now assembles the verified417290 backend using retained
model reset/start/duration/advance adapters. The POSE callback evaluates then
refreshes physical spheres; sound attachment queries evaluate before reading
bones except for the no-attachment sentinel. External sound lookup/move owners
are explicit. Missing sound operations return RF_NOT_FOUND when requested.
Callback errors latch, suppress subsequent backend work, and are returned to
the caller. Earlier fade/field mutations are not rolled back.

The authored harness now runs all720 frames through this shared update entry
and separately evaluates the final pose as a consumer. Pose hashes and cache
accounting remain unchanged. A missing sound backend returns failure without
advancing playback; negative health marks deletion and returns before sound
lookup. Both builds and all18 CTests pass.

This covers ordinary playback and the mentioned failure/early-return paths.
The adapter transition-bit8 branch, playing sound movement and emitter owners
still need integrated tests. No complete live corpse list, deferred deletion
dispatch, render placement or XEMU corpse is claimed. Existing full original
417290 verification covers the shared orchestration independently of adapters.


## Authored transition, sphere and sound-position coverage

The authored PC harness now loads the render-model sphere definitions and
executes bit8 transition through the scene update adapter for miner/guard on
L1S1/L1S2/L1S3. Physical spheres use fixture radius1, not reconstructed class
configuration; transition refresh preserves those radii. Resulting bounds
radii are2.404366 miner and2.386669 guard.

The same update expires the fade timer and still performs transition/sound
work, clears only the enabled low byte of an expired emitter, clears bit8,
updates both object radius fields, and forwards the evaluated attachment point
to a supplied sound callback. A subsequent callback failure returns RF_IO.
Existing missing-backend/negative-health cases and final reference cleanup
still pass, together with720 ordinary authored frames and all18 CTests.

PC build passes. This commit changes tests/documentation only; it adds no
Xbox runtime changes or new XEMU result. Sound callbacks validate data but
produce no audio. Actual sound/emitter ownership, full corpse construction
from authored actors, live dispatch and rendering remain open.


## Corpse sound field is a world-object handle

Full original459a20 calls40a0e0, then accepts only object+24 equal1.
tools/verify_corpse_sound_object_lookup.py runs384 unhooked combinations of
slot, generation, type, presence and stale stored handle, including high-bit
handles. Only16 exact type1/live-generation cases resolve. Report:
artifacts/corpse-sound-object-lookup.json.

This changes the next integration step: corpse2cc must resolve a type1 world
sound object, not campaign_device_voice_ids or a mixer slot. Original48a230
then updates public/current/pending positions, radius bounds and dirty flag;
rf_group_pose_set_position already reconstructs those writes (COLLISION.md).
Sound-object creation/publication and its connection to audio voice refresh
remain to be recovered. Direct mixer movement would skip this owner state.
The public borrowed sound-view comment now states this distinction. Runtime
behavior is unchanged; no build or new XEMU result is claimed for this audit.


## Attached-item correction: corpse2cc is not sound

The prior sound interpretation was incorrect. Verified459a20 resolves exact
type1 objects. Newly inspected original45a3d0 resolves that object, checks its
class weapon/ammo fields at294+40/+3c, and updates actor inventory fields.
Original459bb0 restores its mesh;459560 handles pickup/respawn eligibility.
Dash Faction identifies type1 as OT_ITEM and459bb0 as item_restore_mesh:
https://github.com/rafalh/dashfaction/blob/master/game_patch/rf/object.h
https://github.com/rafalh/dashfaction/blob/master/game_patch/rf/item.h
These declarations are corroborating analysis leads; no third-party runtime
code was copied. The 384-case unhooked lookup result remains valid but proves
item lookup, not a sound lifecycle.

Shared corpse update/deletion fields and callbacks now use item terminology.
The generic48ac70 helper is rf_model_object_follow_point: it finds the object
root attachment position and does not implement audio. Layouts and algorithms
are unchanged. Historical sound-labelled tests exercised item movement and
deferred deletion, not audio. The integration target is attached item creation,
publication into corpse2cc, world-pose/bounds movement and eventual deletion.
Remove the previously inferred corpse-to-voice dependency from planning.

Validation after correction: both builds,18 CTests,720 authored frames,
4096 original/PC/NXDK update comparisons and4096 original/PC/NXDK root-follow
comparisons plus6 guards pass. Historical verifier filenames/output keys
containing sound remain for compatibility; their item semantics are clarified
here. No new live item owner or audio behavior is claimed.


## Attached item world-pose assignment

rf_scene_corpse_item_position delegates to the already verified48a230 position
assignment. The authored item callback now owns a retained world-pose record
and applies the actual helper, checking public/current/pending positions,
positive-radius min/max bounds, dirty04000000, and unchanged velocity/base
position. All three authored level runs and18 CTests pass with both builds.
The underlying position verifier passes8030 original/PC/NXDK assignments and
3 guards. Registry/type/lifetime resolution remains external to this helper.

The item publication search is incomplete: bounded disassembly of416000..
419000 finds2cc reads in update/deletion and the deletion clear, not assignment
of a new handle. Broader displacement matches belong to other object layouts
or need further classification; linear disassembly is not proof of absence.
No forced default, automatic item spawn or corpse handle publication is added.


## Authored actor-class sphere inputs in corpse transitions

The authored harness now loads all16 movement descriptors and runs the shared
initial actor pose selection/advance before model transfer. It loads each
selected actor class physics configuration and builds its class spheres from
that startup pose, then carries those spheres into the transition fixture.
The former uniform radius1 fixture is replaced with actual class geometry.

Across L1S1/L1S2/L1S3, miner1 radii0.6/0.4/0.15 and env_guard radii
0.432211/0.382409/0.177864 remain unchanged through corpse pose refresh.
Final bounds radii are1.554366 miner and1.564534 guard. All720 animation frames
still pass with unchanged hashes and zero motion references after teardown.
This strengthens constructor-input evidence but does not yet construct the
complete live corpse from an actor or assign its optional item handle.


## Corpse temperature metadata

Original41d4bc pushes the string $Body Temperature(F):,41d4ce reads the float,
and41d4d3 stores class+13e4;41d4db defaults it to zero. Constructor416940
copies5cd8e4 + class_index*1514 into corpse2b0. The417290 decay therefore
represents cooling relative to the class body temperature, not an unknown
general-purpose lifetime value.

rf_entity_corpse_config now retains body_temperature, adding4 bytes per class
inside existing budget accounting. The parser accepts one finite value with
zero default; the lexer splits the tag parentheses into separate tokens.
PC/NXDK metadata comparison passes63 installed classes and85 total cases,
including missing/negative/duplicate/nonfinite values. Three level seed loads
retain the field through archive close and exact/short-budget checks. Original
parser tag/default evidence is static instruction inspection, not parser execution.

The authored update fixture now seeds current/class temperatures from retained
metadata. Both env_guard and miner1 start at90F and finish at89.268402F after
120 ordinary ticks plus eligible transition/failure-path updates. Both builds,
18 CTests and720 authored frames pass. Full live constructor-source population
remains open; infrared rendering is not implemented by this change.


## Authored owned construction through retained class inputs

rf_scene_corpse_class_source populates only retained class fields: class/model
kind,724/728 flags, health, body temperature, replacement model string, emitter
lifetime and base model-local action IDs. Actor fields and resolved emitter
index remain caller-owned. Invalid class or weapon-switched mapping rejects
before mutation. Borrowed model strings require the class owner to stay alive.

The authored harness now calls rf_corpse_owned_create_bound with the actual
scene model handoff, action-name lookup and evaluated sphere refresh. It owns
body sphere copies and names through the real pool, registry and sentinel
lists. After120 update frames and transition tests, real owned deletion retires
the transferred model and frees names/body/slot. Registry/list counts and
allocated bytes return to baseline; all motion references return to zero.

Six constructor/update/destructor cycles pass across L1S1/L1S2/L1S3,720 frames
total. Clip residency is now16224/16136/16184 bytes: constructor pose demand
loads retained startup clips too. Both builds and18 CTests pass.

Dynamic actor words/position, material coefficients and optional item absence
remain explicit fixture inputs. Collision/source effects are recorded only;
replacement/emitter requests are rejected by this test, with selected authored
classes confirmed to request neither. No live death dispatch, actual attachment
publication, gameplay physics, rendering or XEMU corpse is claimed.


## Original corpse source-effect reservation and lookup

tools/verify_corpse_source_effects.py executes the original42dc00 dispatcher
and the reservation/attachment-lookup portion of42dc50. It passes25 dispatch
cases,10 gated returns and90 reservation cases, including30 complete original
failed-lookup returns. No shared C/PC/NXDK equivalence is claimed yet.

Flag08000000 requests eye with raw float arguments5.0/0.25; flag10000000
requests spine with8.0/0.5. The second flag is reread after the first call,
including when that call changes the flags. The arguments are not assigned
physical meanings until the downstream update/render ownership is recovered.

42dc50 first gates on global5a00f0. Original40a490 returns actor[0] in EAX;
it is a descriptor pointer, not the float suggested by raw Ghidra.
Original503c00 reads model[8] unconditionally before the descriptor/null
metadata gates. Do not change this into a speculative model-kind predicate.

Free ring62f488 and active ring62f764 use next/previous at4c/50. If no free
slot exists, the original chooses the first active node with greatest field0
value above-1.0, unlinks it, and places it alone in the free ring BEFORE
attachment lookup. Existing free rings are left unchanged. Lookup51d5b0
falls back to51d690 only on-1; both receive the same model metadata and name.
If both fail, the recycled effect remains removed from the active ring and
reserved in the free ring, with payload bytes unchanged. This ordering must
be preserved even though allocating only after lookup would seem convenient.

Successful-lookup cases stop before5034f0 and verify model/index plus actor
basis48/position3c argument pointers. Scratch constructors and attachment
lookup are supplied boundaries; geometry, successful publication, rendering,
empty/corrupt pools and live actor integration remain unverified. The next
reconstruction work is the attachment transform and downward geometry query,
then effect update/render/lifetime ownership; fixture source effects remain
explicitly unimplemented rather than being silently treated as complete.


## Complete original source-effect surface construction

verify_corpse_source_surface.py executes all of42dc50, including unchanged
4df690 room-tree geometry and its callees, vector arithmetic,4fcfa0 basis
construction and publication. Attachment lookup/5034f0 transform and4e5c60
face-color sampling are supplied boundaries.384 cases pass with192 accepted
hits across flat/sloped single-face rooms, three heights, above/below/out-of-
reach starts, both numeric argument pairs and one/two free slots with/without
an existing active node. No shared C/PC/NXDK equivalence is claimed.

The first actor word is the descriptor pointer passed to4df690, whose+3c
tree is traversed. Earlier wording identifying this word as an object ID was
incorrect and has been corrected. The earlier gate harness only observed raw
nonzero words before this pointer was dereferenced. World6460e8 also supplies
the face-list nonempty gate at+74. This is a room-tree query, not498e80
mover/static-world composition or an arbitrary downward world ray.

The descriptor uses attachment world position as start, displacement(0,-1,0),
radius0 and flags4, with FLT_MAX initial fraction. A surface beyond one unit
is missed. On contact the unmodified hit point is passed to face-color lookup;
the stored center is hit+normal*0.01, preserving the intermediate float scale.
4fcfa0 constructs the stored basis from the contact normal. The slot retains
the descriptor at44 and sampled color at48; fields0/4 initialize to0,8/c
copy the two input arguments and10 receives float(global5895a8/argument1).
Success removes the reserved free head and appends it to the active tail;
a geometry miss leaves the free-ring slot and payload unchanged. Recycling
before this query remains as established by the earlier lookup harness.

Next: reconstruct the shared effect owner against this contract, retaining
attachment/room/color ownership boundaries, then recover update/render and
retirement before connecting live actor source effects.42df20 disassembly
shows sine growth and quad submission, but its newly-created Ghidra function
export is candidate evidence only, not verified rendering or lifetime.


## Shared corpse surface-effect construction

include/rf/corpse_effect.h and src/core/effect.c now reconstruct42dc00/42dc50
dispatch and construction. Caller-owned circular rings provide storage with
no allocation:84 bytes per slot and12 bytes per pool header on32-bit targets.
The model metadata, attachment lookup/placement, descriptor-room surface query
and face-color sampling remain explicit callbacks. Full scene bindings are
not supplied by this change. Pool capacity is caller-provisioned; it is not
claimed to recover the original global pool count or initialization routine.

The constructor preserves reservation-before-lookup, first greatest elapsed
recycling, fallback-on-minus-one, geometry miss retention, offset/basis/color
payload writes and active-tail publication. Source dispatch rereads flags
between eye/spine calls. Backend errors stop shared dispatch and retain prior
mutations; a color error leaves the reserved position updated. Callbacks
must not mutate/reenter the pool, whose rings/storage remain owner-valid.
Empty full pools, invalid capacity and nonfinite/invalid authored inputs are
defensive RF_RANGE cases, outside the original valid-state equivalence claim.

verify_corpse_surface_shared.py compares all76 payload bytes and normalized
ring links against the384 full-original flat/sloped cases on both PC and
compiled NXDK code in Unicorn.192 hits and192 misses match exactly. The
original executes geometry; shared code consumes its supplied hit, so this
is construction equivalence rather than a shared room-query comparison.
The original executable hash remains pinned and the native binary hash is
recorded in artifacts/corpse-surface-shared.json.

New corpse_surface_failures CTest checks recycling at each active position,
failed/fallback lookup, placement/query/color errors, partial payload rules,
16 live flag-mutation combinations and disabled/empty/invalid-time guards.
Both builds and all19 CTests pass. These failure guards are PC tests, not an
additional original/NXDK differential test. No live corpse effect, native
emulator rendering, authored attachment integration or lifecycle is claimed.


## Corpse authored file-tag placement

rf_scene_corpse_file_tag resolves an explicit authored file view: case-folded
exact bone names, then LOD0 attachments; fallback uses original51d690
case-sensitive bone substring matching. The original middle runtime-tag
group is NOT represented. File-view indices must not be mixed with original
runtime indices, and this helper must not stand in for a complete runtime
tag owner. It preserves output on absence or invalid input.

rf_scene_corpse_file_tag_point resolves the registered model pose, including
owned corpse models. Bone indices use its current cached matrix. Attachment
indices read the immutable file record, construct its quaternion transform,
compose with the current parent bone, and use verified5034f0 placement.
It does not advance or evaluate playback; the caller must supply an evaluated
pose. File metadata borrows the meshes archive, which must remain open.
No persistent allocation is added; file-attachment records are read on demand.

The authored corpse harness checks both tags for120 frames each on miner1
and env_guard across L1S1/L1S2/L1S3. Eye file-view indices are27/26 and spine
is15 for both25-bone models. Finite placement, translation covariance, EYE
case folding, missing-name/invalid-index preservation and unchanged playback
generation pass after the former actor matrices have been poisoned.720
frames provide1440 base tag-point checks plus translated repeat checks.
Tag hashes are2861486865 for miner1 and611322986 for env_guard. Shared clip
residency remains16224/16136/16184 bytes. PC/NXDK builds and19 CTests pass.

This tests authored pose ownership and composed existing helpers, not a new
original-runtime frame comparison or XEMU execution. Scene surface/color
callbacks, runtime-created tag precedence, effect rendering/lifetime and
live source-effect dispatch remain open; constructor fixture source effects
are still explicitly observed only.


## Shared descriptor-room surface query

rf_geometry_corpse_surface now supplies the surface callback for the shared
constructor. The descriptor token is retained room index+1, matching the
existing room-tracking adapter; zero means absent. It traverses only that
room tree with displacement(0,-1,0), flags4, radius0 and FLT_MAX. It does not
route through primary/child lists, room skip gates, movers or world selection.
Accepted face indices map back through retained level source_indices. Misses
and errors preserve the hit; shared tree scratch requires serialized calls.
No allocation is added and valid retained geometry remains caller-owned.

The thin-face/tree helpers now accept any finite nonnegative fraction limit.
Their former limit<=1 restriction rejected this original caller. Segment-
plane acceptance still constrains contact to the actual segment; no ray
extension or replacement of FLT_MAX with1 is introduced. Other swept/room
wrapper contracts are unchanged.

verify_corpse_room_surface.py compares shared PC/NXDK query output with the
unchanged original4df690 geometry executed inside42dc50.384 flat/sloped
queries match point/normal bytes and mapped face identity, including misses
beyond one unit. Three port guards cover absent/out-of-range descriptors and
an empty selected room while another room contains a face. No primary list
is supplied to the shared fixture, establishing selected-room operation.

Both builds,19 CTests,1800 original tree cases plus3 guards and6060 original
thin-face cases plus2 guards pass. This adds a verified scene-geometry backend
but does not yet compose the authored model, actual tracked room, face color
and effect pool in live actor dispatch. Dynamic geometry, lightmap sampling,
effect lifetime/rendering and runtime-created tag ownership remain open.


## Face lightmap texel sampling

rf_lightmap_sample_1555 reconstructs4e5c60 after world-to-UV calculation
and texture lock. It uses trunc(width*u), trunc(height*v), byte pitch and
little-endian RGB555 extraction, with low three RGB bits zero and alpha255.
The stored high bit is ignored. This is not RGB565 or full8-bit color.
Exact binary32 scaling/truncation uses integer significand arithmetic to
avoid host-double rounding moving a coordinate across a texel boundary.

Unavailable pixel storage yields opaque white. This represents the original
lock failure, while the future face adapter must also select white for a
negative face lightmap index. Already-clamped UV1 is not changed to the last
texel: original u==1 can sample row padding/the next row. The shared view
permits that read only inside its supplied byte extent. Out-of-buffer reads,
bad pitch and invalid UV reject with unchanged output as port guards.

verify_lightmap_sample.py executes original4e5c60 with UV and bitmap-lock
boundaries supplied; texture lookup, float-to-integer conversion, pitched
fetch, color conversion and unlock ordering stay original.2021 valid cases
match shared PC/NXDK, including296 unavailable/missing-lightmap white cases
and319 in-buffer u==1 samples. Four port guards pass. Original unsafe reads
are excluded rather than executed. Both builds and all19 CTests pass.

The existing level lightmap loader still retains RGB-derived RGBA images.
Original texture-format conversion/ownership and4e49d0 world-to-UV mapping
must be recovered before this sampler can replace the face-color fixture.
No authored color, live corpse effect, rendering or XEMU result is claimed.


## Shared world-to-lightmap projection arithmetic

rf_lightmap_project reconstructs unhooked4e49d0: read point components at
descriptor axes60/64, multiply by4c/50, spill each product to float, add54/58
offsets with another float store, then clamp to[0,1]. Raw Ghidra output
obscured the multiply-before-offset ordering; disassembly and direct
execution establish it. Shared projection records carry just two axes, two
scales and two offsets,24 bytes, with no allocation.

Finite inputs and axes0..2 are required. Finite-input arithmetic overflow
clamps as original; invalid input preserves output. Shared point/output
aliasing is supported by calculating both outputs before publishing them.
This does not assert equivalence for original aliased output pointers.

verify_lightmap_project.py passes4096 original/PC/NXDK comparisons, including
all axis pairs, interior UVs, clamp boundaries, extreme finite scales and
a cancellation case sensitive to the intermediate product rounding.2048
shared alias cases and four invalid-axis/nonfinite guards pass. Both builds
and19 CTests pass. Geometry-to-descriptor construction and packed texture
ownership are still missing; these supplied projection records do not yet
provide an authored face-color binding or rendered corpse effects.


## Authored saved lightmap projection records

The original saved mapping path4ee2db..4ee51d is distinct from the lighting
generation path4e4a60. For version180 it consumes96 bytes: image index, four
byte fields, two scalar floats, two vectors, plane, flag/axis words, offsets,
scales and trailing word. Projection axes are saved offsets68/72, offsets
76/80 and scales84/88. They become descriptor60/64,54/58 and4c/50.

rf_lightmap_projection_read decodes these fields with finite/axis validation;
rf_geometry_lightmap_projection supplies an allocation-free accessor over
the retained geometry mapping records. Original allocation and image-owner
registration are separate. Invalid projection records preserve output.

verify_lightmap_projection_read.py supplies sequential typed file reads and
version180 while executing the original loader slice, including stores,
branches and image-index resolution.1024 cases consume exactly96 bytes and
match the shared PC/NXDK projection. Four port guards cover bad axes and
nonfinite values. The primitive file-reader implementations and allocator
are not exercised by this boundary test.

verify_lightmap_authored_projections.py decodes301871 mappings across all94
inventoried installed levels. It compares projected UVs with clamped saved
corner UVs at1819163 corners: nine components exceed0.0001, maximum
difference0.000228129327. This is measured agreement, not exact baked-UV
equivalence. The verified original projection must not be adjusted to fit
those saved UVs without further evidence. Both builds and19 CTests pass.

Next ownership dependency: original RGB-to-packed texture conversion and
pitch/storage policy. Authored projection now exists, but face-color binding,
live source effects, runtime tags and lifetime/rendering are still open.


## Original RGB upload packing

rf_lightmap_pack_1555 reconstructs4ed32c..4ed4fa after RGB bytes are loaded.
Original50df50 decides whether the in-place RGB pass is needed: a zero low
byte causes min(2*c+1,255), otherwise RGB remains unchanged. Both branches
pack max(c>>3,4) into each RGB555 channel and set bit15. Therefore the decoded
channel floor is32, not0; a simple RGB555 truncation would be wrong.

The shared caller supplies double_rgb explicitly rather than guessing the
active renderer capability. Source RGB is contiguous and mutable; packed
output uses caller-supplied even byte pitch and disjoint storage. No allocation
occurs. A NULL packed destination represents a subsequent lock failure:
the selected RGB brightening still takes effect. Buffer guards run before
mutation. Valid row padding remains untouched.

verify_lightmap_pack.py executes the original upload slice and max helper,
with capability and lock/unlock boundaries supplied.1024 cases cover all256
byte values, both branch decisions, pitched rows and lock success/failure.
Shared PC/NXDK source mutation and packed bytes match exactly; five port
buffer/argument guards pass. Both builds and19 CTests pass.

4f5e80 retains width/height, an RGB owner, a format5 bitmap handle and global
image registration. Its destructor4f5f20 releases the handle and RGB/optional
auxiliary allocation. These are traced leads, not reconstructed resource
ownership.50df50 dispatches by renderer mode17c7bcc; mode104 returns true,
mode102 calls546a00 with the query word, and others return false.546a00 uses
capability bytes1cfcc1c/1cfcc1d and the low five query bits. Their complete
live initialization remains unresolved. The existing preview lightmaps are
unchanged; no authored packed upload, face-color binding or rendering claim.

## Authored packed-lightmap owner and face-color binding

rf_packed_lightmaps_open/close now provide a port-owned CPU resource for
version180 saved RGB lightmaps. This is not a reconstruction of the original
bitmap allocator or GPU owner. The caller explicitly selects double_rgb;
packing uses the separately original/PC/NXDK-verified conversion. Each image
uses tightly packed16-bit pixels. Accounted bytes include the owner header,
image views and pixels, excluding allocator overhead and1536-byte stack scratch.
Failed partial loads release their allocations; close is repeatable.

rf_geometry_corpse_color resolves authored face mappings, projects the world
point with the verified saved projection, and samples the packed image.
Geometry and images are borrowed and must remain alive. Missing face lightmaps
produce opaque white; invalid or out-of-buffer samples return explicit errors.

verify_packed_lightmap_owner.py passes L1S1/L1S2/L1S3 in both conversion modes:
all pixels in110 image instances match independent conversion of archive RGB,
and384 face centroid samples match independently computed projection/colors.
Exact-budget opens succeed; one-byte-short opens clean up and empty the owner.
Per-level accounted bytes are754136,295104,754136 respectively. Both modes
have the same allocation size. These are component budgets, not total campaign
residency claims. PC and NXDK builds and19 CTests pass. The new resource owner
is exercised on PC; native allocation, live renderer-capability selection,
GPU texture sharing and live corpse surface dispatch/rendering remain open.

## Eight-slot surface pool reset and elapsed ticking

Original42dbb0 rebuilds the free circle from eight84-byte slots at62f490,
ending before62f730, and clears active62f764. Head62f488 points to the
first slot; next links ascend slot order, previous links descend and wrap.
Only links change: all76 payload bytes per slot survive reset. The static
constructor42dab0 also declares eight84-byte elements. The shared
rf_corpse_surface_reset now uses exactly eight caller-owned slots.

Original42e190 traverses the active circle and adds frame delta5a4014 to
each elapsed float, storing binary32. It does not change extent, expire
entries or return them to the free list. rf_corpse_surface_tick preserves
this separation, accepting finite nonnegative caller delta and valid bounded
rings. The port bounds traversal by capacity; errors after a malformed
traversal are not transactional. Payload elapsed values must be finite.

verify_corpse_surface_pool.py runs both complete original functions without
hooks against PC and compiled NXDK implementations:288 cases verify exact
reset ring order, retained payloads and elapsed-only updates for active
counts0..8, large ages and tiny/zero/large deltas. PC/NXDK builds and19 CTests
pass. This is instruction-level native verification, not an XEMU gameplay run.

Disassembly leads for the next integration:42e140 submits only effects whose
descriptor44 matches the requested room, through4d3560 with callback42df20.
42df20 computes extent during drawing using sine before growth_time, then
max_extent, and submits a four-vertex polygon through517110. Exact vector
construction, texture/render state and live scheduling remain unverified.
Do not add assumed age-based expiration to the verified tick function.

## Surface-effect growth and quad construction

rf_corpse_surface_build_quad reconstructs42df20 up to renderer submission.
For elapsed below growth_time, extent is sin(growth_rate*elapsed)*max_extent;
at or above the boundary it copies max_extent. Original arithmetic performs
x87 multiplication/fsin/multiplication before the float store. Shared C uses
double libm, with exact agreement in the tested512 cases; this is not proof
of universal transcendental bit-equivalence. Only effect extent changes.

The first two basis vectors at20/2c are separately scaled and float-stored.
Corners are (position-a)+b, (position+a)+b, (position+a)-b, (position-a)-b,
with a float store after the first add/subtract. UVs are00,10,11,01.
Color retains the low three RGB bytes and forces alpha255. The third basis
vector is unused here. Output is an84-byte vertices/UV/color value, with no
allocation or GPU state. Guarded invalid input preserves both outputs.

verify_corpse_surface_draw_original.py executes the full original routine and
unchanged vector helpers, supplying only final517110 submission.512 cases
cover both authored5/8-second growth values,0.25/0.5 maximum extents, random
positions/bases, zero/boundary/mature ages and arbitrary input alpha.
verify_corpse_surface_quad.py compares full84-byte effect state and84-byte
quad to actual PC and compiled NXDK code, including native libm. All bytes
match. Both builds and19 CTests pass. This is instruction-level verification;
no new XEMU frame or live effect rendering is claimed. Texture62f73c, render
state17c7c58, room submission4d3560 and lifecycle scheduling remain to bind.

## Shared world-quad projection and texture lead

Original42db58 stores the bitmap loaded from string595ef8 into62f73c;
the string is somenewblood_A.tga, present in the installed archive inventory.
This identifies the requested asset, not a live port texture owner.42df20
passes mode17c7c58. Its startup value00118c42 and decoded ordinary-particle
blend/depth/texture rules are already verified in docs/RENDER_STATE.md.
Do not confuse startup mode evidence with proof of every later mutation.

517110 forwards mode102 to558d40, expanding RGBA bytes into arguments.
The shared rf_particle_world_quad now exposes the existing four-vertex world
transform/clip/project path, with individual vertex depths and no billboard
depth override. rf_particle_world_stretch delegates its nonfallback geometry
to this helper. Finite vertex positions/UVs are checked before processing.

verify_particle_world_quad.py executes full original517110/558d40 through
final551900 submission, which is the only intercepted boundary.1024 tilted
quads match exact PC/NXDK screen polygon bytes:523 retain four vertices,
500 reject and one clips to five vertices. Varied camera, perspective, far
plane and clipping settings are supplied. The existing1024 world-stretch
cases still match original output. Both builds and19 CTests pass.

This makes the verified projection path reusable by surface quads; it does
not yet bind their room queue, texture residency, color backend or live
submission. No new GPU output or XEMU screenshot is claimed.

## Original room collector and deferred growth ordering

verify_corpse_room_queue_original.py executes complete42e140 with unchanged
4d3560,5186a0 and vector helpers, without hooks. Across288 cases it compares
all2048 queue slots, accepted counts and all eight effect payload/link records.
Active counts0..8, requested descriptors0..3, initial queue counts0/2046/2048,
world offsets and an optional x-plane are supplied. Results:206 appends,
15 sphere rejections,864 room mismatches and67 full-queue rejections.
The first independent plane expectation used the opposite normal convention;
inspection of the existing verified sphere implementation resolved it to
dot(normal,position)+distance > radius, with tangency accepted.

Only matching descriptor44 effects are submitted. Queue object is the effect
pointer; position and cull source both use effect14. Radius is the EXISTING
extent04, even when elapsed00 is1000 and growth_time08 is5. The collector
does not call the growth/quad builder;42df20 is queued for later execution.
Growing extent before queue culling would change original frame ordering.

Queue fields are sorted1, drawn0, grouped0, lighting0, lighting_flag1,
plane/minimum/maximum0 and callback42df20. Existing reserved and distance
bytes survive. Queue rejection does not stop traversal or mutate effects.
Room identifier0 is compared normally, without an extra absence gate here.

This establishes original collector behavior for the next shared binding.
No new C collector is introduced in this change; instance transforms, live
queue integration and deferred draw execution remain open. No GPU run.

## Shared corpse surface room collector

rf_corpse_surface_collect_room now reconstructs42e140 using the existing
verified rf_render_queue_append for non-instanced4d3560 submissions. The
caller supplies frustum, world offset, queue storage/count/capacity and a
nonzero draw callback token. Stored position is unchanged; world offset
is added only to the cull position with binary32 stores. Existing extent
is passed to culling and queued without evaluating growth.

The collector emits sorted1, lighting0, lighting_flag1, empty plane/bounds
and the supplied callback. Object tokens are32-bit node addresses, matching
the current PC/NXDK targets; larger addresses reject. Nodes must stay alive
until callbacks finish. Valid bounded rings and disjoint output storage are
caller requirements. Culling/full-queue rejection continues traversal; errors
stop with earlier appends retained. No allocation or draw callback occurs.

verify_corpse_room_queue_shared.py compares all288 original collector cases
to actual PC and compiled NXDK source: exact queue count and all2048 records,
including stale reserved/distance bytes. PC object addresses are normalized
to fixture nodes; NXDK uses identical synthetic addresses. Effect payload
and active ring links survive unchanged. Both builds and19 CTests pass.
Instance transform handling, live campaign collection, texture ownership and
deferred draw dispatch remain open. No new XEMU output is claimed.

## Authored blood-pool texture binding

rf_corpse_surface_texture_open selects somenewblood_A.tga from original
42db58/595ef8 and delegates frame0 loading to the existing bitmap owner.
There is no second pixel allocation or new texture-cache implementation.
Caller archive order and budget remain explicit; release uses existing
rf_particle_bitmap_close. This is port resource ownership, not a recreated
original bitmap handle. GPU binding and scene lifetime remain separate.

The installed maps_en.vpp entry is a64x64 uncompressed32-bit TGA with
16384 decoded RGBA bytes and source format7. Accounted owner plus pixels
is16420 bytes on the32-bit PC/NXDK targets, excluding allocator metadata
and fixed decoder stack. Alpha spans0..255.

verify_corpse_surface_texture.py independently decodes file orientation and
BGRA channels, then compares the retained pixel FNV1a checksum380274036.
Five PC cases cover success, exact-budget success, one-byte-short cleanup,
missing asset and archive index/order. The probe closes archives before
pixel inspection and repeats close. Both builds and19 CTests pass.
Native GPU upload, live scene residency and composed blood-pool drawing
remain open; this change does not add a new screenshot.

## Composed surface draw preparation

rf_corpse_surface_prepare_draw now composes growth/quad construction, world
transform/clipping/projection and final renderer vertex encoding. The caller
supplies the resolved camera and vertex environment plus12 output slots.
Effect RGB with forced opaque alpha replaces the environment RGBA value.
No heap allocation, texture binding or GPU call occurs. Extent updates before
projection, including subsequently rejected polygons; count/vertex outputs
commit only on success. This preserves the original draw-time growth order.

verify_corpse_surface_draw.py executes full original42df20,517110,558d40
and551900 through GPU vertex conversion. Only550850 state binding,559e80
index submission and559d90 GPU batch flush are supplied.551900 is observed
without replacement. The harness reads each32-byte primary vertex from its
original40-byte stride, excluding untouched secondary UV storage.

1024 PC/NXDK cases match exact effect state and encoded vertices:543 produce
four vertices,443 reject and38 clip to five vertices. Both authored growth
parameters, boundary ages, camera transforms, clipping/far/perspective flags
and input colors vary. Ordinary vertex color/alpha, depth/UV scales and no
color transform/fog are resolved fixture inputs. Both builds and19 CTests
pass. Universal sine bit-equivalence, actual rasterization, texture upload,
room scheduling and live death/effect dispatch are not established by this test.

## Synchronous renderer sink and authored PC raster integration

rf_corpse_surface_draw now sends prepared vertices, the borrowed texture and
caller-selected mode to a synchronous callback compatible with the existing
PC/Xbox scene particle sink. Image validity and sink are checked before growth.
Rejected polygons skip the sink. Sink errors propagate without rolling back
the earlier extent update. Texture ownership stays with the caller; no copy
or additional persistent allocation is introduced.

tests/particle_pixel_probe.c --corpse-texture maps_en.vpp exercises the actual
PC rasterizer from the authored texture owner through growth, projection,
vertex encoding and sink dispatch. Five cleared640x480 passes cover zero,
half-grown, mature, repeated mature and mature behind nearer depth. Changed
pixel counts are0,1926,3859,3859,0. Mature image hashes match217397337;
all depth-buffer values survive each pass. Behind-camera rejection skips
the callback while updating extent. An injected sink failure returns RF_IO
and also preserves the updated extent. Archives close before drawing.

verify_corpse_surface_texture.py records these results alongside independent
texture decoding/budget checks. Both builds and19 CTests pass. This is a
PC rendering integration test, not an original GPU image comparison or an
XEMU render. Live campaign creation/queue dispatch and Xbox raster validation
remain open. No meaningful new gameplay screenshot is available yet.

## Stock64MiB XEMU surface texture and draw validation

The particle diagnostic now includes corpse_pixel_test using the same
corpse_surface_fixture camera/effect setup as the PC raster probe. It loads
somenewblood_A.tga from D:\maps_en.vpp, closes the archive before drawing,
and calls the composed surface draw through the real Xbox scene particle
sink. Zero, half-grown, mature and repeated mature passes each record a
16x16 grid directly from the native framebuffer after GPU completion.
This is a diagnostic scene, not campaign death dispatch.

tools/xemu_particle_pixels.py run particle-pixels-20260912-004902 passes.
QMP confirms67108864 base bytes and0 plugged memory. All1024 blood-pool
pixel samples differ from PC by at most1 per color channel. Zero growth
leaves the background, mid/mature images differ, and repeated mature images
match exactly. Physical available pages are14095 before allocation,14091
with the texture retained and14095 after release. Accounted texture bytes
are16420; the native allocation consumes four physical pages.

The existing particle blend/depth/fog, animated-texture, stretched-particle
and flash checks also pass in the same isolated run. Both builds and19
CTests pass. XBE SHA256:
621be5f4d004bec17c3b3eb99a9a6e0024ab5b4eeee262d06454b57a5c2806d0

No host input or desktop capture was used. The harness closes XEMU and
restores/repackages its diagnostic flag. Live campaign creation, real room
queue scheduling, renderer capability binding and hardware/PS2 parity remain
open; these diagnostic samples do not establish those broader requirements.

## Lightmap upload capability query resolved

Original50c0e0 calls411e00 to initialize17756c4 to00418c45, fields
[texture5,color2,alpha3,blend3,depth4,fog0]. This is the query passed from
4ed32c into50df50 before RGB brightening. For renderer102,546a00 checks
multitexture byte1cfcc1c and, specifically for texture5, modulate2x byte
1cfcc1d. Both nonzero skips brightening; otherwise upload brightens.
Renderer104 always skips brightening; other renderer IDs brighten.

rf_lightmap_requires_brightening implements this decision from explicit
renderer and capability inputs. Byte semantics are preserved (low8 bits),
including noncanonical true values. This policy does not choose the port
renderer capabilities or silently replace current preview lightmaps.

verify_lightmap_brightening.py executes unchanged initializer/constructor
and query/helper functions against PC/NXDK in343 combinations. Query fields
and all decisions match. Both builds and19 CTests pass. Runtime mutation
of the query word and port device-capability binding are not proven here.

Additional disassembly leads:545ef3 reads1cfcb5c/60 and enables multitexture
when both are at least2. An earlier52afc0 check searches device description
1cfc7e8 for string5a9d50, Voodoo2, and bypasses that enable branch on a match.
545f46 writes the already documented TextureOpCaps MODULATE2X bit into
1cfcc1d. These initialization leads are not an Xbox capability probe.

## Live renderer lightmap representation audit

Source inspection confirms preview_fragment.ps.cg samples base/lightmap
textures and multiplies lighting by2. tools/pc_raster.c also uses
min(1,base*light*2). This demonstrates the currently selected port operation,
not a queried device capability. Both campaign entry points still call
rf_lightmaps_open, whose decoded image copies archive RGB unchanged.
The separately verified packed owner is not yet the live world texture owner.

tools/audit_lightmap_live_representation.py measures archive RGB versus
no-brightening1555 packing followed by original CPU channel-times-eight
sampling. L1S1/L1S2/L1S3 have1130496/442368/1130496 channels, with maximum
channel difference7 and mean differences2.664946/2.595864/2.144020.
Below-floor channel counts are132317/1/77980. Other quantized channel
counts are505957/268817/445459. These are representation differences,
not rendered pixel errors or PS2 parity measurements.

Next integration must distinguish normalized GPU5-bit sampling from CPU
lightmap sampling (channel*8), preserve the shader doubled modulation, and
share residency rather than casually retaining another full lightmap copy.
Applying RGB brightening independently while keeping the doubled shader,
or expanding GPU textures with CPU sampling rules, would change fidelity.
No live lighting change is made by this audit; it records the concrete
representation migration still required before binding campaign effects.

## Native1555 image storage and renderer sampling

rf_image now supports packed1555 when source_format is5 and bytes equals
width*height*2. The descriptor ABI is unchanged. Existing decoded TGA/VBM
images remain RGBA8 because their allocation is four bytes per pixel.
rf_image_is_packed_1555 exposes the distinction. Allocation and logical pixel
addressing use two bytes only for that explicit combination; Xbox keeps
its existing Morton swizzle and physically contiguous allocation.

The Xbox upload path selects native SZ_A1R5G5B5 and checks transparency
using bit15. PC raster sampling decodes each RGB component as channel/31
and alpha as bit15 before bilinear filtering. This intentionally differs
from the original CPU lightmap sample expansion channel*8. World/particle
sampling share this decoder. A remaining RGBA-only PC particle size guard
was found in the first harness run and updated before the successful rerun.

Shared packed_lightmap_fixture uses a32x2 native1555 image containing every
5-bit channel value and both alpha states. XEMU run
particle-pixels-20260912-010000 passes all64 sampled pixels against PC with
maximum per-channel difference1; transparent-row samples preserve background.
QMP confirms67108864 base bytes and0 plugged memory. Existing particle,
animation, stretch, flash and blood-pool probes also pass. Both builds and
19 CTests pass. XBE SHA256:
f9b18dd0aa9602c70dd3b0130114b301f08739c9ceffc5380e3c754efcfed79c

Campaign lightmap loading still uses the previous RGBA path. Migration and
shared swizzle-aware CPU lightmap sampling are the next integration steps;
this format test does not claim a campaign memory saving already achieved.

## Campaign migration to packed1555 lightmaps

rf_lightmaps_open now writes original1555 packed texels into the existing
rf_image owner. PC stores linear words; Xbox writes the same logical words
through swizzle-aware rf_image_pixel. Source format5 selects the tested
native texture path. Current world backends explicitly implement two-texture
MODULATE2X, so the verified102/1/1 capability profile skips RGB brightening.
No additional lightmap allocation is retained. Decoder scratch is2560 bytes.

verify_campaign_lightmaps.py compares every image hash to independently
packed archive RGB for L1S1/L1S2/L1S3, including exact-budget success and
one-byte-short empty-owner failure. Descriptor-plus-pixel allocations are
754124/295092/754124 bytes, down from1507788/590004/1507788. Pixel storage
is halved, saving753664/294912/753664 bytes respectively. Owner header,
allocator metadata and fixed stack scratch remain excluded by the existing
budget convention. This is now the live campaign loader on both platforms.

Stock64MiB XEMU replay-20260912-010403 passes180 frames with door staging,
NPC damage8456 and captured audio. Native gameplay state matches PC. QMP
confirms67108864 base bytes and0 plugged memory. Available pages at
completion are8717; historical replay-20260911-222939 recorded8538, though
other intervening code/layout changes prevent assigning all179 pages solely
to this migration. Exact lightmap savings are established by owner accounting.

The native framebuffer was inspected and shows the textured scene/robot;
this is a rendering sanity check, not quantitative PS2 visual parity. Both
builds and19 CTests pass. Shared CPU face sampling still needs a borrowed,
swizzle-aware view of this owner; it must retain channel-times-eight behavior
separately from normalized GPU sampling. Campaign blood-pool source/queue
activation remains open. The audit tool now labels raw RGB as historical.

## Shared renderer-owned CPU lightmap sampling

rf_lightmap_sample_image_1555 borrows the existing campaign rf_image owner.
It uses the verified exact binary32 texel-index calculation, resolves the
logical linear row address, bounds-checks it, then calls rf_image_pixel for
PC linear or Xbox swizzled storage. u1 may reach the following row exactly
as the original tight-pitch address; a final-row overread returns RF_RANGE
without changing output. Missing pixels return white. RGB channels expand
by8 with alpha255, regardless of the texel alpha bit, matching CPU sampling
rather than normalized renderer sampling. No allocation/copy occurs.

rf_geometry_corpse_resident_color supplies the authored mapping/projection
callback over borrowed rf_geometry and rf_lightmaps. The owners must remain
alive.192 face-centroid samples across L1S1/L1S2/L1S3 match the separately
owned packed reference in the no-brightening mode used by campaign rendering.
The existing six-mode/level packing checks also pass. The duplicate owner
exists only in that comparison test, not in the new sampling callback.

Stock64MiB XEMU particle-pixels-20260912-011000 verifies66 exact PC/native
CPU samples from the renderer-owned32x2 image: all64 texels including both
alpha states, u1 crossing to the next row, and final overread preservation.
Expected channel-times-eight colors are computed independently. The existing
normalized GPU samples, blood-pool rendering and other graphics checks pass.
Both builds and19 CTests pass. Live effect source/queue wiring and runtime
tag ownership remain open; no additional gameplay behavior is claimed.


## Authored source surface composition (2026-09-12)

`rf_scene_corpse_file_surface_effects` now supplies the reconstructed42dc00/
42dc50 source constructor with the transferred model's authored file tags,
retained room collision tree queries and the existing campaign1555 image owner.
The context borrows all owners, serializes tree scratch, does not allocate,
and does not advance/evaluate animation. Lookup backend errors are latched
across eye/spine dispatch instead of being mistaken for missing tags.

`tools/verify_corpse_authored.py` exercises six transferred miner/guard models
across L1S1/L1S2/L1S3,120 death-pose frames each, two attachments per frame.
Using authored NPC positions and an explicit identity orientation, it locates
the room and checks constructed nodes against separate tag/query/color calls:
L1S1:306 hits/174 misses; L1S2:315/165; L1S3:331/149 (1440 attempts).
Checks include exact color, descriptor, growth parameters, zero initial age/
extent, offset position, ring membership/count, disabled dispatch and invalid
model error propagation. Existing transferred-pose hashes and retirement checks
still pass. This is integration coverage of previously verified primitives,
not an independent original-frame oracle. Construction each sampled frame is
a test choice, not a claim about original death scheduling.

PC build, authored verifier and all19 CTests pass; NXDK build succeeds.
This composed authored test has not run in XEMU. Prior native verification of
individual surface/render/image primitives does not prove this live path.
Runtime-created tags, moving room ownership, death dispatch and deferred room
queue/render scheduling remain unbound; there is no new gameplay screenshot.


## Cached tag evidence and corpse lookup scope correction (2026-09-12)

Earlier entries call the middle51d5b0 group runtime-created/virtual tags.
The loader evidence narrows that description:51d420 obtains legacy attachment
records through51db90 (first submesh+58 count/+5c pointer), then CSPH records
through51dbd0 (+60 count/+64 pointer). It creates56-byte cached records with
borrowed names and local transforms; count is metadata+12b8.51ce60 reads the
header counts and its51d336..51d347 dispatch fills successive44-byte CSPH
records through53adc0. Ghidra's inferred parameter list obscures the ECX
receiver here; instruction-level tracing identifies the destination array.

`tools/verify_model_cached_tags.py` inventories all95 installed V3C models:
all header legacy-attachment counts are zero;38 models contain114 CSPH records.
Executing unchanged51d4ce..51d59f with supplied asset-derived submesh arrays
verifies borrowed name pointers, identity rotation plus CSPH center, count and
flag4. Parent words at cached record+34 remain explicit sentinels: this setup
slice does NOT copy the raw CSPH parent. Do not bind cached-tag placement using
raw sphere parents without recovering the missing behavior. The initializer
slice uses real getters and matrix helpers; it does not execute file I/O.

Original51d5b0/default-locale comparator passes3262 name queries over these
bone/cached-sphere/LOD0 groups. Original exact lookup plus51d690 fallback also
passes190 eye/spine queries: no cached name shadows either source-effect name.
Removing the cached group and translating indices selects the same authored
record or bone fallback for every installed model. For example, miner eye is
original index30 versus local file-view27; env_guard eye is29 versus26. These
are equivalent record selections, not equal numeric tokens. Existing file-view
indices must continue to be resolved exclusively by their paired placement API.

This clears cached-tag precedence as a blocker for the current corpse source
binding. Generic cached-tag pose/parent ownership remains an independent task;
this audit does not prove full model loading, original animated frames, native
C/XEMU composition or live death dispatch. No speculative cached-parent code
was added. Report: artifacts/model-cached-tags.json.


## Death-animation handoff reconstruction (2026-09-12)

`rf_entity_death_motion_sp` reconstructs41feed..42009f, the SP animation stage
inside41fdc0. It consumes a supplied4895d0 low-byte player predicate; a player
stores action824=-1 even when810 bit80 is set. Nonplayers with bit80 skip the
stage. Otherwise requested83c wins, with420c00 selection only for-1.

The40a1e0 low-byte skeletal path resets503400 and resolves5034d0 before checking
whether the chosen action has a motion. Nonnegative base-class bones13d8/13dc
clear actor1464 and their pose override byte;13e0 clears1468 and its byte.
No clearing occurs for negative base indices. Missing actions/mappings store
824=-1 after this cleanup. For a valid mapped action, a nonzero model resolves
its pose again and, if present, clears the two effective-class bone bytes.
The second resolution is preserved even when the first already succeeded.

Action824 is committed before playback. Base class724 bit200000 sets actor810
bit02000000 and passes blend0; otherwise blend1 is passed and810 bit8 is ORed
AFTER428c90. Both use gain1.0 and final argument1. Playback mutations to810
are retained. Callback contracts expose original helper boundaries and direct
bone-byte writes; no allocation or replacement skeletal representation is added.
Class/model/mapping owners must remain stable and bone tokens valid. Invalid
selected actions or missing required first pose return RF_RANGE, with prior
callback effects retained. The helper does not activate live dying dispatch.

`tools/verify_death_motion.py` executes the original instruction stage unchanged
apart from supplied player/type/selection/reset/pose/play helper boundaries.
It observes actual original bone-byte writes and compares full state, remaining
actor bytes,52 bytes of fixture bone flags and callback order against compiled
PC and NXDK code.2048 cases pass:677 player bypasses,170 bit80 skips,343 missing
animations,858 playback calls (440 ordinary/418 special). Random flag mutation
inside playback tests the pre/post ordering. Four PC/NXDK guard cases cover
invalid actions and a missing first pose. PC/NXDK builds and all19 CTests pass.
This is compiled-code emulation, not an XEMU gameplay capture or actual motion
playback test. Full death-start item drops, linked actors, player/camera effects,
remaining cleanup and live ownership remain to be reconstructed/connected.


## Configured death-item drop (2026-09-12)

`rf_entity_death_drop` reconstructs4200c6..4204a0, the configured82c item drop
inside SP41fdc0. This is separate from the subsequent42ae10 weapon drop. The
inspected SP path does not assign the returned world item to corpse2cc; local
item pointers are forwarded to a multiplayer notification in the MP branch.
Do not invent corpse attachment ownership from their proximity in death-start.

An item index of-1 skips the query. Otherwise the query runs BEFORE the64-byte
owned array scan: start=(x,y+.5,z), end=(x,y-7c4,z), with float spills before
forming end-start; radius.15, flags2000, initial FLT_MAX, hierarchy1 and identity
local transform against6460e8. Positive hit count plus any nonzero owned byte
allows one459100 creation at the hit point. The create contract supplies the
configured default count, empty name, actor handle, identity basis, -1,1,0.
The original calculates intermediate slope axes but reconstructs identity for
this creation call; those temporary axes do not change item position or basis.

A created item's2bc flags receive bit8. Ordinary items obtain503310's second
output vector's first component and add normal*component to base_position,
with float product spills, then copy it to position. Special names receive
only a+.05 Y adjustment and position copy. The actual original string is
`medical kit` (space), not Ghidra's underscored symbol label; the other name is
`riot_stick_battery`. Matching is ASCII case insensitive. Underscored medical_kit
and suffixed names remain ordinary items. The bounds callback must not mutate
the item or class name, matching the observed model-bound query role.

`tools/verify_death_drop.py` passes1024 original/PC/NXDK cases using unchanged
original4200c6..4204a0 arithmetic and500290/5001d0 comparisons. Only world query,
item allocation and model bounds are supplied at their original helper entries.
It checks exact query descriptors, creation arguments/identity, points, flags,
base/current item positions and source actor preservation. Branch counts:
94 disabled,385 query misses,83 empty inventories,109 allocation failures,
160 ordinary adjustments and193 special adjustments. Two additional PC/NXDK
failure cases prove query errors stop creation and bounds errors retain the
created item and already-applied flag8 for caller cleanup. Both builds and all
19 CTests pass. This is compiled-code emulation, not live XEMU item rendering.

Real item allocation/list/registry ownership, authored drop query/model bounds,
weapon-drop42ae10, linked actor release and remaining death-start effects still
need binding/reconstruction. No live death dispatch or corpse-item publication
is claimed. Report: artifacts/death-drop.json.


## Full SP weapon-drop oracle (2026-09-12)

`tools/verify_weapon_drop_original.py` now executes complete original42ae10
in single-player mode, including504e40/504db0/57312d RNG, quantity conversion,
vector/cross/normalization arithmetic, class-name comparison and current-weapon
writes. Only model pose418e60, item mapping459a90/459430, inventory operation
4031a0, collision4df1c0, item creation459100, notification401340 and bounds503310
are supplied.577eef supplies the explicit CRT thread-data owner; the original
RNG advances that owner. Resource callbacks are recorded in call order.

1024 cases pass:496 eligibility rejections,78 handled model-pose branches,
179 missing mappings,118 floor misses,31 allocation failures and122 successful
creations.78 notifications and182 inventory-operation calls are observed.
The report retains every fixture, request/basis, sequence, final current weapon,
RNG and created item state for the next shared implementation. These are original
execution results, not a C/NXDK equivalence or live gameplay claim.

Verified details to preserve in that implementation:

- Excluded global872118 and flag1a8 bit400000 reject first. For nonnegative
  current weapons, reserve+loaded uses signed32-bit wrapping addition and must
  be positive. Model pose handling precedes item mapping; a negative weapon
  reaching the pose helper still rejects when its supplied mapping is-1.
- Once a mapping is available, SP consumes one original random draw. The
  quantity is default_count minus trunc(default_count*range(0,.2)), clamped
  to at least4 after signed32-bit subtraction. The descriptor uses stride550;
  ammo type is85cd2c and drop quantity85cd90. The oracle checks exact integer
  reference reduction using the stored .2f rational13421773/67108864.
- Actual class strings are `Remote Charges` and `Remote Charge`, with spaces.
  The plural class resolves to the singular item after RNG consumption.
- Parameter low byte exactly1 uses the model-pose position, adding.5 to the
  query start and subtracting4*extent from the end. Every other low-byte value
  uses actor position without the+.5 start, calls4031a0 and clears current2a4
  BEFORE querying. This state change persists on misses/allocation failure.
  The callback fixture models an owned-byte removal; this is not independent
  proof of4031a0 internals, which still require recovery/binding.
- Query radius is.1, flags2000, initial FLT_MAX, hierarchy1 and identity local
  transform. Unlike the configured-item drop, the weapon spawn uses the
  computed surface-aligned basis. All original arithmetic is retained in the
  oracle; the report provides exact resulting basis floats.
- Any nonzero parameter low byte emits401340 after the create attempt, even
  when allocation returnsNULL; this differs from the exactly1 placement gate.
  A created item then gets flag8 and normal-scaled model-bound offset. Original
  actor bytes outside current2a4 and the supplied inventory effect stay intact.

The remaining implementation target is the full SP routine, using existing
shared inventory/RNG/query/item types where their contracts match. Item/resource
ownership, real model-pose/bounds and live dispatch remain separate open work.
Report: artifacts/weapon-drop-original.json.


## Shared SP weapon-drop orchestration (2026-09-12)

`rf_weapon_drop_sp` now reconstructs the complete42ae10 single-player routine
with explicit resource callbacks. It borrows the existing weapon inventory and
random-state owner, consumes64 drop definitions, preserves original callback
order/current-weapon writes, and returns any created item to its caller even
when a later bounds callback fails. Model pose may update current before its
mapping is queried; the current index is reread at the original boundaries.

The implementation preserves the parameter low-byte distinction: exactly1
uses model-pose placement without removal, while any nonzero byte notifies
after creation (including failure). Other placement values call removal and
clear current before collision. Queries keep the original radius.1/flags2000/
FLT_MAX/hierarchy1 contract; item requests retain the computed surface basis
and459100's -1,0,0 trailing arguments. Empty-name/default-resource ownership
is a create-backend contract, not a fabricated live allocator.

Quantity reduction uses the exact .2f rational and shared15-bit RNG draw.
Splitting the integer product before multiplication avoids overflowing64 bits
and reproduces truncation without relying on platform x87 intermediate width.
Surface axes use the original cross-product order, double-precision length,
float stores and zero-length normalization fallback. Item position adjustment
preserves the product float spill before addition.

`tools/verify_weapon_drop_shared.py` regenerates the original oracle and passes
1024 exact comparisons for compiled PC and NXDK code: current weapon, modeled
owned-byte changes, RNG, query vectors, item request quantity/point/basis, final
item flags/base/current position, ordered callbacks and notification arguments.
Two additional resource-error cases verify query failure after removal and
bounds failure after creation/notification/flag8; the created item stays exposed
for cleanup. Both builds and all19 CTests pass. The original fixture's4031a0
owned-byte effect remains supplied, not an independent reconstruction of that
helper. Native checks run compiled NXDK code in the instruction harness, not
live XEMU gameplay. Report: artifacts/weapon-drop-shared.json.

Next: bind real item allocation/list/registry ownership, model-pose/bounds,
weapon-to-item/default-count tables and query/removal/notification callbacks,
then compose remaining death-start stages with dying/finalization dispatch.


## Inventory removal and composed weapon drop (2026-09-12)

rf_weapon_remove_owned reconstructs4031a0. Invalid weapon indices are no-ops;
valid indices clear only the selected ownership byte, leaving reserve/loaded
ammo and current weapon unchanged. The active-player scan resolves each player
through the4a5b70 backend, compares inventory identity and the special weapon
at85cce4, then invokes the4a70e0 notification backend. Player count, special
weapon and the notified player handle are reread at the original boundaries.
The shared API bounds the borrowed player array and retains prior effects on
capacity errors; callers must keep the inventory and callback owners alive.

verify_weapon_remove.py passes1024 exact original/PC/compiled-NXDK comparisons,
including287 notifications, callback-driven count/special-weapon/player-slot
changes, invalid indices and empty lists. Original actor bytes are checked for
unintended changes. Resolution and notification implementations remain supplied;
this does not reconstruct4a70e0 or bind the live player registry.

This supersedes the supplied-removal limitation in the preceding drop reports:
the original full-drop oracle now executes4031a0, the PC fixture calls shared
removal, and the NXDK fixture executes a cdecl adapter into compiled shared
removal. These composed fixtures have zero active players. All1024 full-drop
comparisons and two resource-error cases pass, including182 removal paths;
active-player notification behavior is covered separately above. All19 CTests
pass. Native evidence is instruction-harness execution, not live XEMU gameplay.
Reports: artifacts/weapon-remove.json and artifacts/weapon-drop-shared.json.

Next: recover4a70e0 and bind the player registry, real item ownership, pose,
bounds and query backends before enabling complete live death dispatch.


## Player weapon-removal resource slots (2026-09-12)

Original4a70e0 iterates25 pointer slots beginning at player+10e8. Each nonzero
slot is passed to4cc010, then cleared after that call returns. It is resource
cleanup, rather than a generic player message. rf_weapon_release_player_slots
preserves this sequence and rereads later slots as they are reached. Callback
writes to the current slot are overwritten by the clear; writes to previously
visited slots remain. The caller retains the slot owner throughout cleanup.
The underlying4cc010 implementation returns linked resource records to pools;
its allocation and live resource type/binding remain to be recovered.

verify_weapon_player_slots.py executes full original4a70e0 and shared compiled
PC/NXDK code over1024 cases with16722 release callbacks, empty/full arrays and
mutations to future and already-visited slots. It checks surrounding original
player bytes stay unchanged. The4cc010 backend is supplied in this test.
Inventory-removal and full weapon-drop checks also pass against both rebuilt
binaries, as do all19 CTests. These are instruction-harness checks; no live
player resource cleanup or XEMU gameplay integration is claimed. Builds pass;
the PC build also reports an existing C4701 eye warning in diagnostic animation,
and the NXDK linker reports its existing .edata merge warning.
Report: artifacts/weapon-player-slots.json.

Next: identify and bind the resource owners released by4cc010, compose the
player-slot implementation with live inventory removal, and continue the item
allocation/query/bounds and remaining death-start integration.


## Original nested resource recycling audit (2026-09-12)

verify_player_resource_recycle_original.py executes unmodified4cc010, without
replacing any calls, over256 synthetic well-formed ownership configurations.
It verifies762 middle records and1880 leaf records: children are appended to
the leaf free list in traversal order, then their owner to the middle free
list, then the top owner to its free list. Existing free entries retain their
order, owned lists become empty, and payload bytes remain unchanged. The three
free counters increase by exactly the number of returned records.

Original list layout: top records use links+28/+2c and free sentinel876f28;
active sentinel878e18. Their child sentinel is embedded at+4, using links+1c/
+20, with middle free sentinel876f58. Middle records contain leaf sentinels
using links+14/+18, with leaf free sentinel876f80. Counters are878e48/4c/50.
The constructor4cbb30 takes a top free record, appends it to the active list,
initializes its child sentinel, and builds children while traversing model
triangles. This is evidence of model-geometry resources; their exact gameplay
meaning, creation inputs and live rendering binding are still unclassified.
Do not substitute the existing particle-emitter pool for this nested owner.
Report: artifacts/player-resource-recycle-original.json.

The diagnostic eye attachment is now explicitly zero-initialized; missing tags
still fail the existing found/parent validation. The PC rebuild no longer emits
the C4701 warning. This does not introduce a fallback eye pose.


## Riot-shield impact-mark identification and pool limits (2026-09-12)

The previously unnamed player geometry slots are riot-shield impact marks.
Disassembly establishes the weapon identity:4c662c pushes string5a3344,
"riot shield",4c6636 calls the weapon lookup4c81f0, and4c6640 stores its result
in85cce4. The intervening push of "Riot Stick" belongs to the next lookup;
it does not change the result being stored. This matches4031a0's removal gate.

Texture initialization loads "riotshield1.tga" into5a00f4 and
"RiotShieldHit.tga" into5a00f8 (4a9b9d..4a9bd4). Creator4a7110 searches the25
player slots for the first empty slot, resolves the held model via503f20,
uses hit position, randomized size/rotation and these texture handles, then
stores4cbe50's generated model-geometry result at player+10e8+4*slot. The
model triangle traversal and nested geometry recycling documented above are
therefore part of the shield impact-mark path, not a general item allocator.
Full pool allocation/clipping/rendering and impact dispatch remain open.

The original initializer4cbd80 now executes in32 randomized-memory cases in
verify_player_resource_recycle_original.py. Exact free-list order and empty
active/child lists pass. Capacities are50 top records at876fa0 (48 bytes),150
polygon records at877900 (36 bytes), and500 vertex records at873878 (28 bytes):
21800 bytes of record arrays, excluding sentinels/counters. Only prescribed
links, counters and top texture=-1 fields change; geometry payloads persist.
The previous256 recycling cases also pass. No shared pool implementation or
live shield rendering is claimed. This narrows the remaining cleanup work to
a specific weapon feature; full actor death dispatch still requires separate
item, camera, animation and resource integration.


## Shared death linked-actor handoff (2026-09-12)

rf_entity_death_link_sp reconstructs4204a1..4205e8. A non--1 linked146c handle
is resolved through426fc0; missing actors or a nonzero42a910 low byte skip the
stage. The linked actor receives the source position/basis in both base and
current fields, then48a660 unlink,40c2c0 room query and489f70 refresh execute
in order. Query arguments are current position, radius7c0,height7c4,0,
room69c/room6a0 outputs,1. No source pose update is fabricated.

If the local player exists,409050 and408ac0 act on the linked actor inventory.
The local pointer is reread for4290d0 and after that predicate. Its resolved
parent200 gets flags814 bit800 before4279d0, and word34 is cleared after the
callback. Finally40a210 tests each actor in the current global list; matching
actors have word34 cleared. The adapted list uses a null terminator and an
explicit traversal capacity; callbacks retain all borrowed owners. Registry,
world query, inventory and player detach implementations remain external.

verify_death_link.py passes1024 exact original/PC/compiled-NXDK cases including
400 handoffs. It executes original pose-copy helpers, supplies resource calls,
and checks pose/base pose, room results, flags, word34, local-player changes
and ordered callback arguments. Cases include missing handles/parents, low-byte
predicate gates, local-player absence, owner-callback removal of the local
pointer and detach-callback writes overwritten by the original final clear.
Original actor bytes outside the mapped fields remain unchanged. The initial
PC fixture text-mode input issue was fixed by setting binary I/O before use.
Both builds and all19 CTests pass. No live death dispatch or XEMU gameplay is
claimed. Report: artifacts/death-link.json.

Next: recover the remaining death-start player/camera effects and tail cleanup,
then bind these stages to live registry, animation, item and world owners.


## Shared SP death-start tail cleanup (2026-09-12)

rf_entity_death_tail_sp reconstructs420b03..420bdc for single-player. Action520
value13 calls4096f0 on the actor inventory. Class728 bit20 sets deadline4b8
using shared rf_timer_set: radius78 greater than6 selects2000ms, otherwise
1600ms. The original x87 unordered case also takes1600ms; NaN and infinity
inputs are included in the comparison fixture. The shared clock is borrowed
and read after the inventory callback, preserving strict timer-period wrap.

The429ab0 actor callback always follows the optional timer. Flags810 are then
reread; bit400000 calls43e9b0 with the name resolved from the original string
owner by4ff480. Auxiliary model148c is read after that event. If nonzero,
502b10 receives it and the field is cleared after release, even if the callback
writes another value. MP-only1430/player timer behavior is excluded from this
SP helper. Resource and callback owners must remain alive through the stage.

verify_death_tail.py passes1024 exact original/PC/compiled-NXDK cases, including
536 timer changes,502 events and525 releases. Original4fa360 executes unchanged;
fixture callbacks exercise class-flag mutation, reset flag mutation, event
model replacement and release-time writes. Threshold neighbors, negative
radius, infinities, NaN and clock-wrap boundaries are included. Actor bytes
outside mapped fields remain unchanged. Both builds, all19 CTests and the
1024-case linked-actor comparison pass. No live resource release, death
dispatch or XEMU gameplay is claimed. Report: artifacts/death-tail.json.

Next: recover the early player/camera side effects between death-entry and
animation, then compose the verified stages using live resource ownership.


## Shared early SP death player/timer stage (2026-09-12)

rf_entity_death_early_sp reconstructs41fe5f..41feed after the entry flag writes.
Collision retirement48c9f0 runs first. Only if actor identity matches the
current local actor does it inspect the current player, call4ace90's low-byte
predicate and optionally4ad8a0. Player pointers are reloaded after both calls;
local actor identity is checked again before clearing bytefb0. The adapted
player view preserves the three neighboring bytes. Borrowed owners stay alive.

Both SP timers run independently of local actor identity:62fd48 is set1500ms
from the current game clock and62fd44 is set750ms. Shared rf_timer_set retains
the original strict wrap behavior. Invalid clock errors retain earlier effects.
The player mode callbacks remain external to this orchestration. Disassembly
shows4ace90 reads bytef94;4ad8a0 clears bytesf94/f95 and wordf98. These field
owners still need mapping to live player state; no specific camera-mode name
has been inferred from those offsets alone.

verify_death_early.py passes1024 exact original/PC/compiled-NXDK cases including
210 mode stops. Original timer helpers run unchanged. The fixture covers
collision-driven local identity changes, predicate-driven player replacement
or removal, identity changes, stop-driven player replacement, null players,
high-byte-only predicate results and clock-wrap boundaries. Only the intended
fb0 byte changes in original player storage. Both builds and all19 CTests pass.
Resource callbacks are supplied; this does not enable full death dispatch or
claim XEMU gameplay. Report: artifacts/death-early.json.

Next: map the small player-mode field handlers, audit remaining SP branches
between the reconstructed stages, and compose them with live resource owners.


## Player-mode field handlers and death-stage gap audit (2026-09-12)

rf_player_mode_active and rf_player_mode_stop reconstruct4ace90 and4ad8a0.
The query returns bytef94 without boolean normalization; stop clears only
bytesf94/f95 and wordf98, preservingf96/f97. The8-byte adapted state is owned
separately from fb0. verify_player_mode.py executes both original handlers
without hooks and compares PC/NXDK output in1024 randomized player buffers,
covering every active-byte value. Original surrounding bytes remain intact.
Both builds and all19 CTests pass. The early-death callback fixture still
supplies mode callbacks; these handlers are not yet bound to the live player.
Report: artifacts/player-mode.json.

The stage-gap audit retains an explicit mode restriction: shared motion-stage
comparisons use6fc4d8=0 as well as64ecb9=0. A nonzero6fc4d8 bypasses the normal
4895d0 player gate and can clear player11f4 after42a8e0 at4200a0..4200c6. The
setter480b80 initializes additional player entities and distinct view modes
when59f294 is2;4359e0 clears this mode and restores the first player view.
This is evidence of an alternate player mode, not proof of its complete
semantics. It must not silently enter the ordinary-SP composition.

Ordinary-SP stage order now accounted for: entry vectors/flags, collision and
local player/timers, death motion, configured item, linked actor, weapon reset
41ae70 followed by weapon drop42ae10, then tail cleanup. The intervening
420600..420b03 branches are gated by64ecb9. A composed implementation still
needs one coherent actor field owner across all adapted views and callbacks,
plus real model, world, registry, item, inventory, event and player owners.
Per-stage comparisons do not establish full41fdc0 equivalence or live gameplay.


## Retained campaign NPC death fields (2026-09-12)

campaign_npc_body now owns24 bytes for item82c, requested death action83c,
active death action824, linked actor146c, deadline4b8 and auxiliary model148c.
The first five initialize to-1; the model initializes to0. Source evidence is
423367..423385 for item/actions/link,40e380 for the inactive timer, and423af2
for the auxiliary model before its MP-only respawn visual branch. These fields
are distinct from the existing actor action520 and pain selected action828.
This supplies persistent storage for composition; it does not start dying
updates or replace the existing health/flags, physics and model-pose owners.

NPC_DEATH_OWNERS telemetry reports registered count, added bytes and initial
state hash. The XEMU replay harness compares PC/guest values and independently
checks the five-absent/one-zero initializer pattern. Existing NPC body budget
accounting includes the enlarged records. The Live Mines replay has78 owned
records,1872 added bytes and hash3974211757. PC/Xbox builds and all19 CTests
pass. The180-frame stock64MiB XEMU door/damage/audio replay also passes with
8715 available pages at completion and unchanged movement/audio comparison
results. NPC body accounting is50740 resident bytes and425380 peak bytes.
Evidence: artifacts/xemu/replay-20260912-023602/report.json.

This is live storage and memory validation, not live death behavior. Connect
the stage views to these retained fields and existing shared owners, then
bind resource callbacks before enabling complete death dispatch.


## Registered NPC tail adapter (2026-09-12)

rf_scene_npc_death_tail resolves a current NPC registration and runs the shared
SP tail against its retained death deadline/model, published actor action and
flags, model radius and retained class flags. It publishes timer/model changes
before every resource callback and reloads the owner afterward, so inventory
class changes, reset flags and event-driven model replacement reach subsequent
steps. Published view.flags810 is mirrored into the damage view; unrelated
death item, requested/active death action and linked-actor fields are preserved.
The name token and resource callbacks are supplied by the eventual dispatcher.
Registration and all resource owners must stay alive during this operation.

The NPC residency test now exercises this adapter with an actual registered
campaign_npc_body: callback-visible wrapped timer, flags synchronization,
event-selected model release, the post-release clear, stale generation,
unregistered handle, and invalid clock retaining preceding effects. This is a
PC ownership-binding test; compiled NXDK includes the adapter, while the1024
original/PC/NXDK tail comparison verifies the shared stage separately. Both
builds and all19 CTests pass. No allocation is added by the adapter.

Full death dispatch remains disabled: this tail cannot substitute for entry,
collision retirement, animation, configured-item/weapon drops and linked-actor
handoff. Resource operations are still supplied, and this adapter has not been
exercised by a live XEMU death. Bind the other stage views and their resources
before scheduling the complete transition.


## Registered model reset for death animation (2026-09-12)

The503400 reset in the death animation stage dispatches via501cd0 model kind2
to51c390. That operation is the already reconstructed rf_motion_stop_nonlooping:
clear primary/freeze designation and frozen state, zero only weights whose
loop byte is exactly0, and retain slots, cursors and resource references.
Model kind3 has a separate representation and is not covered by this binding.

rf_scene_model_stop_nonlooping resolves the registered model's currently
published pose and its shared playback resources, then executes that existing
operation. It adds no allocations. The NPC residency test covers loop bytes
0/1/2, complete playback state, unchanged reference counts, malformed motion
preservation, out-of-range and retired owners. It also transfers a model to
owned corpse pose storage, poisons the original actor playback, and confirms
the reset reaches only the transferred pose before normal retirement releases
its references. This protects against indexing the old level-pose array.

Both builds and all19 CTests pass; the extended residency test passes after
adding transfer coverage. The1800-case original-vs-PC motion-stop verifier also
passes. The binding itself is tested on PC and compiled into NXDK; no new
XEMU death-animation execution is claimed. Death motion still requires real
bone-override state, action selection/playback and complete dispatch binding.


## Registered NPC death playback (2026-09-12)

rf_scene_npc_death_play binds the death stage's428c90(actor,action,1,freeze,1)
operation to the registered NPC owner, current actor pose and bounded motion
cache. It validates handle generation and base unarmed mapping, publishes
action824 before loading/starting, then forwards the declared action sound to
a caller-owned callback. Loading or sound errors retain preceding effects.
Armed mappings and transferred-away actor poses are not supported by this
binding. No full death dispatch or actual sound selection is added here.

The fourth original argument is freeze-at-end, previously described as blend
in death-stage terminology. Only low byte1 designates the freeze slot. A
restart with low byte0 preserves an existing designation; the separate reset
clears it. The authored residency fixture checks both orders, values256/257,
reference retention, stale handles, missing mappings, armed rejection and
callback-visible action state with missing/failing/successful sound callbacks.

All19 PC CTests pass. The6000-case original-vs-PC action-start verifier passes
through loaded type-two controls and sound-class dispatch; it excludes actual
sound selection/playback. PC and NXDK builds succeed (NXDK is up to date).
The new ownership binding is tested on PC, not yet through live XEMU death.
Complete death composition, bone overrides, armed declarations and resource
callbacks remain open.


## Bone override clearing and cached death poses (2026-09-12)

Inspection of original51b500 shows the override branch after parent/root
composition and generation-stamp publication. The per-bone record uses
instance1398+48*bone for a nine-float basis,13bc+48*bone for the enabled byte
and13c0+48*bone for blend weight. Nonzero enabled bytes enter the branch.
The generation comparison occurs before this branch, so clearing the byte
alone must not forcibly rebuild a pose already evaluated this generation.

tools/verify_bone_override_clear_original.py executes the original evaluator
and its callees without hooks on installed miner stand/crouch motions. Its320
cases cover two ticks each, bones0/1/8/24, weights0/.25/.5/1 and enabled bytes
1/2/127/128/255. For each case it compares complete25-bone matrices: enabled
byte variants agree; clearing only the byte preserves current-generation
matrices; advancing the generation restores the unmodified baseline exactly.
The override basis, weight and surrounding record bytes remain unchanged.
310 cases differ bitwise from baseline before clearing; that count measures
byte differences, not visual significance or nonzero blend contribution.
Report: artifacts/bone-override-clear-original.json, PASS.

The shared evaluator currently has no override record or override blend path.
Do not replace death's CLEAR_BONE callback with a cache invalidation or a
no-op. Implement retained override storage, original conversion/blending and
owned-pose transfer before binding it. This audit proves original behavior
only; it is not a PC/Xbox implementation comparison or live death test.


## Override basis conversion (2026-09-12)

rf_model_basis_rotation reconstructs518e10, the non-normalizing matrix-to-
quaternion conversion used by the override branch. It covers nonnegative
trace and largest X/Y/Z diagonal branches, including their strict tie order.
Instruction inspection shows that the X branch reloads a float-rounded
Y+Z diagonal sum; the trace comparison and Y/Z branches retain intermediate
precision. The shared helper preserves that distinction using explicit
float/double operations. Finite inputs are required; failures preserve output,
and aliased output is supported by computing the quaternion locally first.

tools/verify_basis_rotation.py compares all four quaternion components byte
for byte against unhooked518e10 for4101 cases: identity, principal half-turns,
zero basis and4096 seeded general finite bases. Branch counts are
2023/668/704/706. Both PC and NXDK-compiled C match. NXDK instructions execute
in the isolated instruction harness, not XEMU. No normalized-input assumption
or corrective normalization is introduced. Both builds and all19 CTests pass.

This supplies one prerequisite for override evaluation. Retained records,
override interpolation/matrix reconstruction, pose transfer and death-stage
clearing still require integration and an end-to-end original comparison.


## Complete override rotation blend (2026-09-12)

rf_model_override_pose reconstructs51b94c..51b9db: convert override basis and
composed bone rotation with518e10, interpolate current toward override with
519da0, reconstruct the rotation with5193f0 semantics, and preserve all three
translation floats. Caller owns enabled-byte and generation gates. No
allocation or retained record is added by this helper.

The first original comparison exposed missing weight wrapping in the shared
interpolator. Original519da0 repeatedly adds float1 while negative and
subtracts float1 while greater than1; endpoints0 and1 remain distinct.
The shared path now does the same, rejecting non-finite weights and steps
that make no float progress rather than hanging on malformed huge values.
This is wrapping, not clamping or extrapolation.

tools/verify_override_pose.py executes the full original override block
without replacing its callees and compares all48 matrix bytes with PC and
NXDK-compiled C. All1024 cases pass, covering seeded rotations, equal bases,
translations and weights -2/-1/-.25/0/.25/.5/1/1.25/2/3. Original translations
are independently checked against input. NXDK runs in the instruction
harness, not XEMU. Existing1600 pose-blend comparisons and all19 CTests pass;
both builds succeed. Retained overrides, evaluation/transfer integration
and death clearing remain open; no live gameplay change is claimed.


## Overrides in hierarchical pose evaluation (2026-09-12)

rf_model_evaluate_overrides accepts one44-byte override record per bone, or
NULL, and uses the same shared playback evaluator. After normal local/parent
composition and generation-stamp publication, a nonzero enabled byte applies
rf_model_override_pose before descendants are evaluated. Matching generation
stamps skip the whole operation. Disabled records are not interpreted. The
existing evaluator APIs pass NULL and keep their prior behavior. Records are
caller-owned; this step adds no campaign allocation or persistent pose fields.

tools/verify_bone_override_evaluation.py compares complete original51b500
execution with the PC C evaluator on the same320 fixtures as the original
clearing audit. Every case evaluates enabled, clears the byte and reevaluates
the same generation, then advances to the next generation. All24000 bone
matrices and24000 generation stamps match byte for byte. The selection covers
root/internal/leaf bones, so parent override propagation is included. It does
not yet cover simultaneous overrides or mixed playback slots with overrides.

Existing playback-skeleton comparisons also pass320 cases/8000 matrices/320
eye transforms without overrides, and all19 CTests pass. Both PC and NXDK
builds succeed. Full override evaluation is tested on PC and compiled into
NXDK; no new XEMU runtime evidence is claimed. Next retain records in actor
poses, transfer them with corpse ownership, account their memory and bind
death's clear operation without invalidating generation stamps.


## Retained actor and corpse override ownership (2026-09-12)

rf_entity_poses_open now allocates zeroed44-byte override records per bone
and includes them in its existing budget; each actor receives its own slice.
rf_entity_pose_evaluate passes that slice to the verified override evaluator.
Existing synthetic poses may omit overrides. Transfer copies present records
into independent owned storage, then clears the consumed source records.
The destination generation stamps remain intact. Owned storage lays out
matrices, aligned override records and then16-bit stamps. Close releases the
whole allocation and moved animation references once. No budget is enlarged.

Opening-level allocation tests verify zero records, separate slices, exact
accounting, one-byte-short failure and repeatable close. L1S1 has1689 bones,
74316 override payload bytes and182506 total pose-owner bytes. On32-bit
builds, rf_entity_pose is304 bytes and rf_entity_owned_pose312 bytes; a50-bone
owned pose uses5012 bytes with overrides or2812 without. Existing per-scene
corpse memory limits still apply to total allocations.

Extended PC transfer tests poison old source buffers and check independent
override payloads. The NXDK instruction harness passes150 cases each for
plain/registered transfers, both with and without overrides (600 total),
including tight budgets, heap failure, reference preservation and cleanup.
Both builds and all19 CTests pass.

Native replay artifacts/xemu/replay-20260912-031030/report.json passes180
frames on base-memory67108864, plugged-memory0, with8698 available pages at
completion. Existing PC/guest playback, movement and audio comparisons pass.
This validates the new allocation and disabled-override baseline, not live
enabled overrides or death transitions. Bind CLEAR_BONE and original override
producers next; full SP death composition remains open.


## Registered model death bone clearing (2026-09-12)

rf_scene_model_clear_bone_override resolves the currently published actor or
transferred corpse pose through the model registry, bounds the bone index
and clears only that record's enabled byte. It preserves basis, weight,
record padding, playback, matrices and generation stamps. Missing/retired
poses or missing override storage fail without fabricating a record. The
full death dispatcher still supplies model identity and the selected bones.

The registered-model fixture checks repeated clearing, invalid model/bone,
missing storage and retired models. It transfers an enabled override to
owned storage, poisons the original actor record, then verifies the clear
reaches only the published corpse pose with its cached matrix/stamp intact.
The subsequent retirement exposed an old-layout preflight in scene.c; it
now validates override placement, shifted stamps and expanded allocation
size before retiring and freeing the owned model. Reference counts drain
once and the scene-owned allocation is released normally.

Both builds and all19 CTests pass. The320-case full original/PC override
evaluation comparison remains passing. These new binding/retirement checks
run on PC; NXDK compiles the adapters. The preceding stock64MiB replay covers
baseline allocation, not this new death-clear call or transferred retirement
in XEMU. Full death composition and enabled-override gameplay remain open.


## Authored death-bone selection (2026-09-12)

rf_model_death_bones reconstructs424e47..424f00 within class setup4246e0.
The original573930 searches case-sensitive substrings, not exact names or
case-insensitive tags: take the first two names containing spine and first
containing head. If the second spine's parent is not the first spine, swap
the pair before storing class13d8/13dc/13e0. When the second spine is absent,
the original descriptor address calculation reads bone count at48 as the
parent comparison. The shared helper models that value without out-of-bounds
C access. A lone spine therefore ends in slot1 with slot0=-1. Non-skeletal
setup uses a separate all-minus-one branch outside this helper.

tools/verify_death_bones.py runs the original selection block and real string
search without hooks, comparing all three outputs to PC and NXDK C. All1119
cases pass:1024 synthetic name/parent layouts and95 installed V3C skeletons.
Miner resolves to15/16/8. Sixty-one installed models lack a full spine pair.
This does not establish their effective class configuration or prove that
all can reach the same unguarded effective-bone clear path in death-start.
Base bone clears already guard negative indices; effective class selection
and missing-pair reachability need tracing before full composition.

Both builds and all19 CTests pass. Native C runs in the isolated instruction
harness, not XEMU. The helper allocates nothing and has not yet been bound
to retained base/effective class views; malformed unterminated names and
counts above50 fail without modifying output.


## Missing effective bone index aliases playback cursor (2026-09-12)

Original42000c..42003a uses effective class13d8/13dc without negative guards.
For index-1, pose13bc+48*index is138c: the low byte of slot15's tick, since
slot0 begins12d4 with12-byte records and tick at+4. This is not an override
record and not a harmless skipped bone.

tools/verify_death_missing_bone_original.py executes that instruction block
unchanged across1024 random pose buffers and effective-index pairs,569 with
at least one negative index. Complete8192-byte buffer comparisons confirm
only the addressed bytes change. For index-1, all cases independently match
slot15.tick bits AND ffffff00. No callees or writes are substituted.

rf_scene_model_clear_bone_override now treats UINT32_MAX as this exact
original index-1 effect using explicit unsigned bits and memcpy, avoiding
undefined indexing in C. Nonnegative bones retain the enabled-byte behavior.
The registered actor and transferred-corpse tests each exercise all256 low
byte values, checking the complete playback state, override record, matrix
and generation stamp. Other out-of-range indices still fail.

Both builds and all19 CTests pass. The original audit is instruction-level
evidence; ownership checks run on PC and the adapter compiles into NXDK.
Actual class reachability of this path is still unproven and must be traced
when binding base/effective views. No live death/XEMU execution is claimed.


## Loaded class gate for death bones (2026-09-12)

Caller424520 supplies the bone-selection gate to4246e0 only for kind2 models
with class724 bit20000, the authored humanoid flag. In the common base-equals-
effective path this uses40a1e0 (nonnull actor/model and base kind2) followed
by40a150 (base class humanoid bit); the separate-view paths test each class.
The disabled branch424f02 writes -1 to all three class bone fields. The
already-initialized bit20000000 and failed/null model loading are separate
setup lifecycle gates, not evidence for changing the selection itself.

rf_entity_class_death_bones binds the verified selector to loaded seed-class
and skeleton ownership. Non-kind2 or non-humanoid classes return all-minus-one
without needing skeleton storage. Eligible classes validate their mapping
and use the decoded original bone names/parents. No allocation is added.
This helper models successful loaded setup, not an already-initialized flag
mutation or failed resource-loading path.

The opening-class probe verifies14 entries across L1S1/L1S2/L1S3 against
original-audited model selections plus the recovered gate: six humanoid
miner/guard entries resolve15/16/8, eight other entries resolve-1/-1/-1.
Forced non-humanoid and non-skeletal tests also return all-minus-one without
skeleton access. tools/verify_class_death_bones.py passes; its source model
comparison reruns1119 original/PC/NXDK cases. All19 CTests and both builds
pass. Class binding is tested on PC and compiled into NXDK, not live XEMU
death. Effective view ownership and full stage composition remain open.


## Registered NPC death-animation composition (2026-09-12)

rf_scene_npc_death_motion now composes the verified ordinary-SP animation
stage with registered NPC/model ownership: loaded class bone selection,
non-looping reset, exact bone-byte clearing (including index-1 cursor effect),
and actual mapped death-action loading/playback. The current base/unarmed
view supplies both base and effective bone indices; armed mappings and
transferred-away actors are rejected. This is an explicit current-view
restriction, not proof that effective declarations always equal base.

Before each operation, retained flags/action/request and actor1464/1468 words
are published. After callbacks, mutable owner fields are reloaded and damage
flags synchronized, preserving changes before subsequent operations and the
ordinary post-play bit8. Selected action and sound services remain supplied;
missing selection is rejected before model mutation. First service failure
stops later resource operations and final publication; preceding effects
remain. This error policy is a port boundary, not original exception behavior.

Two retained words add8 bytes per NPC (624 for78 actors). Current allocation
zeroes them; constructor4235f0 explicitly clears1464, while1468 producer/
initialization semantics still need recovery. Humanoid death clears both
through the verified stage before later use. No other consumer is added.

The registered-NPC fixture checks a mapped action through the real motion
cache, base/effective bone clears, word resets, slot15 cursor bytes, ordinary
freeze/bit8 and special no-freeze/02000000 flags, sound callback mutations,
selection success/failure, stale handles and armed rejection. All19 CTests
and both builds pass. The separate2048-case original/PC/NXDK death-motion
stage comparison remains passing. New composed ownership execution is on
PC; NXDK compiles it, with no new live XEMU death test.

Complete death dispatch remains open: entry/collision retirement, early
player/timers, drops, linked actor, tail and dying updates must share owners
and resource scheduling. Selection/clearance/RNG and audio callbacks need
actual campaign callers; alternate-mode and effective-view paths remain.


## Registered NPC death selection (2026-09-12)

rf_scene_npc_death_select can now be assigned directly to the composed
animation stage's select callback. Its caller-owned context supplies the
collision world, registered-entity scratch capacity and shared RNG. It
resolves the live registered base/unarmed actor and pose, takes current/next
controller states, actor flags, retained action824 and mapped availability,
then calls the verified420c00 selector. Clearance requests use the existing
registered scene geometry/actor query. It allocates nothing.

The selector fields previously named damage138c/1390 are now current138c
and next1390:42a650 tests logical animation states, not damage types. Existing
controller reconstruction already retains these fields. The field rename
does not change layout or the reconstructed selector's arithmetic.

Query or validation errors preserve output and shared RNG by using a local
RNG copy until success. Caller scratch may change. Actor state is read-only
in selection; the composed stage commits the returned action later. The
registered fixture verifies random choice/RNG, current13, next13 and crouch
selection, stale handles, clearance failure preserving output/RNG, and
selection through the composed animation stage into actual motion playback.

Both builds and all19 CTests pass. The8192-case original/PC/NXDK selection
comparison also passes exact choices, RNG and query order; its clearance
boundary is supplied. New ownership composition runs on PC and compiles
into NXDK, without a new live XEMU death test. Scene clearance still uses
stationary NPC orientation; moving orientation, effective class changes,
audio ownership and full death scheduling remain open.


## Action sound callback for death animation (2026-09-12)

Original428c90 starts motion, resolves the action's group through434da0 and,
for a present sample, calls5056a0(sample,actor+3c,1,173c378,0). It uses actor
position rather than the eye position used by pain playback, and stores no
voice handle. rf_scene_npc_death_sound now implements the composed stage's
sound callback using the same rf_scene_death_selection_context as selection.
It resolves the declared Foley label, uses the shared group/RNG selector,
then reuses bounded campaign sample loading and unity spatial playback at
the registered NPC's published position. Action telemetry is separate from
pain counters. Missing groups/samples skip playback, matching original
absence handling; malformed owned groups fail. No cooldown is introduced.

The composed registered-NPC test now uses both production callbacks: one
RNG draw chooses a death action, the following draw selects from a two-entry
sound group, and final RNG/action agree with that order. Its samples are-1,
so this tests selection/order and absence handling, not waveform playback.
Missing labels, stale handles and malformed groups are also checked.

All19 CTests and both builds pass. Existing sound-dispatch verification
passes1024 original group selections (258 linked NXDK comparisons) and256
original wrapper dispatch cases. That shared helper evidence does not prove
new death callback device output. Run a campaign death-animation harness
with real declared samples and native audio capture before claiming audible
XEMU death playback. Complete death scheduling remains open.


Campaign death-animation fixture - 2026-09-12

The optional xemu_replay_check.py --death-animation --damage-uid 8456
fixture runs the composed base/unarmed NPC animation stage at frame120.
It uses registered clearance/selection, retained pose reset and bone clears,
actual mapped motion loading, shared RNG and authored action sound loading.
It resets the requested/current death action to -1 before dispatch; this is
a controlled animation-stage fixture, not lethal damage or full dying/corpse
scheduling. Scratch clearance storage is1580 bytes for this case and freed
after the call. PC enablement is headless-only; Xbox uses an explicit disc
flag. The replay harness preserves/restores that flag, including on failure.

Replay artifacts/xemu/replay-20260912-034940/report.json passes180 frames
with --door --damage-uid 8456 --death-animation --audio-capture. Native
memory reports67108864 base bytes and zero plugged memory. PC/Xbox match
frame120, handle16843008, action5, flags8, status0 and RNG3357800067 to
415139642. Action audio reports one selection, load and playback request,
sample662,59636 PCM bytes and no errors. Total bank accounting is410546
bytes across11 loaded waveforms. The initial replay exposed an outdated
bank assertion that omitted the new action-sound bytes; the corrected
accounting and full rerun pass. All19 CTests and PC/NXDK builds pass.

The native8192-byte DSP ring contains4037 nonzero samples. This proves
combined scene device output, not isolated death-sound audibility. Complete
death scheduling, armed/effective class views, corpse creation, enabled
override visuals and real hardware remain open.


Resolved falling predicate and death-entry verification - 2026-09-12

rf_entity_falling reconstructs42a020 for a present actor. Modes3/8 return
true directly; otherwise429990/486c90 must resolve physics use-kind1 and
actor1380 contact material must equal-1. It reads no command714 data, so
resolving this predicate before the shared entry helper cannot observe that
helper's first vector clear. The campaign NPC support probe now uses this
shared predicate in place of its identical inline rule.

verify_death_entry_falling.py executes original42a020,429990 and486c90
without helper hooks. It exercises756 combinations of object families,
movement modes, use kinds and contact materials, including noncanonical
upper bits:530 grounded and226 falling. Shared PC/NXDK results match.
Each case also runs both fresh and already-dying original41fdc0 prefixes
with the real predicate (1512 entry cases), stopping at48c9f0 collision
teardown. Selected entry fields match PC/NXDK and every other actor byte
is preserved. The existing4096-case supplied-low-byte suite still passes,
as do all19 CTests and both builds. No new native XEMU run is claimed.

Live entry still needs command714 storage/initialization ownership and
body-field publication. Collision teardown, timers, death scheduling and
corpse creation remain integration work; this predicate adds no allocator
or replacement lifecycle.


Registered NPC death-entry binding - 2026-09-12

Original422eaf loads actor+714 into ECX and422ed3 calls4fad00, explicitly
zeroing the three command words during creation. campaign_npc_body now
retains command_714[3], zeroed by its existing calloc. The twelve added bytes
per actor are included in sizeof-based owner budgeting (936 bytes for78
opening actors). AI/steering command production is still open.

rf_scene_npc_death_entry resolves a registered handle, returns entered=0
without any owner changes when already dying, and otherwise resolves the
verified falling predicate from retained movement slot, class use-kind and
support material. Embedded physics at actor88 maps bodybc/c8/120 to
actor144/150/1a8: velocity, vector_c8 and body flags. Shared entry results
publish to these fields, command714 and authoritative view.flags810, also
mirroring flags into damage.effects. No allocation or later death callbacks
are performed. Invalid handles/state preserve the owner and output.

The registered-owner test covers192 mode/use/material/dying combinations,
whole-owner preservation outside the intended writes, repeated calls, stale
handles, null output, invalid movement slot and invalid class. All19 CTests
and PC/NXDK builds pass. Native stock64MiB replay
artifacts/xemu/replay-20260912-035803/report.json passes180 frames with the
new retained allocation and the existing death-animation/sound fixture.
That replay does not call the new death-entry adapter; native entry-binding
activation remains unverified. The original predicate/entry instruction
comparison evidence is recorded in the preceding section.

Do not call this alone as a full death handler:48c9f0 collision-pair
retirement, global timers and downstream effects still need composition.
The shared collision-pair retire helper exists, but a live campaign pair
list owner has not yet been connected; an empty substitute is not evidence
of original collision teardown.


Collision pair pool and creation - 2026-09-12

Original48c950 seeds records73db30 through75db20 at16-byte stride:8192
records,131072 bytes. It prepends in ascending address order, leaving the
highest address at the free-list head. Only next pointers change; endpoint
and flags payload is preserved. It then registers an empty48c980 exit
callback. rf_collision_pairs_seed reproduces list seeding with explicit
caller-owned storage, preserving a preexisting available list/count. It is
not a per-frame reset and must not be called twice on linked storage.

rf_collision_pair_record adds the fourth word (flags) after the existing
retirement header. rf_collision_pair_create reconstructs48bd80: zero local
flags, call gate48be00, reject only when its low byte equals1, then check
free capacity. Pop the free head, prepend to active, and write both actor
identities and returned flags. Counters retain unsigned wrap behavior. The
gate can modify the lists; free capacity is observed after it returns.
The shared API requires valid exclusive lists and a live classification
callback; it does not substitute a permissive gate or deduplicate pairs.

verify_collision_pool.py runs original48c950 up to its CRT exit registration
and compares all8192 next pointers and payload words with NXDK output.
It then compares2048 original48bd80 creation cases with PC/NXDK, including
1364 successful allocations, empty capacity, arbitrary free/active order,
noncanonical gate bytes and counter wrap. Only48be00 classification is
supplied; original list helpers execute unchanged. Existing4096 retirement
cases (43750 removals), all19 CTests and both builds pass. Seed verification
is original/NXDK; creation verification includes PC. No XEMU run is claimed.

The pool is not yet allocated in the campaign. Its 128KiB plus list headers
must be budgeted when discovery is connected.48be00 uses object-family,
body flags, use-kind, size and model checks, and is not reconstructed by
this change.48ca60 traverses active pairs, checks retirement via48cc10 and
selects response functions from flags;49b900 queries active pairs during
sweeps. These consumers and discovery scheduling must be connected before
claiming live death collision cleanup.


Actor/actor collision classification - 2026-09-12

rf_collision_actor_pair_reject reconstructs48be00 for two kind0 actors,
including self/null and common body/object/global-byte gates. Its resolved
view retains object/body flags, use-kind, primary/secondary weapons and
primary weapon definition flags264, extent180 and case-insensitive equality
with Sea_Creature. Both inputs must be actor views; this is not a fallback
for unimplemented object families. Classification returns1 to reject and0
to allow, preserving initial pair flags except original assignment sites.

Original4895d0 reads object7c bit8.429990 compares486c90 use-kind to1.
408d90 requires primary!=-1, secondary==-1 and primary definition264 bit20.
500290 returns string inequality, so unequal Sea_Creature labels select
flags4 or2 on the corresponding path. Unarmed ordinary actors require
both body flags40 and at least one extent180>=2 unless earlier use-kind
logic accepts them. Other early flags and the special-weapon comparison
retain their original precedence.

The player branches are intentionally asymmetric:48bed0..48bef8 compare
twice-A with B, then twice-B with A;48bf5c..48bf7c compare twice-B with A
twice. The second comparison reuses the x87 value without a float spill.
The shared code uses double arithmetic for these doubled float operands
and preserves this order-dependent boundary rather than symmetrizing it.

verify_actor_pair.py passes8192 original/PC/NXDK cases with no substituted
helpers: full original48be00 runs for kind0/kind0, including original
player/use-kind/weapon/string routines. It checks result, exact flag writes
and unchanged actor facts. Coverage includes512 directed finite size-grid
cases in both orders, common rejection flags, alternate/network low bytes,
weapon/use-kind combinations, case-insensitive names and null/self. Results:
5289 rejections preserve flags;1682 accepts preserve flags;632 assign32;
229 assign2;360 assign4. Nonfinite dimensions are not covered.

Both builds, all19 CTests and existing pool verification pass. No live
classification binding or XEMU invocation is claimed. Other families in
48be00, especially projectiles/items/solids and their helper dependencies,
and broad-phase discovery remain open before campaign pair creation.


Collision discovery traversal and scheduling audit - 2026-09-12

The direct caller of pair allocation48bd80 is48c9a0 (call48c9d4). It reads
global object-list head73d890 and scans next links at object10 through the
73d880 sentinel. For kind2 with definition268 bit20 it first calls48bbe0,
then rereads the global head. Every candidate reaches pair creation, even
self; filtering belongs to48be00. It reads next AFTER the creation call and
ignores creation failure, so exhausted pool capacity does not stop scanning.
rf_collision_pairs_discover reproduces this traversal with explicit object
tokens and callbacks for preparation, creation and next-link resolution.
The callback supplies a valid finite list; no artificial iteration cap or
deduplication changes original behavior.

verify_collision_discovery.py passes1024 original/PC/NXDK cases with171
preparation calls and12723 pair attempts. Original48c9a0 executes;48bbe0
and48bd80 are supplied boundaries. Empty, full and permuted lists include
self candidates, preparation changing the head and creation changing the
current next-link. Callback traces, resulting links/head and continued
traversal on failure match. This does not verify actual projectile prepare
math or classify/create resources inside those callbacks. Both builds and
all19 CTests pass; no native XEMU run is claimed.

Direct48c9a0 call sites:4109cc,41302e,413263,416ed6,423968,4593a4,
48a761,4bfb8f,4c7c9b. Actor creation at423968 runs discovery only when
body1a8 bit20 is set and object7c bit8000 is clear. Unhide48a660 calls it
at48a761 when bit8000 is set, then clears that bit. Their full surrounding
resource effects are not newly reconstructed here. Other callers still need
individual scheduling audits. Pair processing48ca60 is called at487849
inside physics substep loop487770 after body preparation, before the SP
49bb70 pass. These are distinct discovery/processing phases; do not replace
them with a per-frame full pair scan.

Next: complete classification for other object families, projectile prepare
48bbe0 and eligibility48c7f0, then bind the actual global object-list lifetime
and creation/unhide dispatch before using the pool for death cleanup.


Projectile pair eligibility - 2026-09-12

rf_collision_projectile_eligible reconstructs48c7f0 from resolved inputs.
409fa0 computes target3c minus projectile3c with float storage;40a0b0 dots
that displacement with projectile60 in Z/Y/X order. Negative projection
rejects, so this is a forward-axis gate, not a distance limit. Mode low byte
zero then accepts immediately, as used by existing-pair retirement48cc10.

For nonzero mode the original resolves426fc0(projectile30). A present owner
with owner1f8=0 and target1f8=0 rejects kind0 targets whose2c handle differs
from owner560. Definition268 bit20 then enables four planes at75db38,
stride16. Each plane tests targete4 (not target3c) with Z/Y/X dot plus planeD
against the negative sum of target/projectile180 extents; strictly smaller
rejects, equality accepts. The shared152-byte input is caller-owned scratch
and includes resolved owner facts and plane coefficients, with finite
geometry and the established53-bit arithmetic requirement. It does not
perform owner lookup or create the global planes.

verify_projectile_eligibility.py compares8192 full original48c7f0 cases to
PC/NXDK under x87control027f:5339 reject,2853 accept,2937 owner lookups.
Plane-count histogram for0..4 tests is6650/635/363/157/387. The verifier
supplies only426fc0 owner resolution; original subtraction, dot, plane and
kind helpers execute unchanged. It checks boundary equality/adjacent float
values, arithmetic cancellation, owner conditions, mode upper bytes and
input preservation. Both builds and all19 CTests pass. No native XEMU
activation is claimed.

The general classification switch still needs integration for projectile
object pairs. Its eligibility mode1 and retirement48cc10 mode0 must remain
distinct. Recover48bbe0 plane production and registered projectile/owner
state before connecting these checks to live discovery and physics.


Existing projectile pair expiration - 2026-09-12

rf_collision_pair_expired reconstructs48cc10 using the same forward-axis
calculation as projectile eligibility. Pair flags mask1 enables the test.
The first endpoint is chosen if its kind is2; otherwise the second must be
kind2, or the pair remains. If both are projectiles the first wins. Original
48c7f0 is called with mode0, so only the float-stored target-minus-projectile
displacement and forward dot decide expiration. Negative dot retires; zero
retains. No owner resolution, definition flags or global planes participate.
The shared60-byte resolved view is read-only and does not remove list nodes.

verify_collision_expiration.py passes4096 original/PC/NXDK cases:3341 retain,
755 expire. It executes full original48cc10 and48c7f0 with all actual callees
and no hooks. Both-projectile precedence, either endpoint, neither endpoint,
flag gates, zero boundary and finite cancellation are checked under027f.
Owner/definition fields are poisoned and every original actor/pair byte
is checked unchanged. The existing8192 eligibility cases still pass after
factoring the shared directional calculation. Both builds and all19 CTests
pass; no native XEMU run is claimed.

This predicate is ready for the48ca60 pair processor, whose response
selection and mutable-list traversal still need composition. Pair expiration
is not the actor-specific48c9f0 death cleanup: keep their triggers separate.


Collision pair processing loop - 2026-09-12

rf_collision_pairs_process reconstructs full48ca60 control flow around
resource callbacks. Save next before queries; expiration low byte exactly1
removes the pair. Otherwise either body flag40000000 enables processing.
Kind5 uses48bb00 and removes on zero low byte. Other kinds call48bb90;
zero retains the pair without response. Successful bounds queries reread
pair flags and actor facts before response selection.

Response precedence: flags20 selects49ab00 only when both movement modes
are1, otherwise49a420; flags4 plus second model selects49afe0(first,second);
flags2 plus first model selects49afe0(second,first); flags10 with second
kind3 selects49b570(first,second); flags8 with first kind3 selects reversed
49b570; fallback49a420. Removals unlink from active and push onto available,
keeping previous unchanged for consecutive removals. The loop advances to
the cached next. Resource callbacks preserve node/list lifetime; actor lookup
is pure and returns a live resolved view. No heap allocation is added.

verify_collision_process.py passes2048 original/PC/NXDK cases. Original
48ca60 and list helpers run unchanged; expiration, kind5/bounds and response
functions are supplied. Operation counts:8182 expiration queries,1545 kind5
queries,3575 bounds queries,460 mode1 responses,1147 general responses,
313 model responses and122 solid responses. Exact ordered callback traces,
list heads/links/counts, record flags and endpoint identities match, including
empty/full/permuted lists, consecutive removals, unsigned count wrap, query
upper bytes and bounds callbacks replacing flags. Both builds and all19
CTests pass. No native XEMU or live collision response claim is made.

Next bind actual48cc10,48bb00 and48bb90 queries and response resource
owners. Discovery/classification/global-list integration remains required;
the processor is not invoked in campaign physics yet.


Correction and integration of48bb90 response filter - 2026-09-12

The preceding processor section called48bb90 a bounds query. That label was
incorrect: the actual function rejects first200==second2c or second200==
first2c, then calls40a110 on both actors.40a110 checks object7c mask4000.
No geometry is tested. rf_collision_pair_response_allowed now implements
these parent-handle and visibility conditions with direct raw handle
comparisons, preserving the original behavior even for equal sentinel values.

rf_collision_pair_actor_state now includes actual handle, parent handle and
object flags. The processor calls the shared filter directly instead of a
supplied callback; RF_PAIR_BOUNDS_TEST has been removed. Test wire fixtures
and enum values were updated accordingly. This is port-facing reconstructed
state, not a claim about the original object layout or new live scene storage.

verify_collision_process.py now runs actual48bb90/40a110 as part of original
48ca60 while PC/NXDK use the shared filter.2048 cases pass with exact lists
and callback traces:8182 expiration checks,1540 kind5 calls,418 mode1
responses,1063 general responses,298 model responses and110 solid responses.
Input coverage includes parent matches and visibility flags. Expiration,
kind5 and response effects are still supplied boundaries; they are not
newly bound by this change. Both builds and all19 CTests pass. No XEMU
or campaign activation is claimed.

48bb00 is also not merely geometry: it selects the kind5 endpoint (first
wins), optionally filters mode2 through an external handle list, then calls
4bfc60(kind5,other,0) and returns1. That list and dispatch need an explicit
audit before replacing the remaining kind5 callback.


Trigger membership routing integrated into pair processing - 2026-09-12

The kind5 path48bb00 uses the selected trigger's own vector at2d4, not a
global handle list: signed count at2d4 and storage pointer at2dc via
40a490/40a480. First kind5 wins when both endpoints qualify. Filter2 at2c4
searches for the other object's2c handle. Empty/no-match returns0, causing
the processor to retire the pair. Other filters dispatch directly.
Successful routing calls4bfc60(trigger,other,0) then returns1 irrespective
of contact/activation outcome. This means retain the pair, not event fired.

rf_collision_pair_trigger_dispatch now implements that routing, and
rf_collision_pairs_process calls it directly. The actor view adds filter,
signed allowed-count and borrowed allowed-handles. The previous kind5 query
callback is replaced by RF_PAIR_TRIGGER_CONTACT with ordered trigger/other
identities and implicit original input0. The original search has no mutating
callbacks; a stable list snapshot suffices. Negative count behavior is also
preserved: no scan, zero differs from the negative count, so dispatch occurs.
Live owners must still maintain valid lists; this is fidelity evidence, not
an instruction to create malformed trigger storage.

verify_collision_process.py now executes real original48bb00,40a490 and
40a480 in addition to48ca60/48bb90/40a110/list helpers.2048 original/PC/NXDK
cases pass with967 contact dispatches,431 mode1 responses,1054 general
responses,283 model responses and145 solid responses, plus8182 supplied
expiration queries. Trigger lists include empty/missing/matched entries,
negative counts and either/both kind5 endpoints. Exact traces, contact
input0, pair topology/counts, endpoint identity and flags match. Both builds
and all19 CTests pass. No XEMU or campaign event-firing claim is made.

Actual4bfc60 contact/activation and physics responses remain resource
boundaries. Existing shared trigger contact code can supply part of this
work, but full activation effects, pair ownership and frame placement still
need integration. Next replace supplied expiration with the verified
projectile predicate and connect live object/trigger views.


Expiration integrated into pair processing - 2026-09-12

The pair processor now resolves each endpoint's position3c and forward60
and calls the verified shared expiration predicate directly for flags mask1.
The supplied RF_PAIR_EXPIRED callback is removed. Actor views now carry
six additional floats (64-byte total view on32-bit builds); these are
resolved views, not newly allocated campaign actors. Temporary expiration
input is60 stack bytes only on flagged pairs. Existing endpoint precedence
and mode0 behavior remain unchanged.

verify_collision_process.py now executes original48cc10/48c7f0 plus actual
48bb00 membership and48bb90/40a110 filtering inside48ca60. Only4bfc60
contact and the four physics response effects are supplied boundaries.
2048 cases pass on PC/NXDK, including1237 actual directional eligibility
calls. Contact/response trace counts are1386/533/1293/335/189 (trigger,
mode1, general, model, solid). List topology/counts, flags, identities and
ordered effects match with finite geometry under027f, signed trigger counts,
parent/visibility filters and counter wrapping.

The separate4096-case expiration verifier, all19 CTests and both builds
also pass. No XEMU invocation or live campaign collision response is claimed.
Next supply actual live object views, trigger contact/activation and physics
response owners, retaining original discovery and substep scheduling.


### Registered NPC collision view (2026-09-12)

rf_scene_npc_collision_view resolves a registered kind0 NPC and snapshots the
retained body flags, movement descriptor index, published position, parent
handle and object flags. Original object200 maps to view.linked_handle (also
covered by verify_entity_predicates.py); body120 maps to actor1a8. The model
token is actor slot+1 only while campaign_actor_pose publishes its model;
empty/retired or corpse-transferred models produce zero. Invalid resource
owners fail without publishing a partial output. No allocation is added.

Forward follows authored orientation[2], matching current NPC model placement
and clearance, not an assertion that moving orientation ownership is complete.
Trigger fields are neutral for this kind0-only view. Callers must refresh the
snapshot after mutations; no pair pool, discovery scheduling or response effects
are enabled by this adapter. Player, trigger and projectile views remain open.

Validation: npc_collision_binding_check in npc_motion_residency checks stale
handles, model publication/transfer, live state changes, unsupported kinds and
invalid owners preserving output. PC and NXDK builds pass; all19 CTests pass.
This adapter has not yet been exercised in native XEMU gameplay.


### Player collision view and borrowed model publication (2026-09-12)

The current player model belongs to animation_run, outside the NPC model-slot
registry. rf_animation_placement now optionally publishes a borrowed view of
that actual model file, bones, evaluated matrices and playback state. Publication
starts after the first successful pose evaluation/skinning preparation and is
cleared before any referenced storage is released on every cleanup path. It is
absent during initial setup callbacks; consumers must not retain it after the
stream returns. Nonempty publication destinations are rejected on entry. This
adds no heap allocation: the view is20 bytes on Xbox, plus one pointer in the
placement and one scene publication pointer. This remains the miner player
diagnostic's model lifetime, not completed original player creation/death.

rf_scene_player_collision_view requires the registered kind0 player, live body
and model publication. It reads descriptor index through campaign_modes using
rf_scene_actor_landing[1], body1a8 flags, linked parent handle, object flags,
published object position and body forward axis (not camera shake/eye axes).
Model token UINT32_MAX is reserved for this owner, distinct from NPC slot+1;
future model response backends must resolve it accordingly. Missing/stale owners
fail without publishing partial output. Pair scheduling/responses remain open.

Validation: player_collision_binding_check covers mapped state, stale handles,
missing model and invalid movement slot. rf_npc_residency_tests
--model-publication Installed_Game/meshes.vpp Installed_Game/motions.vpp passes
with real miner assets for normal completion, sink failure and archive-open
failure; the initial test needed a valid camera placement instead of a zero
projection. All19 CTests and PC/NXDK builds pass. Native XEMU execution of the
new player snapshot remains unverified.


### Native collision view replay (2026-09-12)

Campaign scene frames now exercise the registered player and NPC snapshot
bindings after physics and NPC playback. Eight telemetry words retain completed
frames, cumulative player/NPC hashes, NPC snapshot count, current model counts,
error and last frame. Hash input is an explicit16-word wire layout with no
host pointers or struct padding; current kind0 views have no trigger list.
The32-byte summary and bounded stack scratch add no heap allocations. This
read-only check does not create collision pairs or dispatch response effects.

Native report artifacts/xemu/replay-20260912-045001/report.json passes the
180-frame door/miner death-animation replay. XEMU reports67108864 base bytes
and zero plugged memory. Collision summary exactly matches PC:
[180,3103947230,2575274397,14040,4294967295,78,0,179].
All180 player and14040 NPC snapshots resolve successfully; all78 NPC model
owners remain published in this animation-only fixture. Existing death animation
and action audio summaries also match. All19 CTests pass. This supersedes the
previous native-unverified notes for the snapshot adapters, without establishing
full collision handling, actor death or moving NPC orientation ownership.


### Actor response directional ray/sphere helper (2026-09-12)

49ab00 uses strict AABB overlap46c340, maximum sphere radii from actor184
lists, and a horizontal relative-motion ray before response field updates.
Its ray constructor467620 only initializes two vectors; it is not a collision
query. The actual sphere intersection is508e40, now rf_collision_ray_sphere.
Raw Ghidra exports for49ab00/49a420/49afe0/49b570 and helpers are local evidence;
this change reconstructs508e40 fully, not those response routines.

508e40 rejects zero length and nonpositive forward projection before checking
initial overlap. Projection is stored as float for later discriminant arithmetic;
the initial comparison uses its unrounded53-bit value. Vector subtraction,
multiplication and addition retain their original float stores and dot order.
Accepted ordinary hits return a normalized fraction and point. A discriminant
candidate beyond ray length writes its unnormalized distance then rejects,
leaving the point untouched. Earlier misses preserve both outputs. Do not
replace this with an all-or-nothing generic intersection API or silently fix
the caller's additional fraction scaling. Finite, nonaliasing input contract.

verify_collision_ray_sphere.py executes the complete original508e40 and vector
callees without hooks. All8192 fixtures match PC and NXDK return values, point,
fraction and input preservation:7654 misses,538 hits,78 misses updating distance.
Coverage includes zero length, initial overlap, forward rejection, tangency
and adjacent float boundaries under x87 control027f. Report is local
artifacts/collision-ray-sphere.json. PC/NXDK builds and all19 CTests pass.
The helper is not yet called by a reconstructed actor response or native XEMU
gameplay; contact state updates and their live owners remain open.


### Complete normal-mode actor response49ab00 (2026-09-12)

rf_collision_actors_normal_response reconstructs49ab00: strict AABB overlap,
ordered maximum-radius scans over24-byte sphere records, relative current/next
positions flattened on Y, original normalization and actual508e40 ray query.
It preserves the original additional division of the returned fraction by ray
length, including the first comparison before rounding that division to float.
Initial-overlap hits are rejected when relative direction dot hit is nonnegative.

If both stored times are later, the routine writes first contact time, negative
normalized relative hit, interpolated first-body contact point minus normal times
first radius, other material/inverse mass/velocity and handle. Original426fc0
resolves the other actor's optional8a0 velocity, which is added before handling
the second actor. The second gets opposite normal and the same contact point.
Both reference1e8 fields become-1 and1f0/1f4 become zero;1ec stays untouched.
When only one time can advance, the active body40000000 gate controls whether
to set20000000 and copy the other stored time, without overwriting contact data.

The callback supplies only a stable optional8a0 vector by handle. Current
finite/disjoint-state contract excludes reentrant mutations and invalid masses.
Zero relative motion returns no contact; the original creates unused local NaNs
then rejects zero ray length. Floating-point exception status is not modeled.
The static zero vector/atexit setup is represented by a constant local zero.

verify_actor_response.py passes4096 original/PC/NXDK cases:2976 no response,
671 both-contact updates,186 first-body deferrals and263 second-body deferrals.
All1342 velocity lookups match order. Original vector, normalization, AABB,
sphere-list and508e40 callees execute without substitution;426fc0 is the only
supplied lookup, and the static vector is preinitialized. All contact fields,
body flags, inputs and surrounding original object bytes are compared. Local
report: artifacts/actor-response.json. PC/NXDK builds and all19 CTests pass.

Live dispatch is still open. The new152-byte x86 response view is borrowed
state, not yet allocated per actor. Existing body vector138/scalar144 correspond
to original actor1c0/1cc; the rest of1b4..1f4 and actor8a0 need explicit shared
ownership before binding this routine. General49a420, model49afe0 and solid
49b570 responses remain unreconstructed. No new native-XEMU response claim.


### Complete general actor response49a420 (2026-09-12)

rf_collision_actors_general_response reconstructs49a420 and uses the actual
shared508e40 helper. After strict bounds overlap it chooses the actor with fewer
spheres as the outer loop; equal counts choose smaller extent, with the second
argument chosen on equal extent. Either body400 flag takes the original shortcut:
only outer contact time0, other handle and inverse mass are published.

The ordinary loop rotates each outer sphere using current and next orientations,
converts its relative trajectory into the other actor's local coordinates, and
tests each inner sphere in order. World-space center motion is retained separately
for the contact point. Ray normalization uses the float-stored length, unlike
49ab00. Accepted fractions are not divided by length a second time. Contact
normal uses the other actor's current orientation, followed by original4faaf0
normalization. Each accepted earlier pair updates retained times before later
sphere queries; this is not an unordered minimum-distance search.

If the outer actor is kind2, its contact is written but the second actor's contact
is suppressed. Deferral branches can mutate body flags/times while the function
returns zero: its return tracks new contact writes, unlike49ab00. The existing
extra-velocity callback resolves the original426fc0/actor8a0 read in order.

verify_actor_general_response.py passes8192 original/PC/NXDK cases with actual
AABB/list/transform/normalization/ray/vector callees; only426fc0 is supplied.
Final outcomes:5879 no-write rejections,218 ordinary contact results,1891
special400 results,204 flag/time deferrals returning zero;394 lookups. Exact
contact writes, return values, lookup order and surrounding original object
bytes match. Coverage includes zero/negative list counts, multiple spheres,
nonzero centers, quarter-turn and oblique current/next orientations, kind2 and
active-body flags. Local report: artifacts/actor-general-response.json.
Normal response4096 cases still pass; PC/NXDK builds and all19 CTests pass.

The232-byte x86 general view is borrowed and has not been allocated per live
actor. Live contact ownership, scheduler integration, native response execution
and model49afe0/solid49b570 response reconstruction remain open. No new visual.


### Solid response49b570 orchestration (2026-09-12)

rf_collision_actor_solid_response reconstructs49b570 around explicit geometry
services. Missing solid exits before bounds; disjoint bounds exit before any
cache calls. Overlap with more than one actor sphere calls4df7e0 preparation
with actor bounds and original final argument0. Every overlapping invocation
releases through4dfb00 after traversal, including zero/negative sphere counts.

Query origin/matrix come from the solid actor's current body transform; flags
are0. Sphere start uses current actor transform. Displacement retains original
rounding: rotated next sphere center plus (actor next position minus start),
not the algebraically equivalent endpoint subtraction. The first result time
is min(actor time,solid time); each query carries the service's resulting time
to the next query, including misses. Positive signed hit count triggers writes
without a second nearest-hit check in the caller. Local point/normal transform
through the solid's current orientation; point then adds current solid position.
The actor gets solid material/inverse mass/velocity/handle, reference-1 and
face1f0 from the query. Kind2 suppresses the other actor's contact. This path
does not perform the426fc0/8a0 extra-velocity lookup.

verify_actor_solid_response.py passes8192 original/PC/NXDK orchestration cases:
4525 early exits,1135 overlap/empty-list releases,712 queried misses and1820
contact results. All6074 query inputs and carried limits match byte-for-byte,
as do callback ordering, contact writes, return and surrounding original bytes.
Bounds/list/vector/matrix/minimum callees execute unchanged.4df7e0,4df1c0 and
4dfb00 are supplied services; this does not establish actual cache/query effects
or their live binding. Local report: artifacts/actor-solid-response.json.
PC/NXDK builds and all19 CTests pass. No new native response/visual claim.


### Model-response immunity branch geometry506ae0 (2026-09-12)

49afe0 resolves the second actor through426fc0 and tests42cca0 armor immunity.
If present/immune and the first object is kind2, it substitutes a unit-radius
segment/sphere query506ae0 for model geometry5031f0. The already verified
rf_entity_armor_immunity predicate can supply42cca0 once live ownership is
connected. rf_collision_segment_sphere now reconstructs506ae0 completely.

The helper retains float-rounded segment length and direction divisions,
projection and closest-point stores, then the original distance/square-root
calculation. Exact tangency rejects. Accepted intersections outside the segment
return start rather than a clamped endpoint. For a zero-length segment the
helper always writes start, including a failed strict-radius test; other misses
preserve output. This must not be replaced by508e40 or an all-or-nothing query.
The original uses static scratch vectors; the port uses local scratch without
allocation. Empty static destructor registration is outside geometric behavior.

verify_collision_segment_sphere.py passes8192 full-original/PC/NXDK cases under
027f:6938 misses,1254 hits,569 misses writing start and983 hits returning start.
Original geometry/vector/distance callees run without hooks; static constructor
flags are preinitialized. Return, point and input preservation match exactly.
Coverage includes zero length, radius/endpoint limits, strict tangency, adjacent
floats and large-coordinate rounding. Report: artifacts/collision-segment-sphere.json.
PC/NXDK builds and all19 CTests pass.49afe0 orchestration, live model query
ownership and native response dispatch remain open.


### Complete model-response49afe0 orchestration (2026-09-12)

rf_collision_actor_model_response reconstructs49afe0 around a stable426fc0
target lookup and5031f0 model query service. Actual shared42cca0 immunity and
506ae0 segment geometry execute inside the routine. Bounds rejection and the
either-body400 shortcut precede lookup. The immunity check still occurs before
the first actor kind2 gate, matching the original lookup ordering.

The projectile branch queries a unit sphere at the target's current position
directly into the projectile contact point. A miss may therefore change that
point without changing other contact fields. Hits compute time from distance
only when segment length exceeds actor extent; otherwise time is zero. Normal
is normalized (hit point minus projectile start), not a sphere normal; a zero
vector retains the original NaN result. Inverse mass is explicitly1 and only
the projectile is updated.

Ordinary model queries use flags2, zero origin and identity matrix. Sphere start
and end transform through actor current/next orientation and relative target
current/next coordinates. The result time starts at1 and survives across queries
including misses; a separate minimum accepted time starts at min(actor times).
Only a nonzero low-byte query result with a strictly earlier time writes contact.
World contact adds target position interpolated by returned time. Contact1f4
receives the model part, while1f0 is zero. The target is updated only when its
mass is strictly below twice actor mass or resolved486c90 use-kind equals1.
No extra8a0 velocity lookup is made.

verify_actor_model_response.py passes8192 complete-original/PC/NXDK cases:
3668 model queries;385 real immunity branches including194 hits and36 rejected
queries that modify contact point. Target lookup and model geometry are supplied;
original immunity, segment, bounds, list, vector, transform, distance and class
helpers run unchanged. Exact query data/time carry, return, contact fields and
surrounding original bytes match under027f, including zero-length/zero-normal
projectile cases, mass/class gates and oblique transforms. Local report:
artifacts/actor-model-response.json. PC/NXDK builds and all19 CTests pass.

All four48ca60 response destinations now have reconstructed control flow, but
model5031f0, solid query/cache services and live contact/actor field ownership
still require integration. No native-XEMU response behavior is claimed here.

## Collision preparation and retained contact ownership

The shared rf_physics_body_prepare_contact reconstructs the post-bounds tails
49f8ea..49f925 and49fdbe..49fdf9. Both set actor1cc=1, actor1e4=-1,
zero actor168/174 and set body01000000. Other contact bytes are preserved,
including point, normal, material, inverse mass, velocity, references and face.
It must run after predicted movement and swept bounds, before pair responses;
calling it after responses would discard the selected collision time/handle.

The existing embedded body starts at actor88: vector138 is actor1c0 normal,
scalar144 is actor1cc time, reference15c is actor1e4 other handle, word164 is
actor1ec reserved, and word168 is actor1f0 face. Missing contact fields remain
actor1b4 point,1d0 material,1d4 inverse mass,1d8 velocity,1e8 reference and1f4
part. Future live ownership must map these existing fields rather than add
a second authoritative copy. Original fresh49f010 leaves the missing fields
untouched; its constructor does not establish zero contact payloads.

verify_physics_prepare_contact.py passes2048 cases across both original tails
with real4fad00 callees and no hooks, PC and NXDK. Full actor-byte preservation
and all308 retained body bytes match; arbitrary bits, flags, repeat calls,
guard bytes and NULL rejection are checked. No body-size increase/allocation.
Both builds and all19 CTests pass. Report:
artifacts/physics-prepare-contact-verification.json. Preceding motion/bounds,
live scheduling and native XEMU execution are outside this verification.

## Compact collision contact storage adapter

rf_collision_contact_extra retains only the40 missing bytes: point, material,
inverse mass, other velocity, reference and part. rf_collision_contact_read
and rf_collision_contact_write gather/scatter the complete68-byte response
payload using the existing28 bytes in rf_physics_body_state. Float payloads
are copied as bits, preserving stale values and NaNs. The adapter never
changes body flags, motion, bounds or accumulators; callers must publish
response flags separately, including general-response return0 deferrals.

verify_collision_contact_storage.py passes4096 independent original actor
layout mappings on PC and NXDK, checks all308 body bytes and40 extension
bytes, guard bytes, source preservation and six NULL failure combinations.
This is a new storage adapter, not an original routine: original offsets
are established by the earlier original-executable response verifiers.
Both builds and all19 CTests pass. Local report:
artifacts/collision-contact-storage.json. No live extension allocation,
response dispatch or native-XEMU integration is claimed.

## Registered NPC contact ownership

Each campaign NPC now retains the40-byte contact extension plus4-byte actor
material. Original486eb8 loads factory parameter+10 and486ec6 stores actor1fc;
creation uses the already resolved config.material.index. These44 bytes are
included by the existing sizeof-based512KiB owner budget. Allocation zeros
the inactive extension as a port storage policy, not as claimed49f010 behavior.

rf_scene_npc_collision_response validates the registered handle/model owner
and body, borrows its ordered spheres, gathers complete contact state, and
copies current/predicted body position/orientation, bounds, mass and velocity.
It does not substitute published render position or model-origin radius for
physics position/extent. rf_scene_npc_collision_publish validates ownership
and publishes contact plus response flags, preserving all other actor fields.
Neither API schedules a response or supplies actor8a0 extra velocity.

The extended npc_collision_binding_check passes in the19-test PC suite: stale
handles, unavailable body, invalid sphere count/storage, empty spheres, NULL
arguments, borrowed sphere identity, distinct predicted/body transforms,
contact bit patterns, flags publication and complete unrelated-owner
preservation. PC build succeeds. Native NXDK build and180-frame stock64MiB
XEMU replay pass: artifacts/xemu/replay-20260912-053555/report.json.
78 NPC owners add3432 bytes; body allocation telemetry is resident55732,
peak430372 bytes under524288. Existing collision-view hashes remain unchanged.
The replay verifies allocation and existing behavior, not execution of the
new response snapshot/publication APIs on Xbox. Player binding, support extra
velocity refresh41e370 and automatic live response dispatch remain open.

## Player response contact ownership

rf_scene_player_collision_response/publish now use the registered campaign
player, existing scene_actor_body and borrowed model publication gate. The
player owns40 contact-extension bytes plus4 material bytes; stream setup
resets the inactive extension and retains physics_config.material.index.
The same collision_body_response mapping now serves both NPCs and player,
using physics current/predicted transforms and borrowed sphere storage.
Publication changes only contact fields and requested body flags.

Extended player_collision_binding_check covers distinct physical/published
positions, pending orientation, material, bounds, borrowed spheres, complete
contact bit patterns, flags, unrelated-body/view preservation, stale handles,
NULL arguments, lost model publication, invalid sphere storage and empty
spheres. NPC regression checks continue to pass. PC/NXDK builds and all19
CTests pass. This turn does not run XEMU: native execution of the new response
snapshot/publication APIs remains open, together with actor8a0 velocity
refresh, pair scheduling and automatic response dispatch.

## Native response snapshot parity

The per-frame campaign collision check now calls registered player and NPC
response snapshot APIs and hashes228 explicit scalar/contact/transform bytes
plus20 meaningful bytes per sphere. Borrowed addresses, structure padding and
opaque sphere word14 are excluded. The six-word24-byte diagnostic records
completed frames, separate player/NPC hashes, NPC snapshots, status and last
frame. PC prints COLLISION_RESPONSES; xemu_replay_check reads the native symbol
and checks exact PC parity and frame/actor coverage. No contact is published
and no collision response is synthesized by this diagnostic.

PC build and all19 CTests pass; native NXDK build and180-frame replay pass
on67108864 bytes with no plugged memory. Report:
artifacts/xemu/replay-20260912-054139/report.json. Response summary:
[180,906628816,3407848021,14040,0,179]. All180 player and14040 NPC snapshots
completed;78 NPCs, body resident55732 and peak430372 bytes. Door/damage/death
animation/audio replay checks also pass. Native response publication, extra
velocity refresh and automatic pair dispatch remain unverified/unconnected.

## Registered response extra-velocity lookup

Original422f35..422f3b clears NPC actor8a0 with4fad00. Each retained NPC now
has the corresponding12-byte support_velocity vector, zeroed by its owner
allocation. Player lookup borrows existing campaign_support_velocity instead
of creating a duplicate. rf_scene_collision_extra_velocity resolves registered
kind0 handles to these stable vectors without requiring model publication.
Original426fc0 calls40a0e0 and rejects nonzero actor24; its disassembly confirms
this type/handle gate. Unowned actor families remain unsupported.

NPC/player binding tests check borrowed identity, changed values, stale and
unregistered handles, wrong actor type and lookup without model publication.
The existing response telemetry now requires successful lookups and hashes
their12 bytes per actor/frame. PC build and19 CTests pass. Native NXDK build
and180-frame stock64MiB replay pass:
artifacts/xemu/replay-20260912-054555/report.json. NPC ownership adds936 bytes
for78 actors. Snapshot parity includes live player support velocity and
constructor-zero NPC vectors. This does not connect NPC support refresh or
response dispatch. Existing rf_physics_support_refresh already reconstructs
41e370/40a420 numerical copy/wake behavior; its scheduler must be connected
to the NPC support handle and resolved moving support before claiming motion.

## Bound actor-pair response execution

rf_scene_actor_pair_response resolves both registered player/NPC snapshots
before mutation, calls49ab00 or49a420 through the verified shared response,
uses registered actor8a0 lookup, and publishes both contact states and body
flags regardless of response return. Caller selects normal_mode0/1 from its
48ca60 dispatch decision; the adapter does not classify, create or schedule
pairs. It also does not apply impulses or damage. Stable owners and finite
geometry/positive masses retain the response routines' preconditions.

actor_pair_binding_check covers18 combinations: NPC/NPC, player/NPC and
NPC/player; normal/general; ordinary contact, strict bounds rejection and
one-sided deferral. Full response snapshots match the separately executed
shared routines; contacts target opposite registered handles. General
deferral explicitly returns0 while publishing body60000000 and time0.
Stale second handles, self-pairs, invalid mode and NULL result preserve
retained bodies/contact extensions and the result. PC/NXDK builds and all19
CTests pass. No new XEMU execution is claimed for this adapter; native
publication and automatic48ca60 dispatch remain open.

## Native registered pair publication fixture

Opt-in --actor-pairs in xemu_replay_check enables RF_REPLAY_ACTOR_PAIRS on
the headless PC run and campaign-actor-pairs.flag on Xbox. The script restores
the prior flag afterwards. Frame0 selects two registered NPCs and the current
player, saves their complete bodies and contact extensions, temporarily
installs simple sphere trajectories, runs18 normal/general contact, bounds
rejection and deferral cases in all three actor pair orders, and compares
published response snapshots with direct shared response results. All three
owners are restored on success or failure. No scheduler, impulse or damage
behavior is introduced by this test; scratch is stack-local, no allocation.

PC/NXDK builds,19 CTests and180-frame stock64MiB XEMU replay pass:
artifacts/xemu/replay-20260912-055255/report.json. ACTOR_PAIR_TEST is
[18,9,9,3,3110799993,0,3,0], including three general zero-return deferrals
that publish flags60000000/time0. Native and PC contact/flag hashes match.
After restoration, collision response summary remains
[180,2734201536,2048788949,14040,0,179]. Final body, door, death-animation
and action-audio summaries match replay054555. This proves bound contact
publication under explicit test inputs, not automatic gameplay collision
scheduling. Connecting48ca60 dispatch and subsequent impulse/damage handling
remains required. No new visual capture was warranted.

## Original model query dispatcher contract

verify_model_query_dispatch.py executes original503120 and wrapper5031f0
with only geometry54e000/54daa0/54e140 supplied.4096 cases pass:2058 wrapper
calls,1174 reset cases,2730 no-geometry rejections,683 type2 calls,416 type1
whole-model calls and267 type1 selected-part calls. Local report:
artifacts/model-query-dispatch.json. Original RF.exe SHA is checked.

Reset occurs only when lowbyte(reset)==1, before any type branch: result
time becomes1 and word1c becomes0; remaining hit bytes are preserved.
Type1 uses model+4 as ECX: part=-1 calls54e000(query,hit,reset), otherwise
54daa0(part,query,hit,reset). Type2 ignores part, uses owner=model+8 and
ECX=owner+19c0+(owner[19bc]-1)*148, and calls54e140(model[4],query,hit,reset).
This hidden pose receiver is missing from raw Ghidra output. Positive pose
counts1..4 are covered; invalid count semantics are not reconstructed.

Type3 reads part metadata (including an optional scan of flags at114 in
124-byte records) but always returns lowbyte0 and never calls geometry.
Other types also return lowbyte0. Rejection upper EAX bits are incidental;
real callers use AL. Geometry callback return is forwarded in full.
Wrapper5031f0 supplies part=-1. Tests verify receiver/stack arguments,
reset-before-callback, modified hit fields, untouched query/model/pose/part
bytes and callee cleanup. Geometry and port dispatcher remain unimplemented;
this audit avoids guessing their ABI or synthesizing type3 collisions.
No source build or XEMU run was needed for this original-executable audit.

## Shared model query dispatcher

rf_collision_model_query/all reconstruct503120/5031f0 with a borrowed
kind/geometry/pose-array view and explicit geometry backend. Type2 selects
the last148-byte pose record; type1 routes whole-model or selected-part
queries. Only lowbyte(reset)==1 initializes hit time/part. Type3/unknown
return0, omitting original inert metadata reads and incidental upper EAX
bits. Callback returns remain unchanged. The type2 callback part is-1 since
the original ignores that argument; pose arrays must be valid and nonempty.

verify_model_query_dispatch.py now also compares the PC probe and actual
NXDK dispatcher against all4096 original cases. Callback operation/geometry
identity/pose address/part/reset, reset-before-callback, query preservation,
all32 hit bytes and result values match.2058 whole-model wrapper cases and
1174 exact-low-byte resets are covered. Builds and all19 CTests pass.
Report: artifacts/model-query-dispatch.json. Geometry callbacks still stand
in for54e000/54daa0/54e140; no real model geometry, live dispatch or XEMU
execution of this new dispatcher is claimed. No allocation is introduced.

## Whole-model part traversal54e000

rf_collision_model_parts_query reconstructs54e000, including preparation
when query flag2 is clear: subtract origin from start, inverse-rotate start
and displacement, then set flag2. Reset initializes time/part only for
lowbyte1. The signed part count is re-read after every callback, so changes
are observed. Each part receives reset0; low return bytes are ORed without
normalization, except flag1 early exit returns1. The current query flag is
read after the callback. Part geometry54daa0 remains supplied.

verify_model_parts_query.py executes full original54e000 with actual53b7bc
and vector helpers, intercepting only54daa0.4096 PC/NXDK comparisons pass:
3260 part calls,2458 preparation paths,1666 initially empty counts and908
early exits. Query/result/count, each callback input/order/reset, surrounding
bytes and result match under027f. Cases include mutable counts/flags,
negative counts, callback return high bits, nontrivial transforms and stale
hit payloads. Report: artifacts/model-parts-query.json. PC/NXDK builds and
all19 CTests pass. No XEMU run or completed triangle geometry is claimed.

The newly exported54daa0 uses per-part data/LOD selection, transforms a
working ray, expands part bounds by query radius, runs508b70 and traverses
geometry batches through54dcd0. Type2 helper54e140 prepares working ray
fields50/5c then calls54e200; unlike54e000, its reset condition is any nonzero
low byte. These paths require separate reconstruction and verification.

## Model ray/plane506430

rf_collision_model_ray_plane reconstructs506430 independently of one-sided
world506550. It computes negated normal/displacement dot in Z/Y/X order,
tests the unrounded denominator for zero, and divides signed plane distance
by its float-stored value. Nonparallel tests write fraction before range
checking. Fractions outside[0,1] preserve the old point; parallel tests
preserve the complete result. Accepted points use separate float stores for
displacement scaling and start addition. Either direction is accepted.

verify_model_ray_plane.py executes full original506430 and all callees with
no hooks.8192 PC/NXDK cases pass:2742 hits,4442 misses writing fraction and
1008 misses preserving output. Tests cover endpoints, coplanar/parallel,
oblique nonunit planes, finite subnormal denominators, input/guard
preservation and exact result bits under027f. Report:
artifacts/model-ray-plane.json. PC/NXDK builds and all19 CTests pass.
No triangle containment or live model query integration is claimed.

Exports54dcd0/54dd10/54de40 establish separate thin-ray and swept-sphere
triangle paths.54dd10 uses506430 then506dd0;54de40 uses5071b0,506dd0 and
5076f0. Existing world polygon containment4e1f50 is not automatically a
substitute for model506dd0, whose projected triangle-fan arithmetic requires
its own reconstruction. Type2 triangle path54e530 also needs verification.

## Model triangle-fan containment506dd0

rf_collision_model_polygon_contains reconstructs506dd0 using contiguous
ordered vertices instead of an array of vertex pointers. It chooses the
projection axis with original strict comparisons/ties, swaps projected axes
according to the normal sign, and tests successive triangles anchored at
vertex0. Float-stored coordinate differences, the open(-0.0001,0.0001)
branch, unrounded first-coordinate lower bound, float-stored upper bound
and unrounded second-coordinate/sum comparisons are retained. It does not
check coplanarity or replace the world polygon routine. Valid finite inputs
and3..INT32_MAX vertices are required; no allocation or mutation.

verify_model_polygon.py passes12288 complete-original/PC/NXDK cases with no
hooks under027f:4177 hits and8111 misses. Coverage includes all axes/signs,
normal magnitude ties,3..8 vertices, irregular and degenerate fans, exact
vertices/edges, adjacent float values near epsilon and barycentric bounds,
and points displaced off the projected plane. Inputs remain unchanged.
Report: artifacts/model-polygon.json. PC/NXDK builds and all19 CTests pass.
Full model triangle composition and native XEMU integration remain open.


## Complete thin-ray model triangle54dd10

rf_collision_model_ray_triangle composes the verified506430 ray/plane and
506dd0 projected containment routines. The caller resolves the original
batch plane and signed vertex indices into a contiguous triangle and passes
a token representing the original triangle-record pointer. The original
ordered double dot product rejects strictly positive back-facing one-sided
queries, or negates all four plane components for two-sided queries. A hit
must be strictly nearer than the stored time. Accepted hits replace time,
point, normal and token; all rejected hits preserve the complete32-byte
result. No allocation, radius handling or part bounds is added.

verify_model_ray_triangle.py runs full original54dd10 and its actual callees
without hooks.8192 exact original/PC/NXDK comparisons pass, with334 hits
including140 flipped-plane hits. Coverage includes signed vertex indices,
batch triangle selection, one/two-sided queries, axis and arbitrary planes,
strict time limits and unchanged input/guard bytes. Report:
artifacts/model-ray-triangle.json. PC/NXDK builds and all19 CTests pass.
This is executable-level verification, not a native XEMU gameplay claim.
Swept-sphere54de40, type2 triangle54e530, complete part queries and live
model geometry binding remain open.


## Closed model polygon sphere-edge sweep5076f0

rf_collision_model_sphere_edges reconstructs the complete ordered closed
edge loop with local scratch instead of original static vectors. Bounds
start at center +/- radius and extend by each signed displacement component
(original436db0/436d70,539460,539630). Each edge passes original508b70
segment/bounds rejection before5072e0 sphere/edge testing. The nearest
strictly smaller contact wins, preserving first-edge ties; the initial
limit is FLT_MAX, so contacts at time1 remain eligible. Incoming fraction
is not a limit. Empty/nonpositive counts and misses preserve outputs.
The shared internal edge helper now permits that initial limit; the public
world-edge API retains its previous [0,1] validation contract.

verify_model_sphere_edges.py passes8192 exact original/PC/NXDK cases:
1495 hits including437 at time1, ordered polygons, closing edges, zero
movement/radius, degenerate edges and nonpositive counts. Original5076f0
runs with every bounds/edge/vector callee unchanged; only static constructor
flags are preinitialized to avoid unrelated CRT exit registration. This
verifier uses027f. Report: artifacts/model-sphere-edges.json. The existing
9013-case sphere-edge verifier plus7 invalid-input guards also passes under
its037f setup. PC/NXDK builds and all19 CTests pass.
Full swept-triangle composition and native/live integration remain open.


## Complete swept-sphere model triangle54de40

rf_collision_model_sphere_triangle composes5071b0 plane contact,506dd0
containment and5076f0 closed edges. The normal/displacement dot uses the
original Z/Y/X double ordering; nonnegative dot rejects one-sided faces or
flips the plane for two-sided queries. The initial plane contact must be
strictly nearer before either containment or edge testing. Interior hits
copy the plane normal. Edge hits also require strictly nearer time and
normalize start minus contact (not the center at impact), matching409fa0
and4fab70, with double reciprocal length and float component stores under
027f. A zero length gives(1,0,0). Misses preserve the entire hit record.
Caller-resolved plane/vertices and triangle-record token match54dd10.

verify_model_sphere_triangle.py executes complete original54de40 with all
geometry/vector callees unchanged; static constructor flags alone are
preinitialized.8192 exact PC/NXDK comparisons pass,610 hits including162
flipped-plane hits and227 non-plane edge normals. Cases exercise one/two
sidedness, radii0..2, strict time limits, signed source indices, all axes
and arbitrary finite planes/triangles; input bytes and miss outputs remain
unchanged. Report: artifacts/model-sphere-triangle.json. Both builds and
all19 CTests pass. This does not establish native XEMU or gameplay wiring.
Part54daa0, type2 geometry and live model response binding remain open.


## Part query54daa0 original execution audit

The model receiver uses parts at+4c, stride90. Part+8c points to shared
metadata: selected LOD index at+0, LOD pointers at index*4, offset at+1c,
minimum at+2c and maximum at+38. Selected LOD+40 flag10 falls back to
metadata[1]. Offset/bounds getters5044e0/504550/504570 still read the shared
metadata; they do not read the selected LOD or its fallback.
Low byte reset exactly1 clears hit time to1 and token to0. Query flags2
selects copy from start30/delta3c, otherwise subtract origin and inverse
rotate. Both branches write working start50/delta5c without modifying the
first80 query bytes. Subtract part offset from working start, expand bounds
by radius, then508b70 tests the full start+delta segment. LOD+8 batches use
stride38 with unsigned16 count at+c; each batch uses unsigned16 triangle
count at+2a, and record flags at+6 masked20.54dcd0 selects thin versus sphere
at radius<0.0001. Successful results add shared part offset back to point,
including early flag1 returns; misses leave the hit result intact apart
from explicit reset. Original traversal must retain ordering and nearest
hit behavior when multiple batches are integrated.

tools/audit_model_part_query.py executes full original54daa0 with actual
triangle/geometry helpers; an observer only records54dcd0 calls and never
changes execution.128 analytic fixtures pass:32 hits,64 LOD fallback cases,
64 empty batches, identity matrices with nonzero origin/offset, both radius
paths, flags0..3 and reset0/1/101/2. Exact query scratch, original query
preservation, translated hit/token, missed result and selected geometry
arguments are asserted. Report: artifacts/model-part-audit.json. This is
original-reference evidence, not a reconstructed part or multi-batch test.

Integration gap: rf_model_geometry currently retains vertices, triangle
indices/flags and reuse, but not stored triangle planes. Batch region4 is
already located by the file parser. Add bounded plane decoding/ownership
with budget accounting before binding original static-part geometry;
do not silently regenerate planes or use render LOD bounds as part bounds.


## Static model envelopes and stored-plane reads

Asset inspection refines the preceding ownership plan: all95 installed V3C
models have170 LODs with flags3 and no stored plane region. All427 V3M
models use magic52463344/version40000; their760 LODs have flags32(757)
or48(3), including plane region4. The same structural section/LOD traversal
parses every installed V3M. rf_model_file_open now accepts either V3C or
V3M magic with the existing strict version/range/section checks; this adds
structural loading, not complete static rendering or collision residency.

rf_model_file_triangle_plane reads16 bytes from bounded region4 at index16,
preserving little-endian bits and leaving output unchanged on missing
region(NOT_FOUND), invalid index(RANGE), or truncated region(FORMAT).
Preserve nonfinite payloads: authored talltree1.v3m contains NaN planes.
Do not reject the entire model or silently regenerate these planes; their
original collision behavior still requires verification before live use.

verify_model_planes.py compares per-batch plane byte hashes against direct
archive bytes across522 installed models,930 LODs and1737 batches, including
absent animated planes and bounds guards. PASS on PC file I/O. PC/NXDK
builds and all19 CTests pass. Format counts are1714 batches518c41 and23
batches110c21; existing vertex/triangle decoders still accept only518c41.
Budgeted static geometry ownership, remaining batch decoding and native
XEMU archive validation remain open. No plane memory is allocated by this
streamed accessor; existing animated geometry residency is unchanged.


## Authored nonfinite triangle-plane behavior

verify_model_nonfinite_planes.py scans every installed static model's stored
planes. Exactly16 triangles, all in talltree1.v3m, contain nonfinite planes;
the only plane pattern is four wordsffc00000. Their authored vertex
positions remain finite. Each triangle is exercised with32 finite queries
through full original54dd10 and54de40 and the shared PC/NXDK routines:
512 thin-ray and512 swept-sphere cases all reject and preserve the complete
hit record and input bytes. No geometry callees are replaced; only original
static constructor flags are initialized. Report:
artifacts/model-nonfinite-planes.json. This verifies the shipped pattern,
not arbitrary nonfinite geometry, floating-point status flags or native
XEMU behavior. No arithmetic change was needed. Preserve these stored
payloads when adding collision geometry ownership.

Remaining110c21 static batches have12-byte position/normal elements,
8-byte triangle/UV/link elements and2-byte reuse elements in their declared
regions. There are23 such batches, including lights/control panels; format
semantics still require original consumer verification before accepting
those through the existing518c41 decoded vertex path.569d20 only stores the
format word; it does not decode it and cannot prove consumer equivalence.


## Reconstructed complete model part trace54daa0

rf_collision_model_part_trace uses borrowed compact part/LOD/batch views
with shared offsets/bounds, selected/fallback LODs, contiguous plane/vertex
arrays and original signed16 triangle indices/flags. Triangle tokens are
base plus index*8. No geometry allocation or owner lookup is introduced.
The104-byte query contains the original80-byte input and24-byte working
start/displacement. Input remains unchanged; scratch is prepared even on
bounds rejection. Shared offsets, reset lowbyte1, flag2 inverse preparation,
radius-expanded bounds and full-segment508b70 match the original. Ordered
batch/triangle iteration calls verified54dd10 below0.0001f and54de40 at or
above it. Flag1 returns the first hit, otherwise the nearest survives;
accepted point receives the shared part offset once on exit.

verify_model_part_trace.py executes full original54daa0 with all actual
54dcd0/triangle/geometry callees and no substituted callbacks. Only static
constructor flags are preinitialized.4096 exact PC/NXDK comparisons pass,
467 hits,0..2 batches and0..3 triangles, both LOD selections, empty geometry,
three axis-permutation orientations, nonzero origins/offsets, reset/flags,
near/equal radius threshold, forward/reverse triangle depth ordering and
one/two-sided records. Exact104-byte query,32-byte result and low return
are checked; input geometry and guard bytes remain unchanged. Report:
artifacts/model-part-trace.json. Both builds and all19 CTests pass.
Retained resource binding, whole-model composition and native XEMU remain
open, as does type2 skeletal geometry. Resolved views require valid owners
and indices; file validation belongs to the resource loader.


## Composed whole-model trace54e000 through triangle queries

rf_collision_model_trace binds the verified whole-model traversal to actual
rf_collision_model_part_trace with stack-local callback context. It reuses
the104-byte query so preparation changes the original input start/delta
and flag2 once; each part then writes its separate working scratch. Empty
or negative signed part counts still execute original reset/preparation.
No part/triangle callbacks are externally substituted and no allocation is
introduced. Existing traversal retains count rereads and first-hit policy.

verify_model_trace.py runs complete original54e000,54daa0,54dcd0 and actual
thin/sphere geometry callees. Only static constructor flags are initialized.
4096 exact PC/NXDK cases pass with273 hits, signed counts-1..2, two parts
with different offsets,0..2 batches,0..3 triangles, LOD fallback, rotations,
reset and query flags, radius threshold and nearest/first-hit ordering.
It compares104-byte query,32-byte result and return, plus geometry/guard
preservation. Report: artifacts/model-trace.json. PC/NXDK builds and all19
CTests pass. This composes the geometry path but does not bind retained
model resources or prove native XEMU execution; those remain explicit work.


## Owned static collision LOD geometry

rf_model_collision_geometry_open retains one original LOD blob and compact
batch views into its position, plane and triangle regions. This follows
54daa0's direct geometry interpretation, independent of render format-word
semantics. It validates region lengths, finite positions and nonnegative
signed16 indices within the declared vertex count. Stored plane payloads
(including authored NaNs) and triangle flags are preserved. Tokens use the
file-relative triangle record offset, requiring the associated resource
owner for interpretation. No host pointer truncation is used.
Budget counts the owner, original blob and batch-view array; allocator
metadata is excluded. Original blob residency includes opaque regions,
without expanding rendering vertices or duplicating individual planes.
Absent stored-plane flag returns NOT_FOUND. Failures free partial ownership
and leave the destination empty; close is repeatable. Views borrow this
owner's lifetime. The storage representation targets little-endian x86.

verify_model_collision_geometry.py checks all522 installed model envelopes:
760 static LODs/1138 batches load, including all23 alternate110c21 batches.
Position/plane/triangle hashes and file-offset tokens equal direct archive
bytes. Exact budget succeeds, one byte below fails, deliberately shortened
attachment bounds fail after allocation without publishing an owner, and
repeated close clears state. Animated LODs reject with NOT_FOUND. Maximum
individual LOD accounting is70240 bytes on32-bit PC/NXDK layouts; this is
not a total scene memory figure. Report: artifacts/model-collision-geometry.json.
PC/NXDK builds and all19 CTests pass. Native archive execution, part metadata
ownership, combined scene residency and resource/trace binding remain open.
The alternate format's rendering decoder remains a separate unresolved task.


## Shared submesh metadata reader

rf_model_file_part_metadata exposes the original5696f0 shared offset/radius/
minimum/maximum block, read after version, LOD count and thresholds in SUBM.
Original writes are metadata+1c,28,2c,38 and agree with the getters used by
54daa0. The reader returns the matching flattened first LOD/count, validates
finite ordered bounds/nonnegative radius and section/range correspondence,
and preserves output on errors. It allocates nothing and does not derive
bounds from a selected render/collision LOD. Valid installed data is copied
without normalizing or recomputing any value.

verify_model_part_metadata.py checks all522 installed models,670 submeshes
and930 LODs. All40 metadata bytes plus8 LOD-range bytes equal direct archive
reads; invalid submesh and truncated metadata leave output unchanged.
Report: artifacts/model-part-metadata.json. PC/NXDK builds and all19 CTests
pass. This is PC archive evidence; native archive execution, owned complete
static-model assembly and live scene binding remain open.


## Complete owned static collision resources

rf_model_collision_resource_open assembles all stored-plane LOD owners and
shared part views into one lifetime. Initial selected LOD is the last in
each part's metadata range (original metadata[metadata[0]]), with the first
LOD retained as fallback for flag10. Offsets/bounds are copied from shared
metadata, not recomputed from geometry. Parts reference stable owned LOD
array elements; no archive/model-file pointer remains borrowed after open.
All LODs remain resident in this resource; future scene residency policy
must account for that explicitly. The budget sums the resource, part views,
LOD owners, batch views and original blobs once, excluding allocator
metadata. Preflight checks total before allocation; failure closes earlier
LODs and clears uncommitted ownership. Repeated close is supported.

verify_model_collision_resource.py passes all522 installed models:427
static resources with575 parts and760 LODs,95 animated resources rejected
as missing stored planes. Part offset/bounds bytes and selected/fallback
array identities equal archive metadata. Exact total budget loads; one
byte below rejects. A malformed final LOD unwinds earlier loaded LODs
without publishing ownership, and repeated close clears the owner.
Maximum complete resource accounting is169844 bytes on32-bit layouts;
this is not total scene residency. Report:
artifacts/model-collision-resource.json. PC/NXDK builds and all19 CTests
pass. Native XEMU archive/query execution and live scene residency/binding
remain open, along with type2 skeletal collision.


## Authored complete-resource query verification

verify_model_authored_trace.py rebuilds original part/LOD/batch pointers
from installed static model bytes, preserving shared metadata and raw
triangle/plane/vertex data. It executes complete original54e000 with all
geometry callees unchanged, initializing only static constructor flags.
Original result record pointers are normalized to file-relative triangle
offsets for comparison with the owned resource's token contract.
The PC probe independently opens the actual VPP/model, loads the complete
owned collision resource, then calls rf_collision_model_trace. Each authored
part gets12 sweeps: both directions on each axis, thin radius0 and sphere
radius0.25, centered on its shared bounds and offset. All427 installed
static models pass6900 exact query/result/return comparisons,6242 hits.
Report: artifacts/model-authored-trace.json. The PC probe build passes.
This validates the archive-to-owned-query composition, not just synthetic
geometry. Original memory setup is reconstructed rather than running the
original archive loader. General trajectories, native XEMU archive/query
execution and live scene residency/dispatch remain open.


## Native stock64MiB static collision archive/query gate

verify_model_authored_trace.py now writes an ignored binary plan containing
all original-verified queries and expected outputs. Shared diagnostic fixture
model_collision_fixture.h streams that plan, opens each archive/model,
loads one complete owned static collision resource under a256KiB budget,
runs every query, compares all140 output bytes and releases the resource.
It retains at most one model at a time. An opt-in disc plan triggers the
Xbox fixture before normal diagnostics; no scene state or host input is used.
The PC probe runs the identical fixture for final telemetry comparison.

python tools/xemu_model_collision.py passes native XEMU using64MiB base
memory and0 plugged memory. Report:
artifacts/xemu/model-collision-20260912-065612/report.json.
Guest and PC telemetry both equal
[1380336468,2,427,6900,6242,144612001,169844,0]: magic,complete,models,
queries,hits,output hash,peak accounted resource bytes,status. Every query
also matches its independently generated original-game answer inside the
guest. Both builds and all19 CTests pass. The harness restores the disc plan
and rebuilds the ordinary ISO on exit; emulator HDD writes use snapshot
mode and EEPROM is isolated. No screenshot was taken because this test has
no new rendered content. Live scene residency, dispatcher binding and
skeletal type2 collision remain open; no full campaign claim is made.


## Skeletal-path one-sided segment/triangle5065b0

Live campaign model owners contain skeletal registration and retained pose
matrices. The newly native-verified static path cannot replace their type2
geometry;54e140/54e200/54e530 remains required before NPC model response
binding.54e530 uses5065b0 for its thin branch. The original radius threshold
at58a290 is0.025f, distinct from static54dcd0's0.0001f threshold.

rf_collision_model_segment_triangle reconstructs complete5065b0: original
506550 one-sided segment/plane test, float-stored scale/add point formation,
and506dd0 triangle containment. Plane rejection preserves the result;
after a plane hit, fraction and point stay written even when containment
rejects. Result layout is point3 followed by fraction. Finite disjoint
inputs are required; no allocation or implicit nearest-hit filtering.

verify_model_segment_triangle.py runs full original5065b0 with all real
callees and no hooks.8192 exact original/PC/NXDK cases pass:234 hits,1499
misses writing intersection and6459 preserving result. Includes axis planes,
parallel/coplanar starts, both movement directions and arbitrary finite
planes, with input and guard preservation. Report:
artifacts/model-segment-triangle.json. PC/NXDK builds and all19 CTests pass.
Full type2 triangle behavior and skeletal/live/native integration remain.


## Complete posed triangle54e530

rf_collision_model_posed_triangle reconstructs radius-expanded vertex bounds
and508b70 rejection using the caller-supplied (potentially clipped) endpoint.
It computes the normal as(v1-v0) cross(v2-v1), retaining float difference
stores and double cross intermediates from4fb050/40caf0. Normal/displacement
dot must be<=0;4fab70 normalization defaults zero length to(1,0,0), and the
plane uses the negative ordered normal/v0 dot. Radius strictly>0.025f uses
sphere contact; thinner queries use5065b0. Interior sphere hits intentionally
replace the result without checking nearest time. Sphere edge and thin hits
require strictly smaller time. Edge hits keep the generated face normal,
not static54de40's start-to-contact normal. Misses preserve the full result.

verify_model_posed_triangle.py runs full original54e530 with actual bounds,
cross/normalization, plane, containment, thin/sphere and edge callees; only
static constructor flags are initialized.8192 exact PC/NXDK cases pass,
240 hits including27 that are not nearer than stored time. Axis-aligned,
arbitrary and degenerate triangles, neighboring0.025 threshold values,
independently clipped endpoints, input guards and miss preservation are
covered. Report: artifacts/model-posed-triangle.json. PC/NXDK builds and
all19 CTests pass. Pose/LOD traversal54e200 and preparation54e140 still
need composition with retained skeletal owners and native XEMU evidence.


## Prepared skeletal geometry traversal54e200

rf_collision_model_pose_trace composes verified collision vertex deformation
and posed triangle queries over borrowed batches/prepared matrices. Caller
supplies scratch for the largest batch; no global vertex cache/allocation.
Entry hit time forms one float-stored endpoint used for every batch and
triangle. Deformation stops at first zero weight and uses byte/256. Batch
bounds start at FLT_MAX minimum and positive FLT_MIN maximum. Original
539530 uses if-minimum/else-if-maximum per axis, not independent updates.
A vertex lowering minimum skips maximum; in descending data maximum can
remain near zero and reject otherwise plausible contacts. This distinction
was exposed by original comparison case2667 and preserved in the port.
Expanded bounds gate ordered triangle calls; token is0, first-hit flag exits,
and the input query remains unchanged. Valid indices/prepared matrices and
adequate disjoint scratch are preconditions; loader/preparation are external.

verify_model_pose_trace.py passes4096 original/PC/NXDK cases,671 hits,
comparing all posed vertex bytes and complete hit output. Original54e200
runs its actual geometry/vector callees; only51ba00 is replaced with a checked
ABI prepared-matrix fixture. Cases use one batch and0..2 triangles, mixed
weights/zero termination, identity and arbitrary dyadic matrices, radius,
time limits and first-hit mode. Report: artifacts/model-pose-trace.json.
PC/NXDK builds and all19 CTests pass. Multi-batch traversal, full51ba00/LOD
composition,54e140 preparation and retained NPC/native XEMU remain open.


### Prepared skeletal traversal across batches (2026-09-12)

`tools/verify_model_pose_trace_multi.py` compares original54e200 with the PC
probe and compiled NXDK routine across8192 cases (685 hits), using zero to
two distinct six-vertex batches with independently varied triangle counts.
Only51ba00 is supplied with checked ABI and prepared matrices; original
geometry/math callees execute. An observation-only539530 hook filters its
return address to count traversal vertex visits without counting triangle
bounds calls. Coverage includes2732 empty traversals,152 first-batch early
exits and2578 complete two-batch traversals. All return values,32 hit bytes
and72 scratch bytes match exactly; poisoned scratch survives empty traversal,
and query/geometry inputs remain unchanged. PC build and19 CTests pass.
This is compiled-code verification through Unicorn, not a new native XEMU
run. Full pose preparation, LOD ownership and live NPC geometry remain open.


### Skeletal query coordinate preparation54e140 (2026-09-12)

`rf_collision_model_pose_query` composes54e140 coordinate preparation with
the prepared54e200 traversal. Any nonzero reset low byte clears time to1
and token to0. Flag2 copies input start/displacement into working vectors;
otherwise start-origin and displacement are inverse-rotated. The80-byte
input, including flags, remains unchanged. Returned geometry remains local.
Selected batches, prepared matrices and caller-owned scratch are required;
full51ba00 pose evaluation and retained-owner LOD selection are not added.

`tools/verify_model_pose_query.py` executes original54e140 and all geometry
callees, supplying only51ba00 with checked ABI and prepared matrices.
Disassembly confirms model in ECX, four stack arguments (pose/query/hit/reset)
and ret16. Across8192 cases (729 hits), PC and compiled NXDK match result,
32-byte hit,72-byte posed scratch and104-byte query exactly. Cases cover
translated/rotated input, both flag2 paths, reset values0/1/2/255/256/257,
zero to two batches,149 first-batch exits and2581 complete two-batch visits.
Both builds and19 CTests pass. This gate uses Unicorn for original/NXDK
code, not a new native XEMU gameplay run.


### Skeletal matrix refresh and query composition (2026-09-12)

`rf_collision_model_skinning_query` combines54e140 coordinate preparation,
51ba00 prepared-matrix refresh after pose evaluation, and54e200 traversal.
The borrowed skin-pose view supplies bind/evaluated/prepared matrices,16-bit
generation stamps and capacity. No allocations or scene ownership are hidden.
Coordinates/reset precede matrix preparation; accepted output is published
only on success. Preparation errors may retain reset/query and earlier cache
writes. The original pose-query API shares the coordinate helper.

`tools/verify_model_skinning_query.py` runs original54e140 with actual51ba00
and51b500; evaluated pose generations are current, while prepared generations
are independently current/stale. No original callee is substituted.8192 cases
(518 hits) match PC and compiled NXDK return/hit/query/scratch plus all192
prepared-matrix bytes and8 generation bytes. Includes125 first-batch exits,
2605 complete two-batch traversals and2732 empty traversals (matrix refresh
still occurs). Both builds and19 CTests pass. Unicorn comparison is not a
native XEMU run. Live animation evaluation, budgeted per-NPC prepared cache
ownership, selected skeletal LOD geometry and live scheduling remain open.


### Owned authored skeletal collision LODs (2026-09-12)

`rf_model_skin_geometry_open/close` owns one skeletal (flag2) LOD blob and
16-byte x86 batch views for54e200: positions, original8-byte links, signed
triangle records and counts. The16-byte owner reports total accounted bytes
and maximum batch vertex count for caller-owned posed scratch. Finite positions,
region sizes, triangle indices and active bone capacity are validated. Bone
validation stops at the first zero weight; bytes remain unmodified. Budget
includes owner/blob/views, excludes allocator overhead and pose scratch/cache.
No source archive is retained; errors preserve the empty output and release
partial allocations. Static LODs return NOT_FOUND.

`tools/verify_model_skin_geometry.py` scans all522 installed model files/930
LODs:170 skeletal LODs with599 batches match independent serialized position,
link and triangle hashes exactly, including maximum batch sizes. Peak owned
LOD is73752 bytes. Checks cover exact budget and one byte under, minimum
active-bone capacity and one under, malformed attachment failure after blob
allocation, repeated cleanup, and static rejection. All skeletal batches use
format518c41. PC/NXDK builds and19 CTests pass; no new native XEMU query or
retained NPC residency is claimed. Per-model selected residency, prepared
cache ownership and authored skeletal collision query comparisons remain open.


### Authored skeletal queries and native stock64MiB gate (2026-09-12)

`tools/verify_model_skin_authored_trace.py` rebuilds original runtime batch
pointers from all95 shipped V3C files/170 LODs and executes54e140/54e200
with actual geometry/math callees. Only51ba00 is supplied via checked ABI
with synthetic prepared identity or per-bone Z translation matrices.24 axis
sweeps per LOD cover thin/sphere and closest/first-hit paths. PC archive-owned
geometry matches all4080 queries (2674 hits): exact return/query/hit plus
FNV hash over the entire poisoned/reused posed scratch buffer. No original
loader or authored animation evaluation is claimed. The generated ignored
plan stores148 input bytes and144 expected output bytes per query.

`tests/model_skin_fixture.h` streams the same independent answers on PC and
Xbox, owning one selected LOD at a time. `tools/xemu_model_skin.py` packages
the opt-in model-skin-test.bin, launches isolated XEMU, reads guest state
through QMP, then restores/rebuilds the diagnostic disc. No host input or
desktop capture. Native run artifacts/xemu/model-skin-20260912-072724/report.json
passes with base memory67108864 and plugged memory0. Guest/PC state is
[1380340564,2,95,4080,2674,2588134272,92700,0]. Peak92700 accounts for the
loaded geometry owner/blob/views, posed scratch and256 test matrices, not
total emulator usage or allocator/other stack overhead. XBE SHA256:
402d92f77ecfe4f04c39af3a1c9dff4524af0697a8affd89b61ff28e2e55be78.
Both builds and19 CTests pass. Selected live NPC residency, budgeted prepared
caches, authored moving poses and scheduled collision remain open.


### Per-instance collision cache ownership (2026-09-12)

`rf_entity_collision_cache_open/close/view` provides a separate prepared-matrix
cache for an evaluated entity pose. Budget is the20-byte x86 owner plus50
bytes per bone (48 matrix,2 generation), in one backing allocation;1..50 bones
are supported. Opening initializes stamps unequal to the current16-bit pose
generation, including0/65535. Closing is repeatable and clears all fields.
No playback reference is acquired or released.

The view validates skeleton/count identity, storage layout/accounting and every
evaluated-pose generation before publishing the borrowed inputs required by
`rf_collision_model_skinning_query`. A moved pose for the same model instance
may reuse the cache; replacement models require close/reopen. Stale evaluation
or mismatched identity preserves the caller view. The cache does not schedule
animation evaluation or own selected collision geometry.

The existing owned_model_pose CTest now covers150 cache fixtures over all
1..50 bone counts and generations0/65535/1: exact/one-under budgets, initial
invalidation, stale-pose rejection, moved-view binding, actual matrix-refresh
composition, same-generation cache reuse, next-generation refresh/rollover,
skeleton mismatch and repeated cleanup. Both builds and19 CTests pass. This
adds the owner and adapter, not yet registration in campaign scene lifetime
or a native XEMU cache fixture; those integrations remain open.


### Registered model collision-cache lifetime and native replay (2026-09-12)

Scene registration now owns a separate slot-indexed collision-cache table,
preflighted under256KiB including table and all50-byte-per-bone backing stores.
Each registered model receives its own cache; actor-to-corpse pose publication
keeps the same slot/cache, and model retirement closes it. The original80-byte
registration owner layout is unchanged. Cache refresh follows successful gated
NPC pose advancement and corpse evaluation, using the resident bind transforms.
No animation generation is advanced merely to fill the cache. Skipped actors
can retain invalid/unrefreshed caches until future query evaluation.

NPC_COLLISION_CACHE telemetry reports created/peak bytes/refresh calls/content
hash/retired/errors. The180-frame campaign actor-pair replay passes PC and
stock64MiB XEMU in artifacts/xemu/replay-20260912-074311/report.json. Both
report[78,86010,6501,4131715306,78,0]; every cache retires. Native base memory
67108864, plugged memory0. XBE SHA256:
e539679a907b5afbb465b4c7888613d2918a786760b69337b291af971ce518e2.
The existing residency fixtures now supply valid bone/cache inputs; corpse
evaluation explicitly checks the moved model collision matrix and generation.
PC/NXDK builds and19 CTests pass. Selected skeletal geometry residency and
scheduled live collision queries remain open; this is startup animation/cache
agreement across platforms, not a full original live-animation audit.

During this gate XEMU repeatedly asserted graphics surface/DMA bounds before
frame submission. The committed scene baseline reproduced it in073946, ruling
out the current scene-cache integration as its cause. Optional static/skeletal
fixture bodies were inlined into main, whose compiled stack frame was0x5918.
Marking these fixture calls noinline reduced that frame to0x10f4; the rebuilt
180-frame replay then passed. This establishes the observed resolution, not
an instruction-level proof of stack corruption. Also fixed missing Xbox entry
prerequisites for test fixture headers: Windows clang dependency paths differ
from MSYS make paths, so explicit *_fixture.h prerequisites now rebuild main.
The074148 failed retry still used the old binary due to that dependency gap;
074311 used the verified smaller entry frame. All diagnostic flags and disc
changes were restored by the harness.


### Selected skeletal scene geometry and registered queries (2026-09-12)

rf_entity_collision_models_open/close owns the original initial skeletal
collision selection: last LOD of the first SUBM per shared model, matching
54e200 metadata access without static fallback. Skeleton/model indices stay
stable. The20-byte x86 owner accounts arrays, raw LOD blobs and batch views;
maximum vertex count sizes one scene-owned scratch buffer. All95 shipped
V3Cs pass tools/verify_entity_collision_models.py: independent first-SUBM
selection, two disjoint owners, exact/one-under total budgets, failure after
the first owner loads and repeated cleanup. Peak two-model fixture147532 bytes.

The campaign loads these resources with shared scratch under a combined1MiB
cap, then frees them on level teardown after model retirement. The public
rf_scene_model_collision_query resolves the registered model's published pose,
checks its evaluated generation through the cache adapter and composes matrix
refresh and skeletal geometry queries. Caller supplies query coordinates and
transform; scratch is synchronous/non-reentrant. It does not advance stale
animation, perform dynamic LOD selection, schedule physics or publish contacts.

The opt-in actor-pair replay now exercises six model-local thin/sphere sweeps
per retained actor every30 ticks, after gated authored pose advancement. Native
stock64MiB run artifacts/xemu/replay-20260912-075119/report.json passes180
frames with PC/Xbox NPC_MODEL_QUERIES=[5,61128,2472,2808,2448,2112961295,0].
Five shared meshes use61128 bytes and scratch2472 bytes. Every query return
is counted and all hit bytes feed the matching result hash. Cache state stays
[78,86010,6501,4131715306,78,0]. Base memory67108864, plugged memory0. XBE
SHA256: e5f1bf4537c534f2ccd7780c55d8ac41ef1a9d898751bb70faae9a339c1fc2cb.
Both builds and19 CTests pass. This proves cross-platform queries against
the currently authored animated scene poses; the independent original geometry
verification remains the earlier synthetic/prepared and authored-mesh gates,
not a new full original animation-runtime comparison. Model dispatch, demand
evaluation, dynamic selection and physical response scheduling remain open.


### Actor/model contact publication through scene geometry (2026-09-12)

rf_scene_actor_model_response now gathers retained player/NPC source state
and retained NPC target state, supplies current armor/class/814 target fields,
and composes original-verified49afe0 with5031f0/503120 dispatch and the registered
skeletal scene query. One published pose adapts the original type2 pose-record
selection without fabricated record contents. The geometry callback copies
80 query input bytes into104-byte working storage and passes reset0, matching
49afe0's original5031f0 call: hit time carries between actor spheres.

Contact mutations stay in the gathered copies until all geometry queries
succeed, then both body/contact owners receive publication. Query errors
preserve changed and do not publish partial response contacts; prepared caches
may already have refreshed. Target use-kind comes from authored class physics.
Scope is retained player/NPC source and retained NPC target; projectile/other
families, stale-pose demand evaluation and automatic pair scheduling remain.
Query telemetry now hashes acceptance and accepted hit bytes only, since an
original-style first miss can retain uninitialized non-time hit fields.

The opt-in actor-pair harness adds24 model-response cases: player/NPC sources,
three axes, both directions, two source spheres and deliberately offset misses.
It verifies accepted time/target handle and reciprocal target publication,
hashes both68-byte contacts and restores all three staged bodies/extras.
Native stock64MiB replay artifacts/xemu/replay-20260912-075820/report.json
passes180 frames with PC/Xbox ACTOR_MODEL_TEST=[24,12,12,569176833,0,3].
NPC_MODEL_QUERIES=[5,61128,2472,2856,2472,425497051,0]; this includes the
continuing animated-pose fixture plus48 response geometry queries. Base memory
67108864, plugged memory0. XBE SHA256:
ec107934606d9f0074aa7bb3a169097959c395f15089f9e724418f3631c4f4fd.
Both builds and19 CTests pass. This validates scene composition/publication
across platforms; original49afe0 numeric/branch verification is the earlier
independent helper gate. No automatic NPC blocking or projectile gameplay is
claimed until these responses are selected by the live pair scheduler.


### Registered actor pair dispatch (2026-09-12)

Scene rf_scene_actor_pairs_process now routes caller-owned actor pair records
through reconstructed48ca60 into registered player/NPC normal49ab00,
general49a420 and model49afe0 contact publication. Original active-body gates,
movement-mode selection and flag2/4 endpoint reversal remain in the core.
The adapter validates registered identities and a bounded list before dispatch;
no pair allocation or discovery occurs. Earlier successful contact publication
remains if a later response fails; the output hit count is preserved on error.

PC Release build and19 CTests pass. Stock64MiB XEMU replay
artifacts/xemu/replay-20260912-080456/report.json passes180 frames against PC:
pair_dispatch=[27,1,2,24,0] (calls,normal,general,model,errors);
actor_model_test=[24,12,12,569176833,0,3]. The model cases exercise player/NPC
callers, both pair orders, three axes, hits and misses; normal/general cases
compare published response fields with the previously verified core routines.
XBE SHA256:01bee4a97b489208341fd5584993313217470325bb552ddb44aa129a2b959069.

These are opt-in staged fixtures with saved scene state restored afterward,
not automatic gameplay collision scheduling. Global pair discovery/pool
ownership, trigger/projectile/solid bindings, stale-pose demand evaluation,
and velocity/position response scheduling remain open.


### Projectile discovery plane production (2026-09-12)

rf_collision_projectile_planes reconstructs full48bbe0 using a52-byte
resolved source: projectile e4 position, fc/108/114 basis rows, and
definition c0 speed. It computes float(10/speed), adds the scaled forward
row to each lateral row, normalizes using original X/Y/Z squared-sum order,
and writes a plane through e4. Each opposite plane reflects that normal
using float-stored dot/projection/scaled components, without renormalizing.
Plane D uses the original Z/Y/X dot order and sign. Four planes are written
to caller-owned storage instead of original global75db38. Inputs must be
finite with nonzero speed and constructed normals; no replacement fallback
normal is introduced.

verify_projectile_planes.py passes8192 exact original/PC/NXDK cases, using
full original48bbe0 and all real vector/normalization/plane helpers with no
substituted callees. Rotated orthonormal and arbitrary finite bases and
positive/negative nonzero speeds are covered; source bytes remain unchanged.
The original executable SHA is checked and x87 control is027f. Result:
artifacts/projectile-planes.json. Both builds and19 CTests pass. This gate
executes Xbox-compiled code in Unicorn; no native XEMU activation is claimed.
Live projectile definition/owner state, classification and discovery
scheduling remain open.


### Full object-family pair classification (2026-09-12)

rf_collision_pair_reject reconstructs48be00 common gates and all family
branches around explicit96-byte resolved endpoint views. Kind0/kind0 uses
the previously original-verified actor classifier. Other branches preserve
identity/null rejection, kind-dependent40000 gates, SP4000 suppression,
assignment versus OR of pair flags, owner exclusions and projectile/trigger
eligibility. Globals6fc4d8,64ecb9,6fc4d9,87210c and5afb78 are explicit inputs.
The resolved disabled4290d0, item-mode427020, projectile48c7f0, trigger48c8e0
and owner-player48a840 predicate fields use their low bytes. Owner facts
represent40a0e0(parent), including owner200 linkage and object7c flags.
The routine does not perform those lookups or predicate side effects.

Original asymmetries are intentional: projectile/solid may assign flags1
without the projectile definition bit while solid/projectile does not;
projectile/kind7 tests other78 while reverse tests other180; solid/kind4
accepts where reverse rejects. Rejected pairs may already have written flags.
No normalization of those routes into symmetric collision rules is applied.

verify_pair_classification.py passes32768 original/PC/NXDK cases over all
kind0..7 combinations, noncanonical predicate/global upper bytes, exact
thresholds and nearby floats, arbitrary initial flags and null/self inputs.
It observes1572 rejections with modified flags. Original48be00 and actual
actor/player/use/weapon/string helpers execute; the six external boundaries
listed above are supplied from explicit fixture facts. Exact reject byte,
flags and unchanged endpoint facts match. This verifies classifier output,
not equivalence of helper call timing or mutation during classification.
Report:artifacts/pair-classification.json. Both builds and19 CTests pass;
the8192-case actor classifier and8192-case projectile eligibility gates also
pass. Xbox-compiled comparisons use Unicorn, not native XEMU activation.

Next: bind registered object-family/owner facts and real eligibility
providers to discovery at creation/unhide, allocate the original pair pool,
and integrate substep processing/contact effects. Predicate facts must not
be replaced with unconditional acceptance when live bindings are added.


### Trigger creation eligibility bound into classification (2026-09-12)

rf_collision_trigger_pair_eligible reconstructs48c8e0 with actual helper
semantics:4c0910 checks trigger2b0 bit4; filter2c4 zero requires object7c
bit8, filter3 excludes it, and filter4 requires use-kind1 only for kind0
actors. Other filters pass this stage. This differs from later4c06d0
activation eligibility and48ca60 allowed-handle filtering; it does not
apply cooldown, activation count, geometry or allowed-handle checks.

The full pair classifier now computes this result from trigger flags/filter
and the opposite actor facts, replacing its supplied trigger predicate.
The96-byte endpoint wire layout stays the same, with trigger_filter replacing
trigger_eligible. verify_pair_classification.py now executes real original
48c8e0 and4c0910 rather than hooking them. All32768 original/PC/NXDK cases
pass, including716 observed trigger calls across filters0..5,256 and-1,
and1621 rejected pairs with changed flags. Both builds and19 CTests pass.
No native XEMU activation or full trigger scheduling is claimed.

Scene ownership inspection confirms the current NPC registry order is
explicitly provisional (serialized registration after class sampling), and
registered weapons are initially absent while full inventory ownership is
open. Do not claim this establishes original global discovery order.
Factory/list insertion order and live weapon facts must be connected before
using scene registration order as original creation/discovery scheduling.


### Original global object-list ownership (2026-09-12)

Original487100 allocates/constructs each object family, clears268/27c and
performs48a160(0), then4872eb increments global73a850 and updates peak73db0c
with a signed comparison.487305..487321 appends the object at the tail of
73d880: object14=old tail, object10=sentinel, old tail10=object, tail=object.
Thus48c9a0 next10 traversal follows successful allocation order, not reverse
order or handle-slot order.4867bc..4867e1 clears both removed links, repairs
neighbors and decrements the count before destructor dispatch and slot return.

rf_object_list retains those rules with caller-owned8-byte links and a16-byte
list/sentinel/count/peak owner on x86. Initialization is explicit; append needs
a detached node and removal a linked live node. No allocation, room mutation,
registry insertion or destructor is folded into the list operations.

verify_object_list.py compares actual original4872eb..487321 and
4867bc..4867e1 instruction regions without hooks against PC/NXDK across8192
operations:4088 appends,4072 removals and32 counter-boundary seeds. Every
link and counter matches after each operation, and independent forward
traversal matches allocation order through removal/reappend. Signed peak
comparison and32-bit count wrap are included. Both builds and19 CTests pass.
Report:artifacts/object-list.json. These are instruction-region comparisons,
not complete factory/destructor executions or native XEMU gameplay.

The insertion mechanism is now established. Level-loader factory call order
and scene ownership binding remain open; the current serialized NPC registry
setup is not by itself evidence of original cross-family allocation order.


### Authored level section dispatch order (2026-09-12)

verify_level_factory_order.py executes original460d3b..461137 post-world-
geometry dispatch with section headers checked against all94 installed RFL
archive entries. It compares exact handler sequences and key arguments in
470 runs: SP, MP client, MP server, multiplayer byte2, and synthetic version70
with the same section list. Section headers, resource/stream operations and
section handlers are supplied boundaries. This proves dispatch order, not
record parsing, constructors, first-phase geometry loading or player creation.
Report:artifacts/level-factory-order.json. No build or native XEMU claim.

For L1S1 the post-geometry sequence is200,300,500,600,6000,a00,1000,c00,
8000,2000,3000,70000,10000,20000,30000,40000,50000,60000,1000000,2000000,
3000000. Unsupported sections are skipped by their byte count. Actor section
30000 invokes464010, trigger60000 invokes465510; group10000 invokes468c50.
Only multiplayer byte exactly1 with server byte0 skips30000 and40000.
Section20000 invokes463d50 only at version71 or later. These handler calls
follow file order, not a globally sorted type list.

Additional Ghidra inspection (not new dynamic constructor verification):
468c50 at v180 reads group names/counts/UID references; its legacy key-object
creation branch is below v140. Thus an earlier group section does not itself
prove earlier controller allocation.464010 loops records sequentially, but
may create an additional masako_endgame entity for its special endgame path.
Do not equate one serialized record with one successful global allocation.
Actual per-handler factory calls and subsequent player creation remain the
next ownership-order evidence needed for collision discovery integration.


### Entity record creation ordering (2026-09-12)

verify_entity_loader_creation_order.py executes original464625..46474a,
with record-skip464e5a as the alternate exit. The resolved initial class ID
and parsed locals are supplied;42cdd0/name comparisons/class lookup and
422360 factories are explicit boundaries.5184 cases verify exact call order,
all seven factory arguments, unchanged transform bytes and complete actor/
class storage writes. Counts:3168 no-create,1890 one-create,126 two-create
cases;63 successful secondary publications. Report:
artifacts/entity-loader-creation-order.json. This is original-code evidence,
not a PC/NXDK equivalence or full-loader/constructor test.

Negative initial class IDs skip the record. Any nonzero multiplayer byte
consults42cdd0; a nonzero low-byte result skips. Main creation flags combine
hidden-byte nonzero->2 and the other parsed flag-byte nonzero->4. Factory
arguments are class ID, record name, UID-1, original position/orientation
pointers, combined flags and player-index-1. On main failure no secondary
checks execute. On success linked146c is initially-1.

When the supplied class-name comparison matches masako_fighter and level
comparison matches l20s2.rfl, the loader looks up masako. A nonnegative class
ID triggers an immediate second422360 call named masako_endgame, using the
same transform, flags2, UID-1 and player-index-1. If successful, it sets
secondary7c8=1, ORs secondary814 with10, writes secondary7cc=10.0f, publishes
secondary2c into main146c and ORs secondary class728 with100. These are
actual observed instruction writes; lookup/string semantics remain supplied.

Scene integration must account for this extra allocation and shared class
mutation before claiming serialized entity records establish global object
order. Subsequent per-record setup and post-load player creation remain open.


### Shared C entity record creation stage (2026-09-12)

rf_entity_loader_create now implements the verified464625..46474a stage
with a resolved44-byte input,28-byte constructor request and24-byte created
actor view on x86. The constructor callback receives class/name/UID/transform/
flags/player-index in original order and returns NULL on allocation failure.
Main success clears linked146c; the immediate endgame second request keeps
the same transform and publishes its link and actor/shared-class mutations
only on success. Secondary failure retains the created main actor. No heap
allocation is added by this orchestration; the callback owns construction.

The extended verify_entity_loader_creation_order.py now compares all5184
original cases against PC and Xbox-compiled C, including exact normalized
constructor requests and changed actor/class fields. The original predicate
lookup trace is verified separately; C consumes stable resolved predicates
and does not claim the same helper call timing or support callback mutation
of those facts. Both builds and19 CTests pass. Native XEMU scene activation,
live constructor ownership and subsequent entity-record setup remain open.


### Stationary NPC collision pose demand evaluation (2026-09-12)

Registered skeletal collision queries now check evaluated bone generations
before obtaining their skinning-cache view. Stale stationary NPC poses ensure
active motion residency and call rf_entity_pose_evaluate at existing playback
times, then continue through the existing skinning/query path. No selection,
timing advance or playback-reference mutation occurs. This follows the
original51ba00-before-prepared-matrices evaluation dependency on51b500;
authored pose reconstruction remains covered by its existing evaluator scope,
not a new full original animation-runtime comparison.

Current stationary NPCs have no retained pending root displacement, so this
path supplies zero displacement. Transferred corpses with stale poses still
require explicit rf_scene_corpse_evaluate with their caller-owned displacement.
Moving-NPC pending-root ownership remains open. Archive/sampling errors abort
the query and may leave partial pose matrices, as documented by the evaluator.

The opt-in model query fixture invalidates one NPC bone cache every30 ticks,
poisons its matrices, queries through the public scene path, and checks exact
restoration of all matrices/stamps with unchanged playback state. PC and native
stock64MiB XEMU replay-20260912-083938 pass180 frames with
npc_pose_demand=[2856,6,0,6,0] (queries,evaluations,errors,fixture cases/errors).
Model-query state remains[5,61128,2472,2856,2472,425497051,0], unchanged from
the prior baseline. Both builds and19 CTests pass. XBE SHA256:
90508e6cf1465ae108a4304be98ef33286832c16207ac7c6286beef3908905cb.
No new visible gameplay or full physics scheduling is claimed.


### Pending displacement producer audit (2026-09-12)

The earlier stationary-query change must not imply every moving actor needs
a synthesized displacement increment. tools/audit_pose_displacement_refs.py
reproduces eight direct operand references to12c0/12c4/12c8 in the checked
original executable. Ghidra containing-function inspection separates instance
51ae00/51ae90 constructor boundaries and explicit zero assignment51af8f,
instance root consumption51b8e9, and shared-definition attachment transforms
at51b45b/51d4e6/51d544/51d568. The latter addresses are not pending root
movement producers despite sharing the numeric offset. Default vector
constructor calls alone are not evidence of a clear;51ae90 also bulk-zeros
instance storage and explicitly assigns the zero pending vector.

Report:artifacts/pose-displacement-references.json. No nonzero instance
producer is established by this scan. Linear decoding with data skipping and
direct-offset matching is not exhaustive pointer-alias analysis, so this does
not prove the field always remains zero. Preserve explicit displacement
consumption where callers supply it; require real producer evidence before
adding movement-driven accumulation. Collision scheduling and NPC body
stepping can be pursued without inventing that missing write.


### Live player swept bounds and contact preparation (2026-09-12)

rf_physics_body_prepare_sweep reconstructs both49f8aa..49f925 and
49fd84..49fdf9: min/max of current and predicted positions, radius expansion,
then contact preparation (time1, handle-1, both force accumulators cleared,
flag01000000 set). Equal coordinates preserve original operand selection,
including signed zero. Nonfinite input/result returns RF_RANGE without body
mutation; finite signed radius is retained rather than silently clamped.

tools/verify_physics_prepare_sweep.py checks2048 cases against both original
instruction spans with actual539460/465ee0/465ec0/4fad00 helpers and no hooks.
All retained PC/NXDK body bytes match; original surrounding bytes, NXDK guard
bytes, repeated calls and invalid-input preservation are checked. Movement
prediction and complete physics scheduling are outside this isolated test.

The live player actor_tick now invokes this stage after movement prediction
and before each sweep, replacing the partial force/flag reset. Both builds
and19 CTests pass. Native stock64MiB replay-20260912-085053 passes180 frames;
the complete pc-reference.txt is byte-identical to replay-20260912-083938.
The existing replay verifies guest command submissions, world/camera hashes
and final body; the isolated verifier proves the new preparation fields.
Completion has8728 available pages and sampled GPU mesh peak1339632 bytes.
XBE SHA256:6a6c01207069ed4eb12aeb3d0682b102b2d5721d37c070b627f6a8311992c570.
NPC stepping, discovered pair scheduling and complete actor-family response
integration remain open. No new visible gameplay is claimed.


### Registered NPC world/mover body sweeps (2026-09-12)

The previous live actor_sweep converted spheres from the global player owner,
which could not be reused for NPC movement. campaign_physics_body_sweep now
takes explicit body state, owned sphere collection and caller conversion
scratch, and the player calls it. rf_scene_npc_body_sweep validates a registered
kind0 NPC handle, borrows its authored spheres and queries an explicit proposed
body against the current campaign world/mover geometry. No query allocation,
actor mutation, response publication or physics scheduling is performed.
Scratch capacity is explicit; hit is preserved on a miss and hit/matched are
preserved on errors. Zero translation returns a miss, preserving the existing
player exit. This is a scene binding to the previously reconstructed body
query, not new evidence for complete original NPC physics scheduling.

The opt-in actor-pairs fixture tests78 retained NPCs across three axes with
positive/negative16-unit proposals and zero translation:702 cases,397 hits,
305 misses, hash799736443, zero errors. Explicit independent query assembly
checks sphere-owner and transform selection; PC/native equality checks complete
hit records via the accumulated hash. Scratch is allocated once for the
fixture and freed. The registered binding test also covers stale handles,
NULL world, insufficient capacity and stationary output preservation.

Both builds and19 CTests pass. Stock64MiB XEMU replay-20260912-085925 passes
180 frames. Its PC reference differs from085053 only by the new NPC_BODY_SWEEP
row; existing movement/rendering telemetry is unchanged. XBE SHA256:
83f90d31d3c99534a324e806d847a3cf8242d80e7ad8c9858bb385db0d15ea35.
Live NPC prediction, contact response, support/landing and position/room
publication remain open; this fixture does not make stationary NPCs move.


### Scheduled registered NPC support refresh (2026-09-12)

rf_scene_npc_refresh_support binds41e370/40a420 to the registered NPC support
handle, movement descriptor, cached velocity and body/object flags. The source
resolver validates object-registry identity against retained NPC, player or
mover owners, then reads actual body velocity (original144), not cached actor
extra velocity8a0. Unsupported families and missing/stale support handles leave
cached velocity and wake flags unchanged. Eligible modes1/3 copy even zero
velocity and wake the body/object; the entity view mirrors object flags.

The campaign tick now refreshes all registered NPCs after controller propagation
and before the player physics phase, alongside the existing player refresh.
Original containing487a40 places41e370 after46bbe0 and before487770 (export
487c33.c.txt); this ordering supports the phase placement but is not a full
verified original frame trace. NPC support acceptance and physics stepping
remain separate unfinished stages. NPC setup still has no accepted support,
so ordinary opening ticks preserve the constructor cache.

verify_support_refresh.py again passes336 PC/NXDK updates against unchanged
original41e370 with real lookup and wake callees. Binding unit coverage adds
stale target, invalid descriptor and self-support from actual body velocity.
The opt-in fixture covers all16 descriptors against five support states:
missing, stale, self NPC, player and mover. Distinct nonzero source velocities
and seeded cached velocity expose wrong-field or wrong-owner reads; actor and
source state are restored. Both builds and19 CTests pass.

Stock64MiB XEMU replay-20260912-090528 passes180 frames with
npc_support_refresh=[179,14042,6,80,1344857061,0]: ticks,actor calls,resolved
reads,fixture cases/hash,errors. Six resolved reads are staged fixture cases;
179*78 ordinary calls have no accepted support yet. The PC reference changes
only by the new NPC_SUPPORT_REFRESH row compared with baseline085925.
XBE SHA256:7eae938c8f4eecc1a716c4710edf8000b0d44b40eece20a8c308ca8341bb5a39.
No platform riding, NPC motion or full contact-response completion is claimed.


### Positive-inverse-mass contact velocity response (2026-09-12)

rf_physics_dynamic_contact reconstructs49dcf6..49ddef, the positive-inverse-
mass branch reached from49d7e0. Resolved missing contact object or NPC against
a player returns zero impact without velocity/normal mutation. Ground mode1
flattens nonzero normal Y strictly between-.95 and+.95 and normalizes, with
original4fab70 zero-length fallback(1,0,0). Other normals remain supplied.
The branch clamps actor normal velocity with min(0,dot), clamps contact-minus-
support normal velocity with max(0,dot), then adds normal*float(impact*1.05).
Intermediate stores, Z/Y/X dot ordering and low-byte predicates are preserved.
This is distinct from the existing zero-inverse-mass static/flag80 helpers.

tools/verify_dynamic_contact.py executes original49d7e0 through49ddef with
actual40a0e0 generation-checked lookup,4895d0 player predicates, normalization
and vector helpers; no intercepted calls.4096 cases match all PC/NXDK body
bytes, normal and impact, with original unrelated actor bytes unchanged.
Coverage includes1633 suppressed/missing cases,112 changed normals, exact
thresholds, zero-horizontal fallback, signed zeros and low-byte predicate facts.
NXDK guard/input preservation and24 nonfinite rejection cases pass. Artifact:
artifacts/dynamic-contact-verification.json. Both builds and19 CTests pass.

The helper does not check inverse mass: the caller must select this branch
from retained contact metadata. It is not yet bound into live scene response.
Liquid/crush/rotating branches, complete contact dispatch and impact damage
remain open. No new native XEMU run or visible behavior is claimed for this
isolated reconstruction; compiled NXDK comparison uses Unicorn at x87 control
word027f, matching the established runtime precision contract.


### Contact entry selection and damping (2026-09-12)

rf_physics_contact_select reconstructs49d7e0 entry through six distinct
response/effect boundaries. Nonzero actor1ec (body.word_164) clears itself
and actor1ac bit1000, scales velocity by float.85 and supplies the new Y as
impact. This route is named DAMPED: the current contact owner calls1ec
reserved, so the historical liquid label does not establish its producer.
Positive contact inverse mass selects49dcf6. Otherwise the default response
comes from body flag80, with crush/stance decisions preceding it.

Crush eligibility requires mode1, a resolved object of radius>1, normalY<-.5,
negative contact velocityY and squared(contact velocity-support velocity)>.1.
Actual428010 checks actor974 or964 not equal-1;40a130 checks actor810 bit400.
An available field and clear bit selects49d907 stance effects; other eligible
cases select49da18 crush damage. No semantic identity for964/974 beyond these
verified field tests is required by the helper. The caller must execute those
effects and preserve their ordering before continuing the default response.

tools/verify_contact_select.py executes original49d7e0 with real generation-
checked object lookup, vector helpers,428010 and40a130. Hooks only stop at
selected instruction boundaries, never replace helper results.8192 cases
match complete PC/NXDK body state, route and impact. Route counts are
[1024,1785,2654,2526,127,76] for damped,dynamic,static,flag80,crush,stance.
Unrelated original actor bytes, NXDK source/guards and10 nonfinite rejection
cases preserve expected state. Both builds and19 CTests pass. Report:
artifacts/contact-select-verification.json.

This is not complete49d7e0: subsequent stance/crush effects, rotating response
and damage-tail dispatch remain open. Existing dynamic/static/flag80 numeric
helpers must be composed at their correct boundaries. No live scene binding
or new XEMU execution is claimed for this isolated compiled-NXDK comparison.


### Rotating-body contact response (2026-09-12)

rf_physics_rotating_contact implements49db7d..49dcf1 when429990 is true
(resolved486c90 use-kind1), flag80 is clear and contact inverse mass is not
positive. Contact offset is point minus body position. The stored angular
vector150 is copied, local X negated, and transformed with the current body
orientation using original Z/Y/X addition order. Angular cross offset adds
to linear velocity, then support velocity, preserving intermediate stores.

The signed impact is contact normal speed minus combined actor normal speed.
A1.1-scaled normal correction is applied only when its normal dot is positive.
On that path, signed sphere count>1 halves the stored angular vector; counts
<=1 preserve it. This differs from the nonrotating path, which clears angular
velocity after correction. Normal is retained without normalization.

tools/verify_rotating_contact.py runs original49d7e0 through49ddef with real
429990/486c90, cross product, transform and arithmetic helpers, no hooks.
4096 cases match complete PC/NXDK body bytes and impact:1926 velocity changes
and965 angular-damping cases. Counts-1/0/1/2/3/32, identity/arbitrary finite
orientations and zero/nonunit normals are covered. Original unrelated bytes,
NXDK source/guards and30 nonfinite rejection cases are checked. Both builds
and19 CTests pass. Report:artifacts/rotating-contact-verification.json.

This supplies the remaining numeric nonpositive-inverse-mass response branch,
not complete contact scheduling. Compose the verified entry router, numeric
responses and crush/stance effects with the damage tail before claiming full
49d7e0. No live scene binding or new XEMU execution is claimed for this
isolated compiled-NXDK comparison.


### Composed single-player contact dispatch (2026-09-12)

rf_physics_contact_process_sp composes complete49d7e0 SP orchestration from
the verified entry/damping and dynamic/static/flag80/rotating numeric paths.
A borrowed mutable actor view retains body/contact pointers, support/command
vectors and movement/class/stance facts. Object lookup occurs first, including
damping. Stance callbacks execute4289d0,427450(mode0),42a580(state9,.25),
then4a3740 and optional player byteb1=1. Response selection afterward rereads
body flag80 and updated actor facts; it does not repeat inverse-mass routing.

Crush dispatches the original9999 damage request (source=current contact
handle, kind-1, argument6=0, auxiliary UID-1, force0), then clears velocity
and returns without ordinary impact. Every other path invokes49cd80 impact
after response, including zero impact. Callbacks must preserve owner lifetime
and synchronize mutated actor facts. Errors stop immediately without rolling
back earlier effects. Multiplayer tail filtering remains outside this SP API.

tools/verify_contact_process_sp.py runs complete original49d7e0 to return,
with real lookup/predicates/numerical helpers. Only the six gameplay/resource
boundaries above and impact/crush effects are supplied by the harness; original
lookup remains actual code and is observed for order.2048 cases match exact
PC/NXDK body/contact/actor facts, player flag and ordered callback arguments.
Counts:[2048 lookup,145 crouch,145 speed,145 motion,145 player lookup,
422 crush,1626 impact]. Controlled callback mutations change body flag80,
movement mode, use-kind, support, inverse mass and velocities; the dispatcher
uses the same post-callback state as original. Damage callbacks also mutate
velocity to prove crush clears it afterward. All seven callback failure
boundaries preserve their already-applied state and stop subsequent work.

Both builds and19 CTests pass. Artifact:artifacts/contact-process-sp-
verification.json (one filename, without the line break). tests/contact_process_probe.h
provides the shared PC wire/callback fixture. This completes orchestration
at explicit backend boundaries, not callback internals or live NPC stepping.
Bind retained scene stance, player flag and damage backends and validate the
composition in XEMU next. No new native run or visual change is claimed here.


### Registered NPC speed and animation requests (2026-09-12)

rf_scene_npc_set_speed and rf_scene_npc_request_motion bind427450/42a580 to
registered active NPC owners. Speed reads retained class settings and body
mass, updates movement settings and mirrors response into body coefficient8c.
Caller supplies resolved actor75c forced_action (-1 absent), because its live
attachment lifecycle is not yet retained here. SP mode is explicit. Animation
requests use the current registered pose controller and authored class motion
mapping, preserving original missing-state fallback and transition retargeting.
Neither binding performs AI selection, pose evaluation or physics stepping.
Stale/non-NPC/transferred-corpse owners fail without actor mutation.

The actor-pairs fixture runs all78 opening NPCs through eight speed cases
(requests0/1/2/-7 with absent/present forced action) and four motion requests
(0/9/23/-1 at.25 seconds). It compares retained output to the independently
assembled class/helper inputs and restores settings, body coefficient and
controller. Stale handles and invalid-duration preservation are also checked.
The first PC harness attempt093513 caught a wrong expected error code in the
new test (negative duration returns RF_FORMAT); correcting that assertion
allowed the full rerun. No game behavior was changed to satisfy the assertion.

Both builds and19 CTests pass. Native stock64MiB XEMU replay-20260912-093543
passes180 frames with npc_motion_request=[78,624,312,1993876021,0]. The PC
reference differs from090528 only by the new NPC_MOTION_REQUEST row. XBE:
930b506172543b9537babfdb5871864cd235c3000419781053778865dc6cca8d.

Crouch4289d0 must still copy retained stance centers, execute reentrant4a0840
ground/landing, then publish clock6460f0 into actor7b4. Its ground call must
not be replaced with a no-op when binding contact effects. Actor964/974 are
already identified as authored logical crouch8/crouch-walk9 motion slots in
NPC initialization; contact adapters can source them from the retained mapping.
Full contact effect binding remains open; these requests alone do not make
NPCs move or change visible playback in the restored test scene.


### Composed post-query support finish (2026-09-12)

rf_physics_support_finish reconstructs4a0a5c through4a0c94 after the ground
query. It preserves lookup gating and rejected-contact fall routing, accepts
static/moving support through the verified numeric commit, copies the complete
contact payload and publishes position/handle/material. The falling predicate
is reevaluated after material1380 is assigned. If true, signed normal impact
is delivered before landing, even if the impact callback changes movement mode.

Query word_1f0 is original local18, the opaque source for4e5c60. Nonzero source
plus a high-bit query handle invokes relative conversion and stores its result
in actor1474 after landing. The query remains immutable across callbacks, so
changes to the body's copied contact word do not replace this source. Backend
callbacks retain actor lifetime and update mode/use-kind facts as needed.
Failures stop immediately; completed contact publication and earlier effects
are not rolled back. Ground preparation/execution is outside this helper.

tools/verify_support_finish.py runs original4a0a5c through return with actual
lookup, predicates,417d80 contact copy and vector/bounds helpers. Only fall,
impact,landing and relative callbacks are supplied.4096 cases exactly match
PC/NXDK body/contact/support/published state, mutable actor facts and callback
arguments/order. Counts:[975 lookup,789 fall,626 impact,626 landing,
432 relative conversion]. Initial/final material-dependent falling, rising
and static support, rejected type3/high-bit bodies and reentrant callback
changes are covered. All five callback failure boundaries preserve applied
state and stop subsequent work; query/probe/source bytes stay unchanged.

Both builds and19 CTests pass. Report:artifacts/support-finish-verification.json.
The PC callback fixture is tests/support_finish_probe.h. This completes
post-query orchestration at callback boundaries, not full4a0840 or live
NPC landing. Compose query execution and scene fall/impact/landing/relative
backends next; crouch must reenter that same path. No new native XEMU run
or visible gameplay change is claimed for this compiled-NXDK comparison.


## Retained NPC ground-query binding (2026-09-12)

rf_scene_npc_ground_query binds original4a0840 preparation to registered NPC
body spheres/next position, retained movement descriptor, class base speed,
class use-kind, support material and vertical support velocity. Elapsed time
is explicit. The single lowest sphere, identity matrix, radius and flags feed
the existing shared world/mover query. A successful hit copies the complete
68-byte contact wire. A miss sets time1 and reserved_1ec0 while preserving
other caller bytes. Invalid/stale owners and query errors preserve outputs.
Player-flag owners require their separate49b900 prequery path. Face tokens
remain port tokens; original pointer lifetime/relative conversion is not claimed.

The first PC fixture failed at authored NPC index15 with no collision spheres.
The adapter correctly rejected an undefined lowest-sphere query; the fixture
incorrectly required all78 owners to have spheres. Seven authored owners have
none. The revised fixture verifies RF_RANGE and unchanged probe/contact/matched
for those owners, without synthesizing spheres. Three elapsed/movement/support
cases for each remaining71 owner yield213 queries:104 hits and109 misses.
There are21 empty-body rejections and stale-handle preservation checks on all
213 successful queries. Each query is compared to separately assembled world/
mover execution. These scene checks extend prior original-executable numeric
preparation verification; they are not an independent full4a0840 comparison.

PC and NXDK builds plus19 CTests pass. Stock64MiB XEMU180-frame replay passes:
artifacts/xemu/replay-20260912-095509/report.json.
npc_ground_query=[213,104,109,3482877009,0,21], exactly equal to PC.
Tested XBE SHA256:5e621d4bf40ae78cb168fec496445bd843ca13e9c4f7b1887c0804ef26d9d35b.
The PC reference differs from replay-20260912-093543 only by the new query row.
Fixture movement/support changes are restored. No landing, live NPC physics
scheduling or visible gameplay change is claimed. Compose support_finish with
fall/impact/landing/relative backends next; crouch must reenter the same path.


## Retained NPC fall transition (2026-09-12)

rf_scene_npc_fall binds4281a0 to registered active NPC owners: body flag1,
class724 bit400 selection of slot8 versus3, enabled-low-byte fallback0, and
stable identity movement orientation. Added retained actor85c ownership;
constructor422360 also installs identity73a858 after descriptor selection.
No support velocity/handle, position, speed, animation or damage mutation is
part of this callback. Original movement orientation is separate from the
body's physical orientation. Stale/corpse owner errors preserve state.

The restored-state native fixture covers78 owners times4 cases: normal,
alternate, and both disabled-low-byte fallbacks (enabled words101/100).
Whole-owner comparison verifies only descriptor, body wake bit and borrowed
orientation change; stale handles preserve that result. Initial nonidentity
orientation verifies actual replacement. Shared class/descriptor changes and
owners are restored after each actor, including failures. The scene binding
uses previously reconstructed rf_movement_fall. Re-run original force/fall
verification passes512 original/PC/NXDK cases with actual4281a0,40a270 and
4339d0, supplying only the audio boundary. This is separate from the native
scene fixture, not a claim of full NPC scheduling against the original.

Both builds and19 CTests pass. Stock64MiB XEMU replay passes180 frames:
artifacts/xemu/replay-20260912-100007/report.json.
npc_fall=[312,78,78,156,4141236165,0], equal to PC.
Tested XBE SHA256:42bdb579683946485d07dd207953b5a98d250c873767614cf8a4bcc818a1f745.
PC reference adds NPC_FALL and increases NPC_BODIES owner/total byte counts
by312 bytes (one4-byte orientation pointer for each of78 owners); the existing
body-state hash and all other rows are unchanged.
The binding remains callable rather than automatically scheduled; compose it
with support finish and complete impact/landing/relative callbacks. Landing
must retain support-relative velocity, sound, stance clearance and special
class effects. No new visible gameplay change is claimed.


## SP impact orchestration (2026-09-12)

rf_entity_impact_process_sp composes49cd80 numeric impact damage with its
ordered effect boundaries. It resolves falling from movement/use-kind and
cached support1380, but the material3 damage reduction uses contact1d0.
Eligible damage queries force suppression45ce50 at published object3c;
only the low byte suppresses. It then dispatches the SP4892c0 request with
source-1, kind9, argument6=0, auxiliary UID-1 and force0. The backend supplies
the target handle from the mutable actor and original unused target-1 slot.
Health is reread after damage: negative or unordered health selects the
lethal sound block, while nonnegative health selects player lookup/feedback.
Original fcomp/test AH bit0 follows the lethal branch for NaN; this behavior
is preserved. Sound uses the current class128 set and current bodye4 position,
which can differ from pre-damage values. Failures stop without undoing earlier
effects. Callback owners/facts must remain alive and current through reentry.

The callback contract explicitly leaves these bodies external:
-45ce50 force-region query.
-4892c0 actual damage and its downstream effects.
-49ce88..49cecf:434d00 sound selection and48a930 playback.
-49ced0..49cf31:48aa90 player lookup, camera/feedback calls and local clamp.
No multiplayer forwarding is included in this SP operation. It does not
claim those callback bodies or scene dispatch are complete.

tools/verify_impact_process_sp.py executes original49cd80 through return,
with actual falling/kind predicates and arithmetic, substituting exactly
those four boundaries.4096 cases match complete retained actor state and
ordered callback arguments on PC and compiled NXDK under x87 control027f.
Counts:[1409 suppression,840 damage,321 lethal sound,519 player feedback].
Coverage includes contact/support material distinction, low-byte suppression,
post-damage zero/negative/positive/unordered health, and callback mutations to
handle, sound set and body position. All four callback failure paths stop with
already-applied state intact; four invalid/overflow speed inputs leave actor
state unchanged and call no backend. The PC fixture is impact_process_probe.h.
Report:artifacts/impact-process-sp-verification.json. Both builds and19 CTests
pass. No new native XEMU run or visible gameplay change is claimed. Bind the
verified orchestration to force, damage, sound and player owners before NPC
support-finish integration; landing and relative conversion remain open.


## Separate impact-death sound metadata (2026-09-12)

The49cd80 lethal sound reads class128, not class124's ordinary $DeathSnd:.
Original41c961 pushes595560 ($Impact Death Sound:); optional parsing resolves
through434cb0 and41c993 stores class128. An absent field branches41c96f to
41c999 without writing it. In contrast,41cb4b stores $DeathSnd: at124 and its
missing-field branch explicitly writes-1. Reusing the existing death group
would therefore select the wrong sound. Miner1 impact group21 (Impact Death)
is distinct from ordinary death group20 (Player Die).

rf_entity_impact_sound_group_read preserves the supplied initial value when
absent, resolves explicit empty/unknown labels to-1, and rejects malformed or
duplicate declarations without changing output. It is a selected-field port
parser, not a reconstruction of the complete original parser. The scene owns
one separate4-byte group per retained class and releases it with NPC metadata.
First-load storage is zero initialized, matching the original static class
array image at5cc500+128; parser41b910 has no missing-field assignment here.
Class reload/inheritance lifecycle remains outside this first-load binding.

verify_pain_groups.py --impact checks all63 installed classes and6 synthetic
cases against independent label extraction and real original434cb0 resolution,
on PC and compiled NXDK.13 classes explicitly resolve21;50 preserve input.
The missing-field tests seed123 to distinguish preservation from zero/-1.
The existing --death check also passes63 classes and7 synthetic cases. No
sample selection, PCM playback or live impact dispatch is claimed yet.

Both builds and19 CTests pass. Native stock64MiB replay101326 completed180
frames but failed an outdated expected allocation formula (48 bytes/class).
Adding one group makes52 bytes/class; the harness was updated accordingly.
Replay artifacts/xemu/replay-20260912-101509/report.json then passes180 frames.
NPC_IMPACT_GROUPS=[5,20,1853699328], exactly equal to PC. Audio owned/peak bytes
both increase20; all other previous PC rows are unchanged. Tested XBE SHA256:
634f169c32828975e9a73689e02a544fc1747b415fd3ecb777220d8a063ad626.
Bind impact audio selection/playback using these retained groups and bodye4
position, along with force suppression/damage services, before support finish.


## Registered NPC impact audio playback (2026-09-12)

rf_scene_npc_impact_sound binds the49ce88..49cecf NPC sound path to retained
class128 groups, shared original-verified group choice, on-demand PCM residency
and unity spatial playback. Position comes from bodye4 rather than published
object3c or eye7d4. It shares the existing audio bank/mixer and eviction policy;
no duplicate cache or per-NPC voice ownership is introduced. The caller must
establish lethal impact eligibility. Current registered NPCs use spatial audio;
associated-player routing remains the separate player service. Selection/RNG
or PCM effects are not undone on a later playback error. Missing samples do
not create a voice. Stale registration rejects before changing RNG or audio.

The explicit --damage-uid fixture requests impact audio after its damage
checks, using a separate seeded random state and temporarily offsetting bodyY
by2 while keeping published position unchanged. It restores the position and
verifies requested-position telemetry and stale-handle preservation. This is
an audio fixture, not a claim of a lethal gameplay collision or automatic
impact dispatch. The existing pain/death routines are unchanged.

Both builds and19 CTests pass. Stock64MiB XEMU180-frame --damage-uid8456 replay:
artifacts/xemu/replay-20260912-102104/report.json PASS.
NPC_IMPACT_AUDIO=[1,1,1,1,2380,89,2745024,0,1022235703,
3232371648,1081468562,1103648368], exactly equal to PC: one selection/play,
one2380-byte PCM load, sample89 and zero errors. This verifies native runtime
requests/state and residency; listening quality is not inferred from counters.
Tested XBE SHA256:7f22e42a624fb72b5b766e008fe81471d7a43ce43a3c834a54cfd79ef07ecf6d.
Compose impact orchestration with real force suppression/damage and associated
player feedback, then connect ground acceptance, landing and NPC scheduling.


## Registered NPC impact dispatch (2026-09-12)

rf_scene_npc_impact composes the verified49cd80 SP sequence with retained
NPC body/contact/class/support facts, current campaign force regions, the
registered NPC damage adapter and class128 impact sound playback. It reloads
retained actor facts after synchronous damage effects before the core chooses
its post-damage branch. Callers supply difficulty, clock, shared RNG and the
real damage-effect backend, preserving actor lifetime through reentry. No new
shadow health owner or alternate damage calculation is introduced.

48aa90's current single-player lookup checks the player's entity and its
linked actor200. The scene binding resolves that retained player view after
damage; an unassociated NPC returns without a player effect. A matched player
requires the supplied full49cedf..49cf31 feedback callback. Missing feedback
then reports NOT_FOUND instead of pretending the effect ran. Scheduling,
additional player owners and the complete feedback callback remain external.

The --damage-uid8456 fixture adds four cases: stale generation preserves actor/
RNG/counters, speed0 preserves the entire NPC owner, a temporary containing
sphere with activation1/flag40 suppresses speed11, and speed11 with the actual
campaign force collection dispatches kind9 damage. The first fixture attempt
used shape0 (unknown), which correctly did not suppress; corrected to original
sphere kind1. The temporary force collection is restored before subsequent
queries. Nonlethal damage runs the existing pain animation/sound/AI callbacks;
health falls88 to80.31999969482422 after class/vitals scaling. This diagnostic
keeps that damage effect; it does not claim automatic collision scheduling.

Both builds and19 CTests pass. Stock64MiB XEMU180 frames PASS:
artifacts/xemu/replay-20260912-102732/report.json.
NPC_IMPACT_DISPATCH=[3,1,1,0,1,0]; NPC_IMPACT_TEST=[4,1118830592,1117823959,0],
exactly equal to PC. One suppressed call, one damage dispatch, no lethal sound
branch and one no-player lookup. The separate impact audio fixture still
passes. Pain sound receives a third request; cooldown keeps selection/play
counts at1, matching original behavior. Tested XBE SHA256:
ec4238376b5e562e20f5320bf39a661fd0bea9333ae5cd21528c533983dd80bc.
Lethal impact integration with complete damage effects and associated-player
feedback still require coverage. Connect landing/relative contact and then
NPC support/physics scheduling; no new visible gameplay claim is made.


## Complete landing ordering at effect boundaries (2026-09-12)

rf_entity_land_process composes419830's sound gate, group selection,
support-relative velocity and post-velocity class/action dispatch. Sound is
requested when contactY minus velocityY exceeds.25 or total velocity length
exceeds.5. The latter uses original40a000's X/Y/Z squared sum and sqrt.
A negative material selects group0; a nonnegative material selects its group
only when that group is positive, otherwise group0. Actor810 flag1000 with
positive group4 overrides that selection. Group0 itself may be zero/negative;
it is passed through to the sound callback rather than skipped.

The sound callback owns4198b3..419901 (selection/playback at published3c and
conditional player landed flag). Its retained actor mutations are observed
by the subsequent stored (velocity+previous support)-contact velocity update.
Action4 then selects normal or slow and preserves body200000. Other actions
optionally dispatch419981..4199c5 special effects, reread crouch after that
callback, request normal/forced-crouch, and clear body200000 only after the
successful stance callback. Callbacks can fail; processing stops without
rollback or later flag clearing. Finite velocity inputs and bounded sound
material indices are the port contract. Existing post-velocity-only API is
retained for its prior callers/verifier.

verify_land_process.py executes original419830 through return, with real
speed/material gates,40a000/vector helpers and40a130 stance predicate. Only
the stated audio/player-flag block, special block and4280b0/428030 are supplied.
4096 cases exactly match full retained actor state and callback arguments/order
on PC and compiled NXDK with x87 control027f. Counts in NORMAL/SLOW/CROUCH/
SPECIAL/SOUND order:[1362,1905,829,782,3345]. Threshold equality/next-float,
fallback/override, and callback velocity/support/action/flag mutations are
covered. Five callback failure routes stop with applied state intact. Ten
port-domain guards (nine nonfinite vector components, one material overrun)
leave the actor unchanged and invoke no callback; these are not claims of
original invalid-input behavior. Both builds and19 CTests pass.
Report:artifacts/land-process-verification.json; PC fixture:land_process_probe.h.
No new native XEMU replay or visible gameplay change is claimed. Connect the
scene sound/player flag and stance/special backends, then compose support
finish, relative contact and NPC physics scheduling.


## Registered NPC standing and clearance (2026-09-12)

rf_scene_npc_try_stand binds original428a60 to registered active NPC owners,
authored stance caches, current spheres and published position. Its clearance
adapter uses the existing NPC/world/mover body sweep with the old body spheres,
original stand endpoint and state124|4, adding100 for radius below.05. It does
not replace spheres or move the body before determining clearance. A blocked
result leaves the NPC unchanged. Success clears actor400, optionally resolves
and clears the caller's player crouch byte, copies cached standing centers,
and calls a REQUIRED ground service. This avoids installing a no-op ground
refresh. View/damage flags are synchronized at mutation boundaries and after
ground; reentrant ground changes to flags/centers survive. Errors preserve
already-applied effects; stood is published only on success. No speed change
or scheduling is introduced.

The actor-pairs fixture exercises60 eligible NPCs with three paths each:
ordinary clearance, a ground callback that performs the real NPC ground query
then reenters crouch/changes a sphere center, and a staged16-unit clearance
endpoint that meets the existing overhead world geometry. It independently
assembles each clearance sweep. Results:180 attempts,120 clear/ground queries,
60 blocked. An initial fixture assumed all sphere owners had stance caches;
11 have no applicable centers. Those now verify RF_RANGE with untouched owner/
output/callback count. Seven sphere-free NPCs are excluded from this stance
fixture. All owner/sphere/class-height changes are restored. The ground callback
is diagnostic query/reentry coverage, not complete landing/support acceptance.

Existing verify_try_stand.py still matches576 full original428a60 cases on
PC/NXDK (clearance/player/ground boundaries supplied),10 port rejections and2
callback failures. Both builds and19 CTests pass. Stock64MiB XEMU180 frames:
artifacts/xemu/replay-20260912-104055/report.json PASS.
NPC_STAND_TEST=[180,120,60,120,3896575309,0,11], exactly equal to PC.
Tested XBE SHA256:cd894d8d11eaf2467d53c395a500e4629c3f9918d618be2d37a8b10321f818bc.
Compared with actor-pairs replay101509, PC output only adds the standing row
and intervening inactive impact telemetry; all preexisting rows are unchanged.
Bind normal/slow/crouch transitions, landing sound/player/special effects and
complete ground/relative callbacks before enabling NPC physics scheduling.


## Registered NPC crouch and post-ground clock (2026-09-12)

rf_scene_npc_crouch binds4289d0 to retained crouch centers and actor400,
then invokes a required ground callback and reads raw6460f0 clock bits only
AFTER it succeeds. The bits are stored in retained actor7b4, without treating
them as millisecond integers or converting their representation. Ground may
reenter stance and change flags/centers; the adapter preserves those changes
and mirrors final view/damage flags. On a ground error it preserves applied
changes and does not read/write the final clock. Missing services/cache or
stale registrations reject. Added one4-byte clock per NPC; its current initial
zero comes from the port owner allocation. Constructor initialization must be
verified before pre-crouch/jump reads rely on that initial value.

The actor-pairs fixture covers60 eligible NPCs times three cases: ordinary,
ground reentry, and injected ground failure. Each callback executes the real
registered ground query, then changes the clock source and writes a sentinel
into7b4. Success overwrites the sentinel with the new clock; error preserves
it and never invokes the clock callback. Reentry clears crouch/changes a center
and those mutations survive.11 sphere owners without stance caches reject
unchanged; seven sphere-free owners are excluded. All actor/sphere changes
are restored. Diagnostic ground queries here do not implement landing effects.

verify_physics_stance.py retains288 PC/NXDK center/endpoint comparisons and
adds128 full ORIGINAL4289d0 executions with a supplied reentrant ground call.
Whole original actor/sphere state confirms raw post-ground timestamp storage
and retained callback changes. This tail audit is original-only; the separate
native scene fixture verifies the adapter and callback contract. Three malformed
center guards and16 class-cache guards also pass. Both builds and19 CTests pass.

Stock64MiB XEMU180 frames PASS:
artifacts/xemu/replay-20260912-104658/report.json.
NPC_CROUCH_TEST=[180,180,120,60,1757960941,0,11], exactly equal to PC.
Tested XBE SHA256:4c990487c848d5e84fee679ab9c762e2f55c6b2b50d11ce9ca0b2c26a8c8b19f.
PC reference adds only the new crouch row and312 owner/total bytes compared
with104055; the existing body-state hash and all other rows are unchanged.
Connect normal/slow transitions and remaining landing callbacks, complete
ground/relative services, then enable NPC physics scheduling.


NPC movement constructor audit (2026-09-12)
------------------------------------------
The complete original40e380 constructor and type0 path487100 preserve actor75c and actor7b4, including A5/5A incoming heap patterns. verify_entity_construction.py explicitly checks both fields at these boundaries (30 checks), alongside its existing full5268-byte comparisons, allocation failures and list insertion checks. The allocation hook supplies raw storage, not a zeroing allocator. This disproves constructor-based zero initialization; it does not establish the first gameplay write in the complete422360 factory/callees. Retained port calloc zero must remain a provisional value until that lifecycle is established.
A direct-displacement executable scan identifies actor clock stores4256a4 (425280),4289bd (jump4288b0),428a45 (crouch4289d0),430cbf (player-associated input), and read428c5b. This is a candidate reference inventory, not proof against indirect accesses. The same displacement at4a342f belongs to another allocated owner and cannot justify NPC initialization. Further first-write/caller ordering remains open; no pre-write clock consumer has been enabled.


Embedded AI movement initialization (2026-09-12)
-----------------------------------------------
Follow-up resolves the clock initialization question above:422c9f..422caf passes actor+2a0, the actor handle and the player flag to402c20. Successful426fc0 lookup writes the resolved actor pointer into that embedded owner. Stores402d73/402d79/402d7f clear embedded510/514/518, which are actor7b0/7b4/7b8. Later402e96 stores EBP=-1 into embedded4bc, actor75c. This explains why direct actor-displacement searches missed initialization. The ordinary C++ constructor still preserves these bytes; the later AI initializer supplies their intended values.
verify_npc_movement_initialization.py executes128 registered original prefixes with unchanged timer callees and128 failed-lookup complete returns; random incoming actor bytes prevent accidental zero-default agreement. It separately executes128 attachment-store blocks using the prefix-established registers and compares every actor byte, and executes actual factory argument preparation. The intervening AI/resource services and full factory are not executed. Clock zero is now supported for successful AI initialization; retaining attachment75c and its later lifecycle remains required before normal/slow scene bindings. No additional live movement or visual progress is claimed.


Retained NPC attachment speed binding (2026-09-12)
------------------------------------------------
NPC bodies now retain actor75c initialized to -1 and explicitly initialize clock7b4 to zero from the verified embedded AI initializer. Both startup speed and rf_scene_npc_set_speed read this retained attachment field; the public scene API no longer accepts a caller-supplied forced-action value. The frame-zero fixture checks all78 initial attachment values, exercises624 speed requests across unattached/attached state, and restores attachment/movement/controller state. Later attachment creation, deletion and save restoration remain open.
PC/NXDK builds and all19 CTests pass. Native stock64MiB replay-20260912-110024 passes180 frames; NPC motion telemetry remains[78,624,312,1993876021,0]. Compared with104658, only NPC_BODIES allocation totals change by312 bytes: owner57604,total432244; body hash1320800694 unchanged. Tested XBE SHA256 b12795b21b7714a6450414a8bec485da685a0bbc4056aa88e140171da2fd811e. No NPC physics scheduling or new visual behavior is claimed.


NPC slow transition scene binding (2026-09-12)
--------------------------------------------
rf_scene_npc_slow binds428030 to retained class flags, registered standing/world clearance, speed/attachment ownership and movement descriptors. Nonwalking classes request slow speed only. Forced-crouch low byte sets flag400 without copying centers or refreshing ground, matching428030; otherwise crouched actors attempt standing and continue even when blocked. After standing callbacks the speed adapter rereads live retained movement/attachment state, then the transition installs descriptor1 (or0 when disabled), stable identity and zero vertical velocity. Applied standing effects survive callback errors; world/ground services are required when standing is attempted. Borrowed owners must survive callbacks.
The restored frame-zero scene fixture repeats all60 eligible NPCs across ordinary clearance, reentrant ground stance mutation and staged overhead blockage. Complete owner and sphere comparisons match independently composed standing plus speed results. Native stock64MiB replay-20260912-110433 passes180 frames with NPC_SLOW_TEST[180,60,2638938133,0]; existing telemetry matches110024. The ground fixture performs actual scene queries but is not full landing/support acceptance. Full512-case original428030/PC/NXDK verifier also passes (standing supplied at its boundary), as do both builds and19 CTests. Scene native coverage here is the walking/attempt-standing branch; forced-crouch and nonwalking branches have core original-oracle coverage, not added native scene cases. XBE SHA256 b056210842475cde09d43e372ff0a601345a2664bc4ebd4e202f4445d996d8af. Continuous NPC scheduling, normal/special transition bindings and complete ground dispatch remain open.


NPC normal transition scene binding (2026-09-12)
----------------------------------------------
rf_scene_npc_normal binds4280b0 to registered NPC standing, retained speed/attachment state, authored class movement index, descriptor enabled low byte and stable identity. Nonwalking classes only request normal speed. Walking classes attempt standing when crouched; blocked standing returns before speed, descriptor, region or vertical-velocity changes. After successful standing, clear retained previous_region_13ec before requesting speed, then install the authored descriptor (fallback0 if disabled) and zero vertical velocity. This differs deliberately from the slow transition. Previous-region pointer ownership adds4 bytes per NPC (312 bytes for78); constructor422360 explicitly clears13ec. Region entry and release scheduling remain open; pointers borrow campaign-owned regions. Invalid authored indices are rejected by this scene adapter, not represented as original null descriptors.
The restored scene fixture repeats60 eligible walking NPCs across clearance, reentrant ground mutation and blockage, stages campaign_regions.items as the previous-region pointer, and compares complete owner/sphere state. Native stock64MiB replay-20260912-110901 passes180 frames; NPC_NORMAL_TEST[180,60,2407052893,0]. Compared with110433, only the new row and312-byte body allocation increase differ (owner57916,total432556; body hash1320800694 unchanged). Both builds,19 CTests and128 original/PC/NXDK core climb-exit comparisons pass. The core oracle supplies standing; native scene coverage uses actual world clearance and a ground-query fixture, not full landing acceptance. XBE SHA256 7e3399f53b4927f3456cfcce0239a585e1f53b94d3ddac734d919be9d3ad2d14. Special-class landing effects and complete ground dispatch remain open; no continuous NPC physics or new visuals claimed.


Special landing navigation and AI boundary audit (2026-09-12)
-----------------------------------------------------------
The special-class branch419981..4199c5 is navigation/AI work, not a visual effect. It calls40b6c0(actor+588), which runs actual40a950 reset; then40c2c0(published3c,radius7c0,height7c4,1,&69c,&6a0,0); then408ac0(actor+2a0). The navigation return value is ignored. These query flags differ from death linked-actor query mode0/finalflag1 and must not be shared as fixed defaults. Navigation selections69c/6a0 and route state currently need retained ownership before binding.
Reset40a950 clears route offsets0/128/12c/160 and vector138..140; sets14/18/144 to -1; initializes11c/134 with actual4fa360(0) game-millisecond timers; sets byte15c=1 and byte170=0. Other route/actor bytes remain unchanged. AI408ac0 includes actor resolution, dead/disabled predicates, target/action branches, state changes, movement requests and peer notification; it cannot be replaced by a no-op. Query40c2c0 selects among navigation records using40c570,40b2e0,40b3d0 and fallback4991c0 visibility, with global-record mutations; replacing it with a world support ray would be incorrect.
verify_special_landing.py executes512 original branch cases with unchanged reset/timer/vector callees and only navigation/AI boundaries supplied. It checks every actor byte, exact query arguments, restored stack, callback order, query output visibility to AI, ignored query return (256 zero/256 nonzero), and retained AI crouch-flag mutation. This is original-code evidence only, not a PC/NXDK implementation or live special-class landing. Next dependency is navigation record ownership/selection and AI state dispatch; full goal remains open.


Shared navigation reset reconstruction (2026-09-12)
--------------------------------------------------
rf_entity_navigation_route preserves the original372-byte prefix through route170; named touched fields have verified offsets and unknown bytes remain uninterpreted. rf_entity_navigation_reset reconstructs40a950 with shared rf_timer_set(now,0), retaining all untouched storage. It rejects null state and clocks outside the existing valid game-clock domain before mutation. The supplied clock remains fixed across the two timer writes, as in the single-threaded original path with no intervening effect callbacks. No pointers are inferred from retained bytes.
verify_navigation_reset.py executes full original40a950 with actual timer/vector callees and no hooks, comparing1024 randomized prefix/clock cases against PC and linked NXDK machine code. It also verifies1024 repeated NXDK resets, four invalid-clock cases with unchanged storage, null-state rejection, and untouched surrounding original/NXDK bytes. Both builds and19 CTests pass. This is compiled NXDK execution in the oracle, not a new native XEMU run. No NPC route allocation or navigation selection has been enabled; scene ownership must reconcile adjacent existing NPC state before installation.


Navigation candidate eligibility (2026-09-12)
------------------------------------------
rf_entity_navigation_candidate_allowed reconstructs40c570 AL result from radius, height, mode, candidate1c/20 dimensions and candidate40 metadata. Each size comparison rejects only ordered greater-than. Actual x87 FCOMP/FNSTSW followed by TEST AH,41 accepts equality and unordered NaN comparisons, contrary to the raw Ghidra <= rendering. Mode low byte exactly1 additionally requires candidate40==0; mode0/2/255 and other low bytes do not. It reads no position and does not mutate the candidate. The first direct-oracle run exposed the NaN discrepancy; the implementation now uses !(radius>candidate_radius) and !(height>candidate_height).
verify_navigation_candidate.py compares4096 full unhooked original calls with PC and compiled NXDK:1471 accepted,2625 rejected. Cases include arbitrary float bits, signed zeros, infinities, NaNs, adjacent floats, equal dimensions, mode257/ffffff01 and arbitrary candidate40 values, with all original node bytes preserved. Both builds and19 CTests pass. This filter is part of40c2c0 selection, not the complete query or live NPC navigation. Candidate single/pair geometry40b2e0/40b3d0, mutable global navigation records and fallback visibility4991c0 remain to reconstruct. No new native XEMU run or visuals for this pure core helper.


Single navigation candidate geometry (2026-09-12)
------------------------------------------------
rf_entity_navigation_single reconstructs40b2e0 using an original-layout68-byte candidate prefix. It copies center0 to query point0c, optionally adjusts query Y by half actor height from candidate lower Y when mode low byte is nonzero, and stores the squared point distance at38 before classification. Vertical bounds include equality. Horizontal direct/overlap thresholds use squared candidate radius minus/plus squared actor radius, not squared radius differences. Both comparisons are strict; classification0=direct,1=overlap,2=outside. Stored intermediate vector differences, half-height and squared radii retain original float rounding before double-precision accumulation/comparisons. Unknown candidate bytes remain untouched.
verify_navigation_single.py executes complete original40b2e0 with actual vector/distance callees and no hooks at53-bit x87 precision.4096 finite cases match full68-byte records and classifications on PC/compiled NXDK (735 direct,281 overlap,3080 outside), including equality, zero/negative dimensions and low-byte mode variants. Ten nonfinite input guards preserve candidate/output; the adapted API requires distinct inputs. Both builds and19 CTests pass. Pair geometry40b3d0, ordered mutable candidate-list selection and visibility fallback remain open. No live navigation or new native XEMU replay is claimed for this core helper.


Navigation pair segment projection (2026-09-12)
-----------------------------------------------
rf_entity_navigation_closest_point reconstructs509100, used by40b3d0 to score partial pair overlaps. It stores end-start differences as floats, computes and stores segment length, stores inverse length before normalization, computes the projection in original Z/Y/X dot order, clamps the stored projection to[0,length], then stores scaled components before adding the start. Coincident endpoints publish start and zero distance. The returned scalar is distance along the segment, not a normalized fraction. Nonfinite or overflowing adapted geometry rejects without publishing outputs.
verify_navigation_closest.py executes full original509100 and unchanged callees with no hooks at53-bit x87 precision.4096 cases match PC/compiled NXDK closest-point bytes and float-stored original return distance, including512 coincident pairs and endpoints; nine nonfinite-input cases preserve outputs. Both builds and19 CTests pass. This supplies pair scoring, not full40b3d0: direction basis4fcea0, narrow/widened oriented box tests and ordered query ownership still remain. No live navigation or new XEMU run claimed.


Navigation direction basis (2026-09-12)
--------------------------------------
rf_entity_navigation_basis reconstructs4fcea0 for finite nonzero directions. Normalize the forward row using original full-precision reciprocal length, then use strict normalized X/Z bounds(-0.0001,+0.0001) for the special vertical basis. Otherwise normalize the horizontal right row and compute forward-cross-right as the up row. Original normalization keeps inverse length in x87 rather than rounding it to float first. Zero/nonfinite directions reject with unchanged output in this helper; the pair wrapper must separately preserve original coincident-candidate behavior.
verify_navigation_basis.py matches all nine matrix words across4096 unhooked original/PC/compiled NXDK cases, including249 vertical cases and adjacent threshold values. Four invalid-direction guards preserve output. Six additional full original40b3d0 cases with equal centers (same pointer or distinct candidate storage, three query positions) return2 and preserve the supplied score. Those execute all original callees with507a50 static vector constructors preinitialized via1754474=7; the initial attempted run without static initialization entered CRT exit registration and was not valid gameplay evidence. Both builds and19 CTests pass. Box composition and broader coincident/degenerate pair coverage remain open; no live scene navigation or new XEMU replay claimed.


Complete navigation pair geometry (2026-09-12)
---------------------------------------------
rf_entity_navigation_pair reconstructs40b3d0 using verified basis, oriented-box and segment-projection helpers. Center is a float-stored endpoint sum times0.5; forward uses first-minus-second; width is2*min(candidate radii)-2*actor radius, height is min(candidate heights), length is endpoint distance. A hit in the narrow box returns0 without changing score. Otherwise width increases by4*actor radius; a hit then writes squared distance to the segment projection and returns1. Miss returns2 and preserves score. Negative box dimensions cannot contain a point but widening can recover a positive width, so they do not abort the whole query. Coincident centers return2 with score preserved, matching the original NaN-basis path. All candidates remain unchanged. Adapted nonfinite/overflow inputs reject before publishing outputs.
verify_navigation_pair.py runs8192 complete unhooked original40b3d0 cases with all original geometry/vector callees and preinitialized507a50 static vectors. Exact PC/compiled NXDK classifications and conditional scores match:547 direct,1007 partial,6638 outside;1024 coincident pairs included. Tests preserve all68 candidate bytes, cover negative dimensions, vertical pairs, midpoint queries and14 nonfinite guards. Both builds and19 CTests pass. This completes pair geometry for the tested finite domain, not40c2c0 ordered mutable-list selection, fallback4991c0 visibility, scene navigation ownership or live AI. No new native XEMU run or visuals for this core composition.


Ordered navigation query reconstruction (2026-09-12)
---------------------------------------------------
rf_entity_navigation_select composes complete40c2c0 selection around verified candidate/single/pair geometry and an explicit visibility boundary. It first updates candidate35 rejection flags, then visits single candidates in order, then per-node ordered neighbor lists at original28 (NOT every global node pair). Pair traversal also preserves unsigned original node-address ordering via order_key. Direct hits return immediately, partial candidates keep strict lowest scores, and ties preserve the earlier winner. The original meaningful return is AL; adapted contained is normalized. Selection indexes use UINT32_MAX for null; a fallback node may be selected while contained remains0.
Fallback chooses the least scored unblocked candidate, subject to625 squared-distance unless allow_far low byte is nonzero, then requests4991c0(candidate query0c,actor point,2.5,1,0,0,0). A zero visibility low byte returns that first node with contained0; blocked nodes get35=1 and selection repeats. Borrowed collections remain stable and the supplied visibility operation must not mutate them. Preflight checks reject invalid refs/neighbor indexes/missing visibility before mutation; later failures preserve already-applied candidate and selection effects. Neighbor references presently must index the supplied node collection.
The first executable comparison exposed the missing per-node adjacency in the initial interpretation of raw Ghidra output; final code and fixtures use actual node28/30 lists. verify_navigation_select.py runs2048 complete original calls with unchanged geometry helpers, only4991c0 supplied, static box vectors preinitialized. PC/compiled NXDK match selections, low-byte return meaning,1600 ordered visibility calls and all candidate bytes. Outcomes:33 single,179 pair,774 fallback,1062 none. Fixtures include0..4 nodes, permuted address and adjacency order, disconnected graphs, linked pair-only cases and low-byte flags; three malformed preflight cases preserve outputs. Both builds and19 CTests pass. Native scene ownership, loading navigation records/adjacency, actual visibility and AI408ac0 remain open; no new XEMU run or live movement claimed.


### Authored navigation loader463d50 audit (2026-09-12)

`tools/inspect_navigation_records.py` reads the installed version180 navigation
sections in place from VPP archives. All94 sections consume exactly245088 bytes:
4752 nodes,74 oriented records,90 tag words and10526 directed connections.
L1S1 contains333 nodes, one orientation, three tags and760 connections in17078
bytes. No original assets or extracted records are tracked.

`tools/verify_navigation_loader.py` executes original463d50 and the actual
40e9e0 constructor, vector/matrix operations, array lookup and466220 duplicate
filter. Boundaries supplied are version180 stream reads,124-byte allocations
initialized with poison, and45ec40 array growth. Comparisons cover all124 node
bytes plus four guard bytes, full ordered tag arrays, global insertion order,
full ordered adjacency arrays, exact section consumption and stack restoration.
All94 authored sections/4752 nodes pass. Additional empty and three-node cases
exercise noncanonical boolean bytes, both orientation branches, duplicate tags,
self/duplicate connections and out-of-range/negative connection indices.

Records contain UID, discarded enabled byte, height, position, radius, node040
word, orientation flag and optional36-byte matrix, three discarded boolean
bytes, node024 float, and tag count/words. Adjacency follows all node records,
with a byte count and32-bit indices per node. Original466220 removes duplicate
neighbors preserving first occurrence; tags retain duplicates. Invalid signed
indices are skipped. The installed authored connections need no such filtering.

40e9e0 initializes array headers028 and070 and byte034 only. Vector and matrix
constructors leave their storage untouched. Loader copies radius to018 and01c,
height to020, the additional float to024, word040, orientation flag044, optional
matrix048 and UID06c. Query scratch and unoriented matrix bytes remain poison
in this audit; they must not be mistaken for authored zero initialization.

This establishes original loader semantics for version180, not shared C runtime
ownership, older formats, original filesystem/allocator behavior, navigation
visibility4991c0, live AI408ac0, or native Xbox execution. Next implement shared
owned node/tag/adjacency storage, retaining collection and original-relative
node order for the verified40c2c0 query, then bind actual visibility and AI.


### Shared owned navigation records (2026-09-12)

`rf_level_owned_navigation_open/close` now implement the audited version180
section20000 layout in shared C. Two bounded streaming passes measure and load
nodes, tags and directed adjacency into one stable allocation; no section-sized
staging buffer is allocated. Budget includes the20-byte x86 owner,120-byte node,
16-byte query reference and4 bytes per serialized tag/edge. Invalid/duplicate
edges are filtered in original order; reserved capacity remains within budget.
Opening L1S1 uses48360 bytes, also the maximum among the94 installed sections.

The owner retains UID, optional orientation, unknown024/040 values and all tag
words. Query references point to stable candidate prefixes and ordered neighbor
indices. Keys follow the port's contiguous node-address order; equivalence to
an arbitrary original heap allocation history is not claimed. Scratch fields,
uninterpreted prefix bytes and absent matrices initialize deterministically to
zero in the port; this is not an original constructor write-footprint claim.
Close invalidates all borrowed references, frees storage and clears the owner.

`tools/verify_navigation_owned.py` compares all94 authored sections/4752 nodes
on PC and compiled NXDK against the original-audited record reader, including
complete68-byte candidate prefixes, orientation, UID, tags, query-reference
identity/order and adjacency. PC closes the source archive before exporting
retained records. Both paths check exact/insufficient budgets, nonempty-owner
rejection, truncation and repeat close. NXDK additionally tests allocation
failure, post-allocation I/O failure cleanup, empty sections, duplicate/self/
invalid edges, duplicate tags and noncanonical orientation boolean values.
NXDK executes actual compiled loader and level bounds with only rf_vpp_read,
calloc and free supplied. PC/NXDK builds and all19 CTests pass.

These owners are not yet retained by the live campaign scene. Next add scene
creation/retirement and budget telemetry, verify stock64MiB XEMU, then compose
route/query ownership with actual4991c0 visibility and408ac0 AI. Native XEMU
navigation behavior, original heap-relative ordering across arbitrary lifetimes,
older RFL versions and continuous NPC navigation remain unverified.


### Native scene navigation residency (2026-09-12)

Campaign startup now opens the shared navigation owner alongside retained
movement regions, before registered NPC creation. It has a65536-byte budget,
propagates load failures through scene cleanup, and closes after NPC teardown.
A missing section is allowed. It does not yet execute NPC route queries.

`rf_scene_navigation` publishes node/edge/tag/oriented counts, allocated bytes
and a pointer-independent FNV hash of UID/orientation/tag/neighbor/key headers,
complete candidate prefixes, matrices, tags and ordered neighbor indices. The
PC replay emits NAVIGATION, and the native harness reads the same guest words.
L1S1 additionally requires the independently computed original-record hash.

Stock64MiB XEMU `replay-20260912-120133` passed180 frames with actor-pair fixtures:
NAVIGATION [333,760,3,1,48360,916127865]. XBE SHA256 is
`eb8a6fea07eb36ff3179ab60f5da7a3582970adc83301e0db7025639a6894c14`.
Guest base memory is67108864 bytes with zero plugged memory. Comparing PC
reference output to the prior110901 replay finds only the added NAVIGATION row;
all existing rows are unchanged. Both builds and all19 CTests pass. Native
harness completed its restore/rebuild cleanup successfully. No new visual was
produced, so no screenshot was captured.

Next bind route/query ownership, then original4991c0 visibility and408ac0 AI.
Fresh raw4991c0 decompilation shows three ordered object-list traversals,
object-size/flag/exclusion filtering, bounds tests and5031f0 model queries before
498e80 world visibility; a static-world-only callback would be incomplete.
This is preliminary inspection, not a verified reconstruction of4991c0. Its
third parameter needs semantic treatment as an object-size threshold rather
than assuming it defines a swept visibility sphere. Continuous NPC navigation,
actual visibility callbacks and campaign-wide behavior remain open.


### Full4991c0 visibility orchestration audit (2026-09-12)

`tools/verify_visibility_original.py` executes full original4991c0 with actual
40a110 predicate,508b70 segment/AABB, constructors and vector/matrix helpers.
Only5031f0 model effects and498e80 world results are supplied.4096 cases cover
empty/partial/permuted three-list traversal, size comparisons including NaN and
infinity, exclusion pointers, hidden/flag2 filtering, broadphase misses,
callback-mutated start/displacement/flags and nonboolean low-byte returns.
Complete initialized80-byte model query prefixes and ordered callback traces
match; original object storage, input endpoints and caller stack are preserved.
There are2189 model calls,3546 world calls,550 early returns,745 query restores,
841 middle-list clipping operations and1640 nonboolean final results.

The float argument is a minimum object extent at object180, not a sphere radius.
FCOMP/FNSTSW/TEST AH,1 rejects smaller and unordered extents; equality passes.
Every list rejects object flags4000 through40a110. First/third lists additionally
reject flag2; the middle list does not. All lists exclude both supplied object
pointers and require the original508b70 test against bounds190/19c using the
original caller endpoints, even after working model displacement is shortened.

A104-byte model query is created with body positione4, matrixfc, caller start,
end-minus-start displacement, radius0 and flags=(caller_flags&1). Its first80
bytes are initialized; remaining scratch is not initialized by the constructor.
Model calls receive model80, query,32-byte hit and reset1. Their low-byte results
are ORed. After each middle-list model call, displacement becomes original
displacement times returned hit.time, even for a model miss. On accumulated
nonzero result with caller bit1 set, return canonical1 immediately. Otherwise
query flag2 restores the original start/displacement and clears only flag2;
other callback flag changes survive. The middle-list clip occurs before that
restore/early return. Later model queries see the resulting working segment.

If no early return occurs,498e80 receives the unchanged caller start/end, full
caller flags and final context argument; its low byte is ORed with accumulated
model results without boolean normalization. The shared navigation callback
contract now correctly describes its2.5 argument as minimum object extent.

Next reconstruct this full orchestration in shared C with ordered stable owner
views and real AABB helper, verify PC/NXDK against these original traces, then
bind model/world geometry and actual three-list ownership. This audit does not
prove5031f0/498e80 geometry internals, shared C visibility or native gameplay.


### Shared full visibility traversal4991c0 (2026-09-12)

`rf_collision_visibility` now implements the verified complete three-list
traversal in shared C. Borrowed ordered88-byte object views carry identity token,
flags, extent, model, body pose and AABB. Lists remain stable during callbacks;
nonzero identity tokens represent original object pointers for exclusions.
The function composes actual shared508b70 bounds checks with required5031f0
model and498e80 world services. No heap allocation or fabricated clear path.

Size comparisons reject unordered values. All lists reject4000; only first and
third reject2. Model query radius stays0; caller bit1 initializes flags and
controls canonical early return1. The middle list rescales original displacement
by returned hit time even on misses; callback flag2 restores original endpoints
and clears only that flag. Final world receives original endpoints/full flags/
context, and its low byte is ORed without boolean normalization. Port scratch
starts at zero, distinct from the original uninitialized constructor bytes.

`tools/verify_visibility_original.py --shared` compares4096 complete original,
PC and compiled NXDK cases, including randomized float endpoints and non-exact
hit-time products, exact initialized80-byte model queries, ordered effects and
return bytes. Latest fixture totals:2183 model calls,3504 world calls,592 early
returns,759 restores,881 middle-list clips,1568 nonboolean results. Seven NXDK
error cases check null inputs/backend, missing list storage, NaN endpoint,
inverted bounds and model/world callback failures. Result stays untouched on
errors; already-observed callback effects are not rolled back. Full PC binary
probe outputs match original traces. Both builds and all19 CTests pass.

The oracle supplies only model/world effect boundaries; this is compiled NXDK
execution under Unicorn, not native XEMU. Next bind the original three object
lists to retained owners and complete5031f0/498e80 geometry services, then use
this visibility path from scene-owned navigation40c2c0 and AI408ac0. Object-list
identity/order, current poses, lifetime and exclusion mapping require actual
owner evidence; current startup NPCs alone are not the complete visibility set.


### Visibility family identity and insertion order (2026-09-12)

4991c0 traverses actors (5cb060 sentinel), clutter (5c9360), then corpses
(5cabb8). The earlier420d00 note incorrectly called5c9360 an entity list; it
is corrected. Clutter factory4104a0 contains original clutte source/assertion
strings, creates type4 through486da0, and appends to5c9360. Corpse416940 appends
to5cabb8; actor422360 appends to5cb060. These are distinct from the global
allocation list at object10/14 and73d880.

`tools/verify_visibility_lists.py` executes the unmodified final append regions:
actors4236ad..4236fc, clutter4109d4..410a0b, corpses416d75..416da3. Each writes
object28c next/object290 previous and appends at its sentinel tail. Counters are
62f2d4,5c9358 and5caed0 respectively.3072 total append operations across shuffled
creation order and wrapping initial counters match the existing compiled NXDK
intrusive-list helper after pointer normalization. Every new-node byte is checked,
including actor13ec/13f0 zeroes and1384/1388 minus-one stores in that region.
Forward and backward traversal both preserve successful factory insertion order.
No full factory, constructor, removal or live registration sequence is claimed.

The visibility API now names the three families and explicitly requires factory
insertion order rather than UID/handle order. Existing scene startup NPC storage
is insufficient for the complete visibility population: the shared level/scene
owners currently contain no retained clutter collection. Section50000 dispatch
in460820 calls465220, which reads authored clutter and invokes class lookup410b60
and factory4104a0, followed by linkage/resource publication. Fresh465220 raw
output is available for the next loader audit. Recover its common464f90 payload,
class/model selection and successful creation order before supplying the middle
visibility list. Actor/corpse lifetime, live model geometry and world498e80
services remain required; do not replace missing clutter with an empty list and
claim complete scene visibility.


### Authored clutter465220/464f90 loader audit (2026-09-12)

`tools/inspect_clutter_records.py` consumes every byte of all91 installed
version180 section50000 payloads:12659 clutter records,787 association words
and2854 nonempty trailing resource names. L1S1 contains170 records in17531 bytes,
67 resource names and no associations. Its13 classes are primarily mine/pole/
runway/portable lights, plants/vines and shards. Three installed levels have no
clutter section. Raw assets and extracted per-record content remain untracked.

Record order is UID, class string, position12, orientation36, instance name,
discarded enabled byte, common count with(string,u32) pairs, resource string,
association count and u32 association IDs. Strings have u16 byte lengths.
Original464f90 consumes and discards common pairs; installed records have none.
The trailing resource name remains a provisional label until410d30 is audited.
Do not infer texture/animation semantics from its position in the record.

`tools/verify_clutter_loader.py` executes full original465220 and464f90 with
actual vector/matrix wrappers, array lookup and string inequality. Version180
I/O, string storage, class lookup410b60, factory4104a0, array growth, resource
lookup410d30 and post callback525c70 are supplied. All authored records pass in
both successful and failed factory modes. Tests compare exact factory class/
instance/pose arguments, full2d8-byte successful-node write footprint, UID20,
resource2bc publication, ordered2c0 associations and post-callback sequence.
The lookup collection includes missing IDs and duplicate IDs with differing
handles, confirming first matching record wins; repeated authored links remain
repeated. A synthetic two-record section covers common pairs, duplicate keys,
noncanonical enabled bytes, repeated links and empty/nonempty resource names.

4104a0 receives resolved class, instance name,-1, position, matrix,1. On success,
each authored association searches646098 in order by first word; the matched
record's94 word is appended to clutter2c0. Missing matches are ignored. UID is
published after associations and before optional410d30 resource lookup. Nonempty
resource names publish the returned word at2bc, then525c70(0,0) executes. Failed
factories still consume the entire record but skip all these publications.

No shared C clutter owner, actual class/model factory, association-target owner,
resource semantics or native clutter visibility is claimed. Next implement
budgeted shared record ownership retaining raw unknown names/pairs/IDs, validate
PC/NXDK against this audit, then recover class/model initialization4104a0 and
ordered successful creation for the visibility middle list.


### Shared compact authored clutter ownership (2026-09-12)

`rf_level_owned_clutter_open/close` now retains version180 section50000 in shared
C. A single allocation holds92-byte x86 records, the complete raw section and
exact-length terminated copies of class/instance/resource names. The owner is
16 bytes. Record-relative common/link offsets and counts preserve uninterpreted
pairs and association IDs without pointer casts or fixed-size string padding.
`rf_level_clutter_link` reads one association word with span/index checks.

The loader first measures bounded records/names, allocates within the caller
budget, retains raw bytes and populates records. There is no extra section-sized
staging allocation. The source must remain stable during open; all retained
fields survive source closure. Empty destinations are required, failures free
partial storage and preserve output, and close clears the owner and is repeatable.
Three factory-facing names reject embedded NUL; unknown common-pair bytes remain
raw. Allocation accounting excludes allocator metadata/stack, as other owners do.

`tools/verify_clutter_owned.py` compares all91 authored sections/12659 records
against the original-audited reader on PC and compiled NXDK: exact fields, poses,
terminated names, raw spans and offsets, associations, exact/insufficient budgets,
nonempty destination and truncation rejection. PC closes archives before export.
NXDK additionally exercises allocation and post-allocation I/O failure cleanup,
empty sections, a long name, common pairs, duplicate links and embedded-NUL
rejection. The compiled loader and level bounds execute; only VPP reads/malloc/
free are supplied. Opening L1S1 uses39834 bytes and the maximum installed case
uses105016 bytes. Both builds and all19 CTests pass.

This owns authored data, not successful runtime clutter instances or models.
Next recover4104a0 class/model initialization and the still-provisional trailing
resource semantics410d30, bind scene lifetime and actual creation order, then
verify native64MiB residency and complete the clutter visibility population.
No new native XEMU clutter or rendering result is claimed by these tests.


### Clutter skin variant semantics410d30 (2026-09-12)

The trailing authored clutter resource_name field is now identified as a skin
variant. Original clutter.tbl declares $Skin names with ordered replacement
textures; opening records use Yellowish, matching the runway-light variants.
The public field keeps its existing spelling with an explicit semantic comment.
410d30 searches class294->c8, an array of108-byte variants. Each begins with an
8-byte string, texture count at8 and up to the record's capacity of8-byte texture
strings atc. A parallel classd4 array supplies glare-class indices.

`tools/verify_clutter_skin.py` executes full original410d30, actual string equality
5001d0/57c130, glare retarget4153e0 and material replacement48ac00. Only model
material lookup503650 and texture-loading50f6a0 are supplied.2048 cases pass:
911 selected,1137 missing,1647 texture calls and1374 matching glare-node updates.
Case variants, duplicate names, empty names/lists, differing texture/material
counts, negative/out-of-range glare indices and missing texture handles are
covered. Exact actor, complete material records and glare nodes are compared.

Names compare ASCII case-insensitively and the first match wins. Missing names
return-1 without texture/glare effects. A matched nonnegative glare index calls
4153e0 with the clutter handle; it updates every glare node whose30 parent handle
matches, provided the index is inside global5cab98. Node2ac becomes the selected
52-byte glare-class pointer and2b0 its index. Nonmatching nodes stay unchanged.

48ac00 obtains model material count/storage, then replaces texture word10 in each
200-byte material record for min(variant texture count, material count) entries.
It calls50f6a0(name,-1,1) in order and publishes the returned handle even if-1;
remaining material bytes/entries stay unchanged.410d30 returns the selected index
but does not write clutter2bc itself;465220 performs that publication afterward.
No shared skin/glare ownership, real texture residency or native rendering proof
is claimed. Existing entity material code can inform the binding, but clutter's
parallel glare selection must be retained.

Fresh original40f410/40f4f0 output identifies clutter.tbl class parsing and its
232-byte class records.4104a0 assembles the common486da0 type4 factory input from
class model/name, kind38, radius40, material4c, authored pose and class flags74,
then initializes life, sound, emitters, glare/light props and final list insertion.
Those complete class/model/resource owners still need reconstruction before
scene visibility can claim the authored clutter population.


### Shared clutter skin application (2026-09-12)

`rf_clutter_skin_apply` now composes410d30 selection,4153e0 matching-parent glare
retarget and48ac00 primary material replacement in shared C. Compact borrowed
variant views retain case-insensitive names, ordered textures and glare index.
Glare views retain parent identity, class index and class-record pointer. Existing
200-byte model material records receive only word10 texture updates. Model
material lookup and texture loading are required resource services; no allocation
or actual scene ownership is fabricated by the helper.

First matching skin wins. Missing skins return success with selected=-1 and no
effects. Valid glare indices retarget every matching parent before model lookup.
Nonpositive material/texture counts make no texture calls. Each replacement calls
the loader with(name,-1,1), publishes even a returned-1 handle, and leaves other
material bytes unchanged. Errors preserve selected while retaining completed
glare/texture effects. The caller still owns publication of actor2bc.

`tools/verify_clutter_skin.py --shared` executes2048 full original cases against
PC and compiled NXDK:904 selected,1144 missing,1319 texture calls and1374 glare
updates. Cases include nonpositive material counts, duplicate/case-varied names,
empty inputs, mismatched material/texture counts and missing texture handles.
Exact result, ordered resource calls, full material records and glare views match.
Seven additional NXDK errors cover missing owners/services/texture storage,
model lookup failure after glare changes, and a second texture failure retaining
the first material replacement. Both builds and all19 CTests pass.

This is compiled NXDK under Unicorn with resource boundaries supplied, not native
XEMU or live clutter rendering. Next recover complete clutter.tbl class/model
inputs and4104a0 factory ownership, bind actual texture/glare resources and retain
successful creation order for scene visibility. Do not treat this helper as a
complete class factory or release/replacement resource policy.


Clutter asset metadata (2026-09-12)
-----------------------------------
Shared rf_clutter_assets_read selects bounded clutter.tbl model and ordered
replacement names without allocation, preserving output on failure. It shares
the existing entity metadata lexer while retaining entity duplicate-skin errors;
clutter selects the first matching skin. Empty skin selects base materials.

verify_clutter_assets.py executes original410b60 with supplied authored class
string storage, including actual500190/57c130 comparison:430 lookup cases pass.
The431 declarations contain429 selectable case-insensitive names; later Cart
and firehose declarations are shadowed by earlier names. Independent table
declarations match complete PC/compiled NXDK output for494 authored selections.
Six synthetic cases cover first duplicate skin, base materials, absent class/
skin, malformed quoting and replacement overflow with unchanged error output.
The NXDK oracle supplies only stack growth on an already mapped fixture stack.

This is metadata selection, not complete original40f4f0 parser equivalence,
4104a0 factory creation, compiled-filename conversion, glare resource lookup or
native XEMU integration. Complete class/body ownership and live scene binding
remain open. No new visual is claimed.
Both PC/NXDK builds pass; existing entity metadata63 classes/168 selections
and all19 CTests pass after sharing the lexer-based selector.


Original clutter factory orchestration (2026-09-12)
--------------------------------------------------
verify_clutter_factory.py executes complete4104a0:512 synthetic cases pass,
465 successful allocations,47 failures,698 emitter requests,688 glare creations
and243 persistent slot registrations. Original SHA256 remains
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.
Full152-byte allocation descriptor, ordered calls, complete728-byte object and
232-byte class write footprints, list links and counters are checked. Failure
publishes no class/object changes and makes no subsequent resource calls.

The descriptor passes type4, handle-1, caller identifier, zeroed152-byte record,
riot-shield flag100000 when applicable and final0. Descriptor fields are model
pointer00, kind04, material10, position3c, matrix48, radius84, flags94=20 if
class flags contain either bit2 or4. Object294/298 receive class pointer/index.
Object flags map class20->100000, class2->40000, class1->1000. Negative life
sets100 health and object flag4; nonnegative life is copied. Empty instance name
falls back to class name. Armor38 is zero. Corpse class29c is looked up only
when its class name is nonempty. Ambient sound50 uses434da0 then5056a0.

Emitters visit authored order, skip negative IDs and prepend successful creates
through object268/emitter150. Actual40a490 on the object supplies its first
word as emitter argument3; its meaning is not inferred. Timer2a4 is cleared;
2a8=-1. Positive emitter lifetime schedules2b0 using trunc(lifetime*1000+0.5),
otherwise clears it. Class70 receives current clock. Object2b4 is cleared;
2b8=0,2bc=-1,2d0=-1. Class glare7c!=-1 and missing flag400 discover sequential
corona_1 onward tags until missing, cache IDs at84 and mark flag400. Cached
entries create glares each instance. corona_rod1 requires corona_rod2; a
nonnegative rod class80 creates a rod. Class flag10 without800 resolves
light_prop into class98 and marks800. Class flag8 creates a screen with e0/e4
dimensions. Nonnegative explosion58 is registered. Eligible collision discovery
precedes list append/counter increment. Optional persistent slot assignment
follows append when requested and5afb84<3200, storing the old slot in2d4 and
registering it through50ea00. Otherwise2d4 remainsffff.

Actual descriptor constructors/destructor, vector/matrix copies, class lookup,
string comparison, array reads, float conversion and timers execute. Supplied
boundaries are generic allocation, string storage/formatting, model tag lookup,
array append storage, sound/emitter/glare/rod/screen/explosion/collision/slot
effects. Cached/uncached paths, failed emitters, absent sounds/corpses and slot
capacity are covered. This verifies factory control/data flow, not resource
implementations, full class parser, shared C factory, invalid-class handling,
malformed rod assertion loop, native XEMU or live scene integration. Next
reconstruct factory/resource ownership and bind retained clutter in successful
creation order. No new visual is claimed. Production source is unchanged;
no build or native replay is required for this original-executable oracle.


Shared clutter factory (2026-09-12)
----------------------------------
rf_clutter_create in model.c / rf/clutter.h reconstructs full4104a0 orchestration
with caller-owned class and object views, generic allocation and explicit
resource services. Class96-byte and state108-byte x86 views preserve retained
class timer/corona/light caches and per-instance health/flags/sound/emitter/
corpse/timer/skin/slot state. No heap allocation occurs in the orchestration.
Generic allocation receives a76-byte semantic descriptor corresponding to the
original152-byte zero-filled record and fixed type4/handle-1/final0 arguments.
Creation order uses the verified intrusive tail append before slot registration.
Names are borrowed stable caller storage; original string allocation is not
reproduced. Resource implementations, resource retirement and live scheduling
remain separate. Out publishes the allocated owner immediately, including on
later service failure, so partial resources remain reachable for cleanup.

verify_clutter_factory.py --shared passes512 full-original/PC/compiled NXDK
cases:465 successes,47 empty allocations,698 emitter calls,862 glare calls,
243 slot registrations. Complete semantic state/class outputs and exact PC/
NXDK request records match, including descriptor/position/matrix arguments.
Normalized results match original object/class write footprints and resource
call order. Original411e40 now executes directly: capacity is four IDs, and
factory tag discovery continues past capacity until the first missing tag.
Expanded fixtures discover up to seven tags and retain only the first four.
The historical688-glare result above used a narrower fixture distribution.

Ten additional compiled-NXDK cases cover invalid class index, nonfinite position,
missing class name/backend, invalid time/cache count, allocation service error,
tag service error after allocation, slot service error after list publication,
and missing second rod tag (RF_FORMAT replaces the original assertion loop).
Output remains unchanged before allocation; partial owner remains accessible
on later errors, and slot failure retains list publication and slot increment.
All19 CTests and PC/NXDK builds pass. NXDK execution here is Unicorn, not a
native XEMU replay. No live clutter or new visual is claimed. Next establish
owned complete class definitions, generic model/body creation and actual
sound/emitter/glare/collision/slot resources, then bind retained level records
and successful creation order to scene visibility.


Authored clutter flag parsing (2026-09-12)
----------------------------------------
rf_clutter_flags_read reconstructs513020 with the nine-entry vocabulary selected
at40f957..40f960 from593f20: collectable1, collide_weapon2, collide_object4,
is_screen8, shatters10, has_alpha20, is_switch40, can_carry80, is_clock100.
Only these nine names are authored flags. The next pointer words reference
clock texture filenames, not additional flag names. Runtime corona-cache400
and light-cache800 bits are set by4104a0, not by the authored parser.

verify_clutter_flags.py executes unmodified513020 and its parser/CRT helpers;
no hooks replace token reading or comparison. All431 installed declarations
and all512 bit combinations match PC and compiled NXDK output and exact byte
consumption. Synthetic combinations include uppercase, duplicate names,
comments and trailing input. Six PC/NXDK malformed/unknown flag cases preserve
both outputs and return RF_FORMAT; the original fatal diagnostic is not invoked.
The reader handles one parenthesized list without allocation; the full class
loader must pass its remaining text and advance by the returned byte count.
This is a class-loader dependency, not complete40f4f0, resource ownership or
live scene integration. No new visual or native XEMU replay is claimed.
PC/NXDK builds and all19 CTests pass after this addition.


Original clutter class defaults (2026-09-12)
------------------------------------------
verify_clutter_class_defaults.py executes40f4f0 through40f9b7 across256 synthetic
cases, supplying tag presence, decoded strings/numbers, material index and flag
bits. Actual default/write branches, extension extraction5143f0 and string case
comparison execute. Required class/model names and life are published; model
kind38 is3 for .vfx ignoring case, otherwise1 including .vcm or no extension.
Material4c is one byte: the following three bytes remain untouched.

Absent emitter lifetime34 and radius40 become-1. Sound50, use-sound54,
explosion58, glare7c and rod80 become-1. Explosion radius scale5c and damage60
default to1. Explosion offset64 is zeroed. All eleven damage factors9c start
at1. Required flags74 receive the parsed mask. Optional screen e0/e4 dimensions
default64. Six independent optional scalar fields exercise present/absent cases:
emitter lifetime, radius, explosion radius, explosion damage, width and height.
Checks include zero, negative and positive float values and zero dimensions;
this records the original parser's writes, not a new restriction on table data.
The installed table has431 class declarations and at most five emitters per
class, but no authored radius overrides or rod-glare declarations, so installed
content alone would not validate those default paths.

This is explicitly a prefix oracle, not full40f4f0 equivalence: parsing beyond
40f9b7 (use/light metadata and skins), resource-name resolution and complete
class allocation/ownership remain open. String/array preconditions are supplied;
this does not execute the table-reset driver. No shared class loader or new
visual is claimed. Production source is unchanged, so no rebuild/native replay
is needed for this original-executable verification step. Next implement owned
factory-facing class input loading, using these verified defaults and the shared
flag reader, then bind generic model/body and actual effect resources.


Owned factory-facing clutter metadata (2026-09-12)
------------------------------------------------
rf_clutter_definition_read selects the first ASCII-insensitive class and copies
name, model, corpse, material, sound, explosion, base glare, rod glare and up to
16 ordered emitter names, plus emitter lifetime, model kind, life, radius,
flags and screen dimensions. Output1568-byte metadata owns all names and has
no archive/text pointers. Read performs no allocation and preserves output on
failure. Model/material/life/flags are required; repeated selected fields fail.
Defaults use the verified original parser-prefix behavior, including .vfx kind3
and other model extensions kind1. Original literal5942a8 is $Rod Glare:, not an
inferred $Glare Rod spelling. Parsing stops before $Skin: so skin glare metadata
cannot overwrite the base class glare. Skin materials/glare selection remain
in their separately verified helpers.

verify_clutter_definition.py checks all429 selectable classes from431 installed
declarations against independent Python metadata extraction, with byte-identical
PC and compiled NXDK output. Ten synthetic cases cover base defaults, missing
radius/rod/dimension authored coverage, emitter order, base-versus-skin glare,
missing life/class, duplicate life, nonfinite radius, emitter overflow, malformed
quotes and oversized names. Both builds and all19 CTests pass. The reader uses
a bounded port grammar; it does not claim full original40f4f0 equivalence.

This record is a temporary metadata representation, not the compact live class
owner. Runtime names and emitter IDs still need compact budgeted storage and
resource resolution into the verified96-byte class view. Damage/debris/use/light
metadata, full class parsing, resource allocation/release and live level binding
remain open. No native XEMU replay or new visual is claimed for this change.


Compact clutter class ownership (2026-09-12)
------------------------------------------
rf_clutter_classes_open converts factory-facing definitions plus explicitly
supplied resource bindings into one allocation:96-byte runtime class records,
ordered int32 emitter IDs and exact terminated name/model/corpse strings. Class
and duplicate-name order are retained. The owner16 bytes plus payload are
included in allocated_bytes and checked against budget before allocation.
No source definitions, binding arrays or emitter arrays are borrowed. No resource
lookup/load/release occurs: referenced IDs remain owned by external systems.
Factory caches start empty, with port initial timer0/light tag-1. Classes must
outlive borrowing objects; close frees only this storage and is repeatable.

verify_clutter_classes.py uses all429 parsed selectable definitions and supplied
material/effect/emitter IDs. PC and compiled NXDK exact-budget results match
complete scalar/name/emitter contents after inputs are overwritten/freed. These
classes occupy55379 bytes including owner, versus672672 bytes for the temporary
metadata records. Nine scenarios cover the complete set, empty set and duplicate
classes, each at exact/short budget and injected NXDK allocation failure. Empty
ownership needs16 budget bytes but no allocation. Eight additional NXDK guards
cover missing emitter storage, unterminated model, invalid material/life/flags,
nonempty output owner, empty class name and invalid model kind. All errors
preserve output; invalid inputs make no allocation. PC/NXDK builds and all19
CTests pass. This is port-owned storage, not original allocator equivalence or
native XEMU residency proof. Resource resolution, generic model/body allocation,
complete class metadata, skin/effect owners and live level integration remain.
No new visual is claimed.


Clutter resource-name lookup (2026-09-12)
---------------------------------------
Shared rf_clutter_material_index reproduces4686c0's ten fixed names and default0
fallback using ASCII-insensitive comparison. rf_glare_name_lookup reproduces
415430's first exact bytewise match: glare names are CASE-SENSITIVE. Shared
rf_emitter_name_lookup reproduces497550's first ASCII-insensitive match. These
helpers perform no allocation, registration or resource loading. Original glare
records have52-byte stride at5c9e98, count5cab98; original emitter name string
records are8 bytes at7b2870, count7bd99c. Glare comparison is inlined bytewise;
material/emitter comparison uses the existing original string/CRT helpers.

verify_clutter_resource_lookup.py executes all three unmodified original lookup
functions against supplied authored/synthetic name storage.510 cases match PC
and compiled NXDK, including case variants, duplicate first match, empty names,
unknown names and the last slot. Seven additional NXDK guards cover invalid
arrays/counts/null queries and material null fallback. Fixtures use56 effects.tbl
glare names,52 emitter names, the ten original material names and94 clutter glare
references. One distinct clutter glare reference, CoolWedge02_SmallCone_NoVolume,
is absent from the authored glare table; original lookup returns-1, which is
preserved. No reference fails solely due to case. Do not silently correct or
substitute this missing resource while binding classes.

PC/NXDK builds and all19 CTests pass. Authored names are supplied storage here;
this does not prove the original table loader or native XEMU behavior. Sound and
vclip lookup already have separate verified helpers; composing these lookups
with class binding, complete effect owners and actual resource loading remains
open. No new visual is claimed.


Clutter class resource binding (2026-09-12)
-----------------------------------------
rf_clutter_definition_bind composes verified emitter/material/Foley/vclip/glare
lookup into the factory binding, retaining original missing-name results. It
uses ordered caller-owned catalogs and copies emitter IDs into caller output;
no allocation, registration, loading or resource release occurs. Material names
are insensitive with default0 fallback, emitter/Foley/vclip names are insensitive,
and glare/rod names are exact. Both outputs remain unchanged on errors.

Definition metadata now includes resource_fields with sound1/explosion2/glare4/
rod8 presence bits, increasing the temporary record from1568 to1572 bytes.
Absent optional fields stay-1 without lookup. Explicit empty fields still invoke
the relevant lookup: empty glare can match an empty catalog slot, while empty
Foley/vclip names remain-1. This distinction must survive parsing even where
installed catalogs contain no empty names. Runtime class size and compact owner
size are unchanged:429 classes still occupy55379 bytes; temporary metadata is
now674388 bytes. The loader must release temporary metadata after construction.

verify_clutter_binding.py compares all429 selectable classes with actual original
4686c0/434cb0/4c1d00/415430/497550 results using authored catalog storage. PC and
compiled NXDK match.51 synthetic cases cover all16 presence masks with empty,
missing and mixed-case names, duplicate catalog entries, insufficient emitter
capacity, invalid presence bits and unterminated material. Four additional NXDK
missing-catalog cases preserve binding and emitter outputs. Updated metadata
verification passes429 classes plus10 synthetic cases including presence bits;
compact ownership passes its nine scenarios and eight guards. Both builds and
all19 CTests pass. Original full class parser and resource loading are not
executed by this binding oracle. Catalog ownership, complete class/effect data,
generic model/body allocation and live level/factory integration remain open.
No native XEMU replay or new visual is claimed.


Archive-backed clutter resource catalogs (2026-09-12)
----------------------------------------------------
rf_clutter_catalogs_open loads authored name order from emitters.tbl's particle
emitter types section, effects.tbl's Glares section and vclip.tbl's Vclips
section. It owns pointer tables and exact terminated names in one allocation,
while reusing one temporary archive buffer across both passes. Vclips expose
64 slots, with unused entries NULL. Foley remains a borrowed scene-owned catalog.
Per-catalog bounds are64 names and63 bytes per name; no runtime resource or
complete effect definition is loaded by this name-only owner.

Installed data contains52 emitters,56 glares and63 vclips. Retained ownership
uses3589 bytes including the36-byte owner; peak45199 includes the41610-byte
scratch allocation. Temporary scratch is released before success. Budgets are
checked before each allocation, and errors preserve a zero output owner.
Callers must keep archives stable across both passes and retire catalog borrowers
before closing; close frees only owned name storage and is repeatable.

verify_clutter_catalogs.py passes15 scenarios: full/exact/short peak budget,
both allocation failures, all six archive-read failure points, empty sections,
oversized name,65-name overflow and missing section end. PC reads actual VPP
files; compiled NXDK receives the same archive data through explicit read/find
boundaries. Full names/order, unused slots, retained pointers after scratch
retirement, peak accounting and cleanup match. Fixture hooks are restricted to
four resource boundary addresses, avoiding per-instruction Python callbacks.
Both builds and all19 CTests pass. Native XEMU residency, full effect/resource
loading, class archive loading and scene/factory integration remain open.
No new visual is claimed.


## Archive-backed clutter class loading (2026-09-12)

rf_clutter_classes_load composes the archive reader, factory-facing metadata parser, resource binding and compact class owner. It preserves all 431 authored declarations, including duplicate names, using one reusable parsed row rather than retaining the expanded definition array. The final classes own their names and emitter IDs; numeric resource IDs still require externally owned resources.

The shared two-pass owner accepts a read-only row callback and validates each row before copying. Archive text, 450 span slots and the reusable definition/binding occupy 169910 scratch bytes on the 32-bit targets. All 431 classes retain 55610 bytes including the owner; loader peak is 225520 bytes, excluding caller-owned catalogs, archive state and stack. This differs from the prior 429 first-match-selected class fixture because authored duplicate rows are now retained.

tools/verify_clutter_load.py passes 11 scenarios: every authored row and bound ID on PC actual archive/catalog loading versus compiled NXDK with supplied archive I/O and catalogs; exact and one-byte-short budgets; scratch/final allocation failures; archive read failure; binding failure in either pass; empty, malformed and capacity-overflow tables. Freed scratch is poisoned before retained fields are inspected. Outputs stay unchanged and partial allocations are released on errors. Existing compact class ownership verification and all 19 PC CTests pass; PC and NXDK builds succeed.

Evidence: artifacts/clutter-load.json. This is archive-to-runtime class metadata composition, not a complete original class parser, generic model/effect allocation, instantiated scene clutter or native XEMU validation. The next integration must retain the class owner for the lifetime of its objects and connect actual resource ownership and factory backends.


## Native scene-owned clutter inputs (2026-09-12)

Campaign startup now retains the archive-backed clutter class catalog and level clutter records after Foley initialization. The name catalogs borrow the existing Foley owner; shared cleanup closes records, classes and catalogs before Foley, including startup failure paths. The tables archive closes immediately after loading. This supplies the owned inputs needed by subsequent object factories; it does not instantiate clutter models, effects, physics or draw calls.

Native stock64MiB XEMU replay-20260912-135452 passes all 180 actor-pair fixture frames and matches the PC content digest. CLUTTER is [431,170,2,55610,39834,99033,229109,3216785036]: classes, placements, unmatched class references, class bytes, placement bytes, retained bytes, loading peak and content hash. Budgets exclude allocator overhead, archive state, stack and other scene owners. The 229109 peak includes the retained resource name catalogs during class loading; placement storage is loaded afterward. No expanded 431-row definition array is retained.

Independent archive inspection identifies both unmatched placements as Pole Light 1, UIDs7143 and9025 (raw enabled byte0); that class name is absent from clutter.tbl. No replacement class or synthetic object is inserted. The original class lookup/factory failure behavior must be preserved when actual creation is connected; the raw enabled byte alone is not used to infer original gating.

The replay checks class/placement content through a pointer-independent hash, counts and memory bounds against PC, plus existing actor/navigation/physics assertions. PC/NXDK builds and all19 CTests pass; the harness completed its build/flag restoration. Remaining work is actual generic object/model/effect ownership and creation-order integration into visibility/collision and AI.


## Original type4 generic allocator audit (2026-09-12)

tools/verify_clutter_base_original.py executes full original486da0 for type4, including real487100/411ac0 construction, registry generation/free-slot removal, global allocation-list append,49ec90/49f010 physics and48a160 room binding. The harness supplies heap, string assignment, parent lookup,489fe0 model attachment/model radius and503250 zero model-sphere count. This extends the earlier type7 corpse audit; it does not claim model resource loading is reconstructed.

All256 successful cases pass across supplied kinds1/3, radius -1/0/.5/2,0/1/2/4 explicit spheres, allocation flags0/4000/10000/100000, inherited parent fields and registry generation wrap. Two early failures (no registry slot and failed728-byte object allocation) leave registration/list state unchanged. The original reserves and publishes the object before model setup. Final flags include06400000;4000 also sets8000;10000 clears descriptor physics20 and suppresses sphere allocation. Negative radius copies the model radius into the descriptor; zero remains zero. Explicit spheres determine the resulting physics bound.

The successful heap traces contain the728-byte object plus one384-byte physics sphere allocation when physics20 is enabled; an initially empty sphere descriptor adds another384-byte allocation while preparing its default sphere. With10000 set, neither sphere allocation occurs. These are observed allocation requests, not proof of full lifetime cleanup or a proposed port memory layout. Model attachment is still supplied, and model failure rollback, real extracted model spheres and room search remain open.

Evidence: artifacts/clutter-base-original.json; original executable SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836. No production C change or new native XEMU claim in this audit. The shared generic owner must preserve this publication/radius/physics behavior while providing actual model ownership and rollback.


## Original type4 failed-model rollback (2026-09-12)

The clutter base harness now also executes real4867b0,48b390 and40e200 after supplied489fe0 reports model failure. All64 cases pass with zero/one remaining free slot, zero/one pre-existing global-list member and generation wrap. The only successful allocation is the728-byte object; its destructor frees it exactly once. Freed memory is poisoned. No physics sphere allocation or model sphere query occurs after failed attachment.

Rollback removes the newly published global-list node and decrements its count, destroys the object, clears the registry object pointer and appends the released slot to the FIFO free queue. It does not restore the consumed generation or generated object ID: generation advances (wrapping752e to1), and59f7e4 decrements even though the factory returns null. By contrast, the previously verified empty-registry/object-heap early failures never publish registration. Shared generic creation must distinguish these stages rather than treating every failure as an unchanged registry.

Evidence: tools/verify_clutter_base_original.py and artifacts/clutter-base-original.json (256 successful,2 early failure,64 model failure cases). Model attachment itself remains a supplied boundary; this is not model-loader implementation or native Xbox validation.


## Shared generic model attachment489fe0 (2026-09-12)

rf_object_model_attach in src/core/model.c reconstructs the full attachment orchestration using a16-byte model/index/radius/property view and explicit resource callbacks. Kind1 loads the full filename with1/-1; kind2 removes the last dot and suffix and loads with0/0; kind3 uses the full filename with9999999. Other kinds retain the incoming model. The original48a100 is a bare ret and therefore has no additional side effects to reproduce.

The model index becomes-1 even for a missing model. A nonzero model triggers the bounds query, radius expansion by the Euclidean length of its center, kind3 motion0 initialization at speed1, then503210 property lookup. A missing model preserves prior radius/property. Callback errors leave published partial state available for explicit caller cleanup; this routine does not own allocations or perform rollback. Port filenames are bounded to63 bytes; original489fe0 has a32-byte local buffer, so original execution fixtures stay within that bound. Non-finite supplied bounds are rejected by the port.

Verification: tools/verify_model_attach.py executes full original489fe0 with actual filename copy/strrchr,48a100 and40a000 norm; only model loads, bounds, animation and property services are supplied. All512 original/PC/compiled-NXDK cases match exact state and ordered callback arguments, including unsupported kinds and missing models. All other bytes of the supplied original object remain unchanged. Four callback failure stages and three port guards (unterminated filename, NaN center, infinite radius) pass PC/NXDK. Both builds and all19 CTests pass. Evidence: artifacts/model-attach.json. Resource loaders, model sphere extraction, generic owner composition and native scene integration remain open; no new XEMU claim.


## Shared model-derived creation spheres (2026-09-12)

rf_model_creation_spheres reconstructs the center/radius portion of486fc5..487040 after the caller has established an empty descriptor sphere list. Wrapper kind1 queries raw static centers; wrapper kind2 uses already evaluated bone matrices through rf_model_collision_sphere_pose. These wrapper kinds are distinct from489fe0 loader-kind arguments. Exactly one model sphere is recentered at the origin after querying it; multiple spheres retain queried centers. Zero model spheres produce no rows here, leaving later49ec90 fallback handling separate.

The original temporary record initializes/queries only center and radius in this block; its final two words are copied without a defined initialization here. Shared code therefore preserves caller parameter_10/opaque_14 values rather than inventing original defaults. Initial argument errors preserve all rows; a later query failure leaves earlier rows published and preserves the failing and subsequent rows. No allocation, class overrides, body creation or lifetime ownership occurs in this helper.

Verification: tools/verify_model_creation_spheres.py executes original486fc5..487040 with real503250/503270 static and cached animated paths, vector recentering and array growth/copy; only its heap allocation is supplied. All512 PC/NXDK cases match center/radius bytes for0..8 model spheres, including unparented and four cached bones. Auxiliary fields and spare output rows remain unchanged. Three error cases cover unsupported wrapper kind, short output capacity and a bad parent after an earlier valid row. PC/NXDK builds and all19 CTests pass. Evidence: artifacts/model-creation-spheres.json. This is explicitly a block-level proof, not full generic object ownership or new native XEMU validation.


## General creation physics owner (2026-09-12)

rf_physics_creation_body_open extracts the existing corpse body preparation into the shared physics module and adds general creation flag support, including clutter20 and disabled sphere setup0. rf_corpse_physics_seed is a layout-preserving alias of rf_physics_creation_seed; rf_corpse_body_open keeps its33/73 gate and delegates to the general helper. The seed still represents constructor-zero tensor/velocity with supplied response/mass, transform, radius and spheres. Geometric mass-grid input is excluded; clutter factory descriptors leave that pointer zero.

When flags70 is nonzero, the helper generates mass/tensor as needed, creates the empty-list fallback if needed and owns exactly the selected sphere records. Otherwise it ignores sphere/radius inputs and allocates no sphere storage. Budget includes the324-byte body and24 bytes per retained sphere. Errors preserve the empty owner; source storage is no longer borrowed after success. The fallback final opaque word remains explicitly port-initialized0 because original initialization was unspecified.

Verification: tools/verify_creation_body.py executes complete original49ec90/49f010 with supplied heap and material coefficients, comparing all308 represented body-state bytes and retained sphere bytes against PC/compiled NXDK. All640 cases pass across flags0/20/40/70/33/73/30/50, response0/10/2.5, generated/inherited mass and0..8 supplied spheres. All640 one-byte-short budgets and560 allocating-case heap failures preserve outputs; exact budgets, poisoned source independence, duplicate-open rejection and repeated close pass. Existing tools/verify_corpse_body.py still passes640 cases and its failure checks. Both builds and all19 CTests pass. Evidence: artifacts/creation-body-verification.json and artifacts/corpse-body-verification.json. Composed generic object ownership, model resources and native scene integration remain open.


## Concrete clutter base owner (2026-09-12)

rf_clutter_base_open composes heap-backed type4 storage, global object-list append, registry insertion and generated UID consumption, shared model attachment, model-derived sphere preparation and shared creation physics ownership. The532-byte owner retains the clutter state, separate global link, model attachment, physics body, material/parent fields, transform/query position and memory accounting. Resource callbacks receive a published handle; model identity is visible in the registered state before bounds/animation callbacks. Room and parent byte/group are resolved by the caller; missing-parent inputs are0/1.

The owner budget includes its532 bytes, retained24-byte physics spheres and temporary24-byte model-sphere records while copying into physics. Models and external registry/list storage require separate budgets. Model-derived auxiliary fields use explicit port-zero initialization because the original temporary words were unspecified. A missing loaded model returns OK/NULL after destruction and FIFO recycling, preserving consumed generation and UID. Service, scratch/body allocation and post-model budget failures clean partial owned resources while preserving the caller output. Loader errors must clean resources not transferred; successful returned models are released by the owner.

rf_clutter_base_close requires family-list/effect borrowers already retired. It removes the global link, closes physics, releases the model, frees the owner and then releases the saved handle slot. This is base-resource retirement, not a complete clutter effect destructor. Full4104a0 effects, persistent slots, family-list ownership, room search and rendering remain integration work.

Verification: tools/verify_clutter_base.py compares all owned bytes (normalizing pointers), retained spheres, callback publication observations and registry/list state across266 PC/compiled-NXDK cases. Its initial256 cases include240 successful owners; additional cases cover each resource-service failure, missing model, empty registry, short/exact budgets and all3 allocations. Source-independent owned storage and repeated close are checked with poisoned freed allocations. Independent expectations verify UID/generation consumption, list peak and FIFO order after cleanup.

For126 successful no-authored-sphere inputs, the verifier also executes full original486da0 with actual constructors, registry/list operations and physics; represented object fields, all308 physics-state bytes, model radius and fallback sphere bytes match. Original material index2 is supplied at its actual28-byte table stride. Model attachment remains a supplied boundary in this comparison; separate original-backed tests cover its orchestration and nonempty model sphere queries. Both PC/NXDK builds and all19 CTests pass. Evidence: artifacts/clutter-base.json. No new native XEMU/live scene claim.


## Static model resource bounds (2026-09-12)

The opening level resolves168 clutter placements to12 static model names; the remaining2 placements reference the absent Pole Light 1 class. Original500ff0 routes loader-kind1 through53ab40, which uses5142d0 with the literal .v3m extension. The class table uses authored .v3d names. Original wrapper-kind1 whole-model bounds (501610 with index-1) call53c36d, while animated wrapper-kind2 uses its existing first-submesh bound.

rf_model_static_bound_sphere reconstructs53c36d: sum each submesh center with float stores, multiply by the rounded reciprocal count, then take the maximum rounded center-distance plus submesh radius. rf_model_file_static_bound_sphere streams both passes over SUBM headers without a heap allocation or retained submesh array. The common bounded SUBM read is shared with the existing animated first-submesh API, whose behavior is unchanged. Empty/nonfinite/negative-radius input is rejected with output preserved.

Verification: tools/verify_model_static_bounds.py executes full original53c36d with actual submesh getters and vector arithmetic, no replaced callees. PC streamed headers are independently extracted using the structural model inspector; aggregation bytes match compiled NXDK and original for all427 installed V3M files,258 synthetic cases and3 port guards. There are42 multi-submesh static files, with a maximum of19 submeshes. Existing tools/verify_model_bound_sphere.py still passes all95 animated models. Both builds and all19 CTests pass. Evidence: artifacts/model-static-bounds.json and artifacts/model-bound-sphere.json. Full original file loading, static resource lifetime ownership and native Xbox archive validation remain separate; no new visual or XEMU claim.


## Owned static model metadata and compiled filenames (2026-09-12)

rf_static_model_metadata_open converts the authored name to.v3m with shared rf_model_compiled_filename, opens a temporary8792-byte streamed model directory, computes the whole-static-model bound and copies every CSPH record into owned48-byte views. The96-byte owner retains its compiled filename, bound, sphere pointer/count and memory totals. The archive and model directory are no longer borrowed after success; the directory is freed on all paths. One optional retained allocation holds only sphere records. Geometry, textures, shared runtime handles, reference sharing and VFX are explicitly excluded.

The skeletal filename helper now delegates to the same bounded conversion with.v3c. The converter preserves the original last-dot rule, supports overlapping authored/output storage, leaves output unchanged on guard failure and requires a disjoint four-character extension including its leading dot. Static loader53ab40 supplies the original literal.v3m at5a7ad8.

Verification: tools/verify_static_model_metadata.py checks all427 static files on PC actual archive reads against compiled NXDK composition with supplied previously verified directory/bounds/CSPH reads. It passes1286 cases: every file at generous/exact/one-byte-short budgets plus both allocations and directory/bound/sphere failure stages. Retained data survives released directory/archive storage; duplicate open, output preservation, poisoned frees and repeated close pass. The files contain229 CSPH records in total; maximum retained metadata is576 bytes and maximum loading peak9368 bytes, including owner/directory/spheres and excluding archive/stack.

tools/verify_static_model_filename.py matches full64-byte outputs against unchanged original5142d0 for1960 accepted cases, plus41 capacity guards, with compiled NXDK comparison. Existing skeletal conversion verification still passes1960 original cases and41 guards. Both builds and all19 CTests pass. Evidence: artifacts/static-model-metadata.json and artifacts/static-model-filename.json. This is metadata ownership for real archive inputs, not complete model resources or native Xbox archive validation.


## Archive-backed static clutter base adapter (2026-09-12)

rf_clutter_static_base_open connects the concrete generic base owner to owned static-model metadata from meshes.vpp. It supplies actual compiled filenames, whole-model bounds and CSPH rows, wrapper-kind1 sphere interpretation and original static503210 property-1. Model token is explicitly a pointer to owned rf_static_model_metadata, not a render-model wrapper. Loader-kind1 only is accepted; VFX requires a separate resource backend. Material coefficients, room and parent identity remain resolved caller inputs.

The model metadata load occurs after generic registration, matching established publication order. Missing archive entries produce a null object through the base rollback path; other loader errors clean metadata before returning. A combined budget checks base+metadata loading peak, then retained metadata plus model-sphere workspace and physical spheres before later allocations. Successful owner counters include both metadata and base storage. rf_clutter_static_base_close retires metadata through the base cleanup callback; archive storage is not borrowed after creation.

Verification: tools/verify_clutter_static_base.py compares actual PC archive-created owners against compiled NXDK full composition with only preverified file directory/bounds/CSPH readers supplied. All427 static models pass at generous budgets with physics20 and exact peak budgets with physics0, plus one-byte-short rejection. Additional cases cover all6 allocations, missing model, bound failure and sphere read failure. All owned bytes, retained metadata/spheres, registration generations/counts and cleanup results match across1290 cases; poisoned frees leave no live allocation. The maximum tested retained base+metadata+physics is1348 bytes and peak9900 bytes. Both builds and all19 CTests pass. Evidence: artifacts/clutter-static-base.json.

This is a real archive-backed base/physics adapter, not complete render resources or full4104a0 effects. Geometry, textures, shared render handles, family lists, persistent slot effects, room search and live scene creation still need integration. No native XEMU or new visual claim is made here.


## Static render geometry readiness audit

verify_model_vertices.py --static exercises the existing decoded rendering
owner against archive bytes for415 of427 installed V3M models:733 LODs,
1081 batches,55485 vertices and45110 triangles. Direct streamed decoding
and retained geometry agree on complete vertex/triangle/reuse hashes and
material mappings. Exact32-bit retained budgets succeed and one byte below
fails for each included LOD; maximum individual retained LOD is56624 bytes.
All12 static model names used by the resolved opening clutter placements
are in this supported set. This is PC archive/ownership evidence, not a
static draw or native XEMU verification.

The report explicitly lists12 excluded models containing23 alternate
110c21 batches, with submesh/local LOD/batch identifiers. Their complete
models are excluded, including otherwise supported batches; they are not
counted as passes. Alternate rendering semantics remain unresolved. Static
files can carry bone-link regions without a skeleton (2PartSwitch.v3m
starts with eight zero bytes), so the audit preserves those bytes without
applying animated bone-index validity checks. This does not establish their
render-time meaning or permit passing them through the skeletal draw path.
Nonfinite normals are preserved and reported. The unchanged animated mode
still passes all95 installed V3C models. No runtime decoder was changed.
Reports: artifacts/static-model-vertices-verification.json and
artifacts/static-model-link-audit.json. Next: recover static draw dispatch
and its transform/material interpretation, then bind shared geometry and
textures to concrete scene owners.


## Original static vertex projection

The existing52fa40 dispatcher routes a zero skeletal-pose pointer to
52de10 and a nonzero pointer to52e9e0. The static path reads position,
normal, UV and reuse streams directly; its projection is not identical
to the previously reconstructed skeletal path. No format acceptance rule
was changed from this inspection.

rf_model_project_static_vertex reconstructs52e1b5 through its visibility
branch. Rotation uses the original X/Z/Y accumulation order; the Z camera
subtraction remains extended while the X/Y differences are stored floats.
The vertical screen calculation consumes the stored reciprocal, unlike
the horizontal calculation. Broad arbitrary-float fixtures exposed the Z
precision difference after dyadic fixtures initially passed. The dedicated
API preserves these operations without changing animated projection. Its
view must already be prepared for model coordinates; object/submesh view
preparation, static lighting/reuse, batch submission and scene integration
are still separate work.

verify_model_static_projection.py executes the unchanged original block
and real5475d0/52fc70 callees with no replaced calls. All4000 dyadic and
arbitrary-float cases match all88 output bytes against PC and the compiled
NXDK function, including all five view gates, signed/zero depth, NaN/Inf
and1813 visibility rejections. Six NXDK null-argument cases preserve all
outputs. Existing2000 animated projection comparisons remain passing.
PC/NXDK builds and all19 CTests pass. This is original/compiled-code
verification, not a native XEMU or visual claim. Report:
artifacts/model-static-projection-verification.json.


## Complete static batch vertex processing

rf_model_geometry_render_static_batch executes the reconstructed static
52e11b..52e438 vertex loop over retained geometry with caller-owned buffers.
Fresh vertices use verified static projection and52fcf0 lighting on raw
authored normals, without skeletal normalization or bone-link interpretation.
Optional batch-local RGB rows suppress computed lighting. Without those rows,
the original lighting gate either updates fresh RGB caches or consumes their
existing contents. Duplicate vertices copy only the clip flag to their own
cache entry and emit from the referenced projected/depth/RGB cache, using
the current vertex UV and any current supplied RGB. Chained reuse therefore
retains the original cache behavior; it is not silently flattened.

World/second cache fields and unconsumed GPU bytes remain untouched, as in
the original static loop. This is not the skeletal loop world-cache contract;
static triangle facing/clipping/submission and prepared model-local view
ownership still need composition before scene use. No heap allocation,
archive read or alternate-format acceptance change is introduced.

verify_model_static_render_batch.py runs the unchanged complete original
vertex loop, including actual projection/light/reuse callees, against PC
and compiled NXDK:512 batches,4096 vertices,1185 positive reuse entries.
All772 status/cache/clip/second/GPU bytes match. Cases cover all five view
gates, lighting and supplied RGB combinations, negative/fresh/chained reuse,
arbitrary bone-link bytes that must be ignored, dyadic/arbitrary floats and
zero/NaN/Inf normals. One PC invalid-reuse and four NXDK range/capacity
guards preserve every output byte; NXDK inputs also remain intact.
PC/NXDK builds and all19 CTests pass; the existing250 skeletal-batch and
4000 static-projection comparisons remain passing. No native XEMU or visual
claim. Report: artifacts/model-static-render-batch.json.


## Static triangle routing and clipping inputs

rf_model_route_static_triangle reconstructs52e49a through the direct/clip
branches using the stored triangle plane and actual547960 facing semantics.
Perspective accepts plane(camera)>0; orthographic accepts when the plane
normal dot the view forward vector is not positive. Consequently unordered
perspective planes reject and unordered orthographic dots accept. Flag20
bypasses facing only; screen/common clip-mask rejection still runs first.
The skeletal route remains unchanged and continues deriving face normals
from transformed vertices. Stored static planes must be supplied by the
resource owner; they are not silently recomputed from decoded vertices.

rf_model_prepare_static_clip_triangle reconstructs52e560..52e62c using
shared validated signed-reuse mapping. Each corner reads the reused clip
position and its own mask/UV/ordinal. RGB comes from the reused cache unless
a batch-local color array supplies the current vertex color. Unrelated
record bytes remain untouched. This composes the already verified record
layout without assuming the skeletal constant-color policy.

verify_model_static_triangle_route.py compares4096 original/PC/NXDK cases:
2369 rejected,894 direct and833 clipping, including arbitrary stored planes,
zero planes and authored-pattern ffc00000 planes. Actual547960/vector
callees execute unchanged. verify_model_static_clip_inputs.py compares all
144 clipping-record bytes in2000 original/PC/NXDK cases (6000 records),
including supplied RGB and bounded negative reuse. Both suites check three
PC and three NXDK invalid-index/reuse guards with output preservation;
NXDK input bytes also stay intact. Existing3000 skeletal-route and2000
skeletal-clipping-input cases remain passing. Both builds and all19 CTests
pass. Reports: artifacts/model-static-triangle-route.json and
artifacts/model-static-clip-inputs.json. Static polygon generation, capacity
checks, submission, view ownership and native scene use are still open;
this is not an XEMU or new-visual claim.


## Composed static triangle clipping and emission

rf_model_geometry_emit_static_batch binds static stored-plane routing and
static clipping records to the shared polygon clipper and generated-vertex
fan emission. The existing skeletal emission API now forwards through the
same internal composition with its original route/record policy. Callers
supply batch-local stored face planes and optional RGBs, prepared caches,
clip/view state and bounded output/pool ownership. Static vertex preparation
remains a separate preceding API. No heap allocation or GPU API is added.
Port capacity/range errors retain the existing emission contract; they are
not a claim to reproduce the original silent capacity-skip policy.

verify_model_static_emission.py executes complete original52e43e..52e842,
including actual facing, clipping, projection/depth and fan callees, across
512 batches with441 generated vertices and1302 emitted indices. Compiled
NXDK matches all2860 output bytes for every batch and preserves input bytes.
Four NXDK missing-plane/index/vertex-capacity/index-capacity guards preserve
vertex/index arrays and counts. Fixtures supply prepared caches with
positive depths, side/far masks, perspective/orthographic gates, optional
RGB, stored-plane facing and u16 base wrap; near/custom planes, multiple
triangles, live archive binding and device submission need broader coverage.

PC matches408 complete records exactly. In the other104, generated float
coordinates differ from x87 output by at most0.000335693359375 screen pixels;
UV/depth scaled error is at most7.748601049685103e-7. Two generated RGB
channels differ by one8-bit step. Counts, indices, alpha/depth bytes and
all untouched bytes match. The report explicitly returns
PASS_WITH_NUMERIC_DIFFERENCES and enforces exact NXDK-original output;
PC float/RGB convergence remains an open item, not an exact-parity claim.
Both builds and all19 CTests pass; existing600 clipped-polygon emission
comparisons remain passing. Report: artifacts/model-static-emission.json.
No native XEMU or new-visual claim.


## Owned static render resources

rf_static_render_resource_open retains all static LOD geometry, stored
triangle planes,48-byte part views, LOD thresholds, raw84-byte material
rows and the whole-model bound. Geometry and its embedded owner are counted
once. The44-byte32-bit resource header and all arrays count against an
explicit budget; the caller-owned8792-byte file directory/archive, textures
and allocator metadata are excluded. No archive or file-directory pointer
is retained. Close releases every LOD/plane array, clears the owner and is
repeatable. Failure cleans partial ownership before publication. Existing
110c21 rendering rejection remains unchanged; animated LODs reject because
they lack the static stored-plane flag. This owns file data, not converted
runtime materials, shared cache handles, textures or device buffers.

verify_static_render_resource.py checks all415 supported static models/733
LODs against serialized part/material/threshold/plane hashes and the existing
independently archive-verified geometry probe. Every resource loads at its
exact retained budget and rejects one byte below with an empty owner. The
probe closes the archive before hashing retained data. Maximum individual
resource is133996 bytes. Twelve alternate-format models explicitly reject
with FORMAT; all95 animated models reject with NOT_FOUND.

verify_static_render_resource_nxdk.py runs the compiled file decoder and
resource composition for the opening12 unique clutter models, supplying
only VPP find/read and heap primitives. All retained hashes and accounting
match PC, with90904 bytes combined, excluding shared cache keys, textures
and caller inputs. The35 cases include exact/short budgets, all eight
first-model allocation failures and three read failures at early/middle/
final positions. Heap liveness and cleared owners are checked after repeat
close. Both builds and all19 CTests pass. Reports:
artifacts/static-render-resource.json and
artifacts/static-render-resource-nxdk.json. Scene cache/instance binding,
runtime material/texture conversion, native XEMU and visuals remain open.


## Native scene-owned static clutter resources

Campaign startup now retains shared static render resources and a record-to-
model slot array after clutter class/record loading. Compiled .v3m names
are deduplicated case-insensitively in placement order. Opening168 resolved
placements share12 resources; two unmatched Pole Light 1 records keep
UINT32_MAX slots. Nonstatic model kinds are counted separately for later
backends. This mapping does not implement original factory visibility,
enabled-record filtering, effects or render-object creation.

The scene allocates one model-slot capacity per authored clutter record,
then embeds only the unique owned resources. Retained accounting includes
all170 capacity slots, names and placement references, without double-
counting embedded44-byte resource headers. One8792-byte heap file directory
is reused across loads and freed before return. Retention is109416 bytes;
peak is118208, under a separate256KiB cap. Textures/runtime material
instances and caller archive state are excluded. Partial startup failures
and normal scene close release all populated resources and slot arrays.

Native180-frame actor-pair replay20260912-152601 passes on stock64MiB XEMU:
CLUTTER_RENDER=[12,168,2,0,109416,118208,23,3534861336]. The eight words are
unique models, bound records, unmatched records, nonstatic records, retained
bytes, load peak, serialized material rows and full retained-data hash.
The hash includes names, bounds, parts, material rows, thresholds, batch
ranges, vertices, indices/reuse, face planes and record slots. PC and native
values match exactly. Existing clutter metadata fingerprint and replay
gates also pass. Guest memory reports67108864 base bytes and0 plugged bytes.
Tested XBE SHA256:e90e40f5946cd9efbab306d49bfdc3aac9f9646ec946f910f72d5bba7df16293.
Both builds and all19 CTests pass after harness restoration. Report:
artifacts/xemu/replay-20260912-152601/report.json. Resources are resident but
not yet drawn; runtime texture/material conversion, view ownership and
GPU submission remain open. No new screenshot was requested or posted.


## Texture/material loading from retained records

rf_model_materials_open_records accepts retained serialized84-byte rows,
reusing the existing model material conversion, primary/secondary texture
deduplication, decoded-image ownership and budget/cleanup path. File-backed
and skin-substituted loading forward through the same internal implementation.
The records entry point copies its input to scratch and borrows no rows or
geometry directory after success. It uses base authored rows; original
clutter skin-switch/factory policies still need binding.

verify_material_records.py compares the retained-record and file-backed
paths for the opening12 static models plus baby_reeper.v3c, including all
200 runtime-record bytes, handle mappings, auxiliary-array sentinel and
resident/peak accounting. Exact peak budgets succeed; one byte below fails.
Source rows are poisoned/freed and archives closed before output inspection.
Three malformed-primary/secondary/missing-image cases clean partial bundles.
The combined23 opening rows deduplicate to11 textures at843404 retained
bytes and845704 peak bytes, including decoded pixels/runtime materials but
excluding caller-owned row arrays and archive state. This enables cross-
model sharing instead of separate per-model texture bundles.

Both builds and all19 CTests pass. Report: artifacts/material-records.json.
Verification here uses PC archive decoding; pixel bytes are not independently
hashed by this new probe. Scene binding, native XEMU texture ownership,
clutter material/skin policy and static drawing remain open.


## Native scene-owned clutter textures and materials

Campaign startup now concatenates retained static material rows into one
shared base-material bundle and retains per-model material offsets. The
caller row copy is freed before return; partial failures and scene cleanup
release the offsets and complete material/image bundle. The1MiB cap includes
retained offsets plus bundle ownership, and both the caller row copy and
loader scratch in peak accounting. Geometry resource ownership is separate.
Original clutter skin variants/factory effects and device texture submission
are not implemented by this base-material binding.

Stock64MiB XEMU actor-pair replay20260912-153543 passes180 frames with
CLUTTER_MATERIALS=[12,23,11,843456,847688,3830229234,837632,3791756628].
These are models, material rows, textures, retained bytes, loading peak,
runtime-material/offset hash, image storage bytes and logical RGBA pixel
hash. Every200-byte runtime record and owned auxiliary array is hashed.
Pixel hashing includes dimensions/source format and reads every pixel in
top-left order through rf_image_pixel, comparing Xbox swizzled storage
with PC row-major storage. All eight fields match PC. Existing static
geometry hash3534861336 and replay checks remain passing. Guest memory
is67108864 base bytes with0 plugged bytes. XBE SHA256:
a24b6a19eb3a9d3bd27f810fae650038425e0ede2ef42a4631fbb234b9fddd94.
Both builds and all19 CTests pass after harness restoration. Evidence:
artifacts/xemu/replay-20260912-153543/report.json. These materials/images
are resident scene resources; clutter instance view preparation, skin
selection and actual draw/device submission still need integration. No
new visual was produced or posted.

Static preview adapter (2026-09-12): rf_preview_static_model_emit shares diagnostic mesh conversion with the skeletal adapter while explicitly selecting stored-plane static facing and static clip records. Required planes are batch-local; optional RGB rows reach the recovered clipper. White tint, 640x480 screen quantization and diagnostic depth remain unchanged. No new allocation or archive I/O. New static_preview_clipping CTest renders a synthetic triangle through the real static vertex processor, leaves skeletal world caches poisoned, verifies direct emission, near-plane fan splitting, stored-plane rejection, prefix preservation, insufficient mesh capacity and null-plane rejection. The fixture disables screen_clip because that original gate rejects any clipped corner instead of clipping. All20 PC CTests and PC/NXDK builds pass. This is adapter evidence only: no live scene placement, original device submission, native XEMU validation or new visual claim. Next: bind shared static geometry/materials and authored poses into the campaign draw path, verify residency and output natively.

Campaign static prop draw integration (2026-09-12): scene_clutter_draw binds retained model slots and authored position/matrix to rf_model_local_view, renders each part first LOD, passes stored batch planes to rf_preview_static_model_emit, and maps base runtime material handles into the renderer image table. Texture pixel ownership transfers once; records remain with the clutter bundle. Shared444880-byte NPC scratch is reused sequentially, plus680 bytes of cached placement room IDs for170 records. Unknown-room props remain eligible. SUBM offsets are not added to vertices (52df09 forwards the instance pose). This remains diagnostic placement/rendering: full factory enabled/visibility/effect ownership, skins, distance LOD, dynamic lighting and original device-state parity are open.
Both builds and20 PC CTests pass. Stock64MiB native replay-20260912-154859 passes180 frames with CLUTTER_DRAW [180, 7, 1506, 155421154, 680, 8]; replay-20260912-155144 passes60 staged door frames with [60, 9, 1998, 81767933, 680, 10]. Both compare complete final prop vertex bytes through hashes with PC, preserve existing NPC/physics checks, and report all five x87 control words0x27f. Native free pages at completion:8441/8409; this is not worst-case campaign residency. Native framebuffer inspected for each run. README image docs/images/xbox-campaign-props.png is the unedited second native guest PNG, showing miner/robot plus newly visible lamps and warning fixture. No real-hardware or PS2 visual parity claim.

Post-conversion clutter material overrides (2026-09-12): rf_model_materials_open_records_overrides retains original base material conversion, loads/deduplicates optional per-row primary replacements, then replaces only runtime record+0x10 as410d30 does. Unlike the pre-conversion skeletal skin loader, replacement alpha does not redefine material flags. NULL row entries retain base handles; secondary handles and auxiliary arrays remain unchanged. Both base and replacement textures stay resident. Budget includes optional third name/mapping scratch lane; failure preserves the initially empty output and releases partial data. No glare side effects or per-placement skin owner yet.
verify_material_overrides.py passes23 opening raw rows across four patterns: all-null, existing names with case variants, partial replacements and one new shared glowball.tga replacement. Every200-byte record and array sentinel equals base except selected handle. Exact/one-byte-short budgets, missing textures, overlong names, source poisoning/freeing and repeated close pass. Existing13-model retained-row equivalence remains unchanged. PC/NXDK builds and20 PC CTests pass. This new loader was tested through actual PC archive loading; native Xbox override integration remains open.
Creation audit reminder: original465220 consumes and discards the authored enabled byte (full loader oracle evidence above), so no filter based on that byte was introduced. Current rendered records still need real class/factory ownership, original visibility gates, ordered associations and skin/glare application. Next: retain appearance records keyed by class/model/skin, build sparse primary override rows from verified clutter metadata and bind their per-placement indices in the scene.

Campaign authored clutter skins (2026-09-12): material appearances now key on case-insensitive compiled model/class/skin while render geometry stays shared. Each placement retains a4-byte appearance index; offsets address appearance rows. A temporary representative list, one table buffer and reusable assets row drive verified clutter metadata lookup. Unknown named variants retain base materials as410d30 does. Replacement count is limited to material count; untouched rows preserve base conversion. Table/parser output and representative scratch are freed before loading images. Combined measured accounting includes persistent offsets/placement slots, row/name scratch and the larger of metadata-stage/image-stage peaks. Parser stack and allocator overhead remain excluded. Glare changes, complete factory and live mutation are not implemented by this loader.
Independent verify_clutter_scene_skins.py reads actual VPP level/table/model bytes, derives first-match class/skin choices and appearance order, and matches PC selection hash1980895853:12 appearances,168 matched placements,67 named Yellowish placements,one replacement row and no missing named variants. The replacement is RunwayLight01_P01_Yellowish.tga. Two unmatched Pole Light1 records remain unbound.
PC/NXDK builds and20 CTests pass. Stock64MiB XEMU replay-20260912-160311 passes60 door frames with CLUTTER_MATERIALS[12,23,12,909700,915680,3535760234,903168,1074275182], CLUTTER_SKINS[12,168,67,1,0,1980895853] and CLUTTER_DRAW[60,9,1998,2338480573,680,10], matching PC including all material records, logical decoded pixel hash and final emitted vertices. Completion free pages8393, all five x87 control words0x27f. Native capture inspected: the fixture above the sign is now authored yellow instead of default red. README image replaced with that unedited native640x480 PNG. Original device-state fidelity, lighting/glare, real hardware and whole-campaign residency remain unproven.

Shared static resource collision metadata (2026-09-12): rf_static_render_resource now retains ordered CSPH records alongside geometry/materials. The x86 owner grows44 to52 bytes; each decoded sphere is48 bytes including authored name, parent, local center and radius. The existing validated sphere decoder supplies the rows; no centering, transform or physical body is performed at resource load. Allocation budget and cleanup include the new storage. Caller may close the archive after success. Scene resource hashes include sphere count and every sphere byte.
PC archive verification passes415 supported static models/733 LODs with223 spheres, exact/short budgets, archive-close retention and repeated cleanup. The12 alternate-render-format models remain explicitly unsupported;95 animated models remain rejected. Maximum resource134292 bytes. Compiled NXDK checks pass60 cases over opening12 models, including all allocations for both a zero-sphere runway light and the four-sphere portable light, nine injected reads including every portable-light CSPH read, and poisoned frees. Pure opening resource storage91192 bytes; the scene owner includes reserved placement/model slots and totals110968 retained/119760 loading peak bytes.
Both builds and20 CTests pass. Native stock64MiB replay-20260912-160912 passes60 door frames with CLUTTER_RENDER [12, 168, 2, 0, 110968, 119760, 23, 2126378738], matching PC full geometry/CSPH hash. Existing skin/material/draw checks pass. PC framebuffer bytes equal the prior skin build (SHA2569daba008f7883a9bd1e1145c11b3e333dd7d34ba73e24f45b7cbcd05095cc307); no capture requested. This establishes shared model input residency, not live clutter collision. Next bind this resource to generic base creation with ownership/refcounts, then full class factory effects and registered visibility/collision dispatch.

Shared static-model base ownership (2026-09-12): rf_clutter_shared_static_model is a stable12-byte x86 borrowed entry (compiled filename, resident resource, reference count). rf_clutter_shared_static_base_open supplies the existing verified generic type4 constructor with filename conversion/case-insensitive match, resident bounds, static property-1 and borrowed CSPH rows. One successful model attachment acquires one reference; every later constructor failure releases it through base cleanup. Missing names preserve original missing-model rollback/UID-generation consumption; reference overflow returns RF_RANGE. Close validates the expected shared entry, destroys only per-object physics/base storage and releases one reference. Caller must keep entry/name/resource stable and not release shared storage until references reaches zero. Body budgets exclude separately owned shared resources; no archive I/O or model allocation occurs in this adapter.
rf_clutter_shared_tests and verify_clutter_shared_bases.py pass415 actual static resources with830 archive-backed/shared base comparisons (physics0/20); identity pointers and resource accounting are normalized, all other532-byte owner state and all physics spheres match. Two concurrent borrowers have separate physical sphere storage. Exact/short budgets, missing-model rollback, reference overflow, surviving borrower registration, full cleanup and repeated close pass. verify_clutter_shared_nxdk.py executes linked code with only heap supplied, all arithmetic at x87 precision0x27f: zero/one/four spheres, both physics modes, two borrowers, budgets and all three allocations failing independently pass9 cases with poisoned frees and unchanged source spheres.
PC/NXDK builds and21 CTests pass. This is the concrete shared model-to-body adapter, not native campaign clutter ownership, original full4104a0 effects, family-list registration or live collision scheduling. No visual change or XEMU run claimed for this adapter. Next install campaign-owned shared entries and base owners with resolved materials/room/parent state, preserving global creation order and retirement before shared resources close.

Campaign clutter base owners (2026-09-12): campaign_clutter_bodies_open retains one pointer per authored record and stable shared static-model entries, constructs each resolved static prop through rf_clutter_shared_static_base_open and the campaign registry, then publishes authored UID. Descriptor uses first matching class, model/material/radius, class bits2/4 -> physics20, authored pose and identifier-1. Coefficients come from the existing parsed materials palette. Room is resolved by the existing collision-world locator; identifier-1 uses verified missing-parent defaults0/1. Original40f360 pushes string59405c (riot_shield),410b60 resolves it and40f36a stores5afb78; the corresponding class gets special allocation100000. No filter is inferred from the discarded enabled byte.
Rendering now consumes owned base position/matrix/room rather than separate record pose and cached room allocation; its extra room buffer is removed. Generic allocation-list and shared reference lifetimes are retained until cleanup before resource close. Failure teardown follows acquired owners; reference/close errors are reported and prevent shared render resource release. This is diagnostic campaign creation order and a clutter-local generated UID cursor before authored UID publication, not proof of original interleaving across all object families. Full4104a0 effects/health/class state, family list, persistent slots/associations, physics scheduling, collision participation and destruction effects remain open.
Independent verify_clutter_scene_bodies.py derives168 owners,149 physics-enabled placements,173 physical spheres and94372 retained/peak bytes from actual level/class flags/model CSPH records and verified constructor storage. PC replay agrees. Both builds and21 CTests pass. Stock64MiB XEMU replay-20260912-162229 passes180 actor-pair frames with CLUTTER_BODIES[168,149,173,94372,94372,3852490571,168,168,0,0], matching PC identity-independent creation/body/sphere hashes and complete retirement; all five x87 control words0x27f, free pages8408 at completion. Existing NPC/physics/material/skin/draw telemetry passes with new registry handles. A separate60-frame PC hall replay is byte-identical to the prior skin capture; no new screenshot taken.

Registered clutter geometry queries (2026-09-12): each shared static scene model now owns collision parts/LODs through rf_model_collision_resource_open alongside render geometry, within the existing256KiB combined cap. Resources close after registered base owners retire. The public registered-handle query validates exact body identity and shared attachment, then routes kind1 through reconstructed5031f0 to54e000 using caller-provided pose/query scratch. No per-query allocation. Startup diagnostics issue six world-axis rays per authored owner without changing physical state; this does not implement live pair scheduling, visibility family lists or full class factory effects.
verify_clutter_scene_collision.py independently decodes archive part/LOD choices, raw collision hashes and byte accounting, and generates placement rays from original-verified static bounds. Archive-backed probe output matches scene query scratch/hit/result hashes; this shares the already verified low-level trace and is not a new original-runtime oracle. PC and stock64MiB XEMU replay-20260912-163400 pass180 actor-pair frames with collision summary[12,12,18,89316,1008,988,3527882789,2476729641,0]. Combined render/collision retained203444 and peak212236 bytes; registered bodies remain94372 bytes with complete retirement. All21 CTests pass, both builds succeed, all five native x87 controls are0x27f. PC frame SHA256 remains78a6164c91c32fda8b15278b698488785a96b72d36131ee9c07b0215b7651d38, identical to preceding body replay. No new screenshot. Next: bind these registered geometry services and complete class state to ordered visibility/contact callers and live scheduling.

Opening factory service census (2026-09-12): inspect_clutter_factory_requirements.py groups authored L1S1 records in insertion order and reads actual factory-facing definitions using the shared metadata reader.168 placements resolve,2 Pole Light1 placements do not.149 resolved placements have classflags2 (collide_weapon), none classflags4 (collide_object). Original-reconstructed4104a0 publishes objectflag0x40000 for classflag2; reconstructed48be00 rejects such objects against non-kind2 partners. Do not connect every retained physical body as a player obstacle. Declarations require glare for149 placements and explosion resources for158; zero startup sound, emitter, rod, screen, light-tag or corpse declarations. These are pre-resolution declarations, not observed effect/callback counts; glare model tags and skin replacements still determine actual instances. Audit changes next integration order: bind factory resource/effect services and class state, then family eligibility/ordered collision scheduling. The existing actor-only pair adapter must not accept raw clutter state as an entity view.

Static factory tag lookup (2026-09-12): original503220 dispatches via501220 kind1 to53c23f; this reads the first submesh attachment array and calls5829a0 with query strlen. It is ASCII-insensitive prefix matching, unlike character51d5b0 exact-name matching. Shared rf_model_find_static_tag implements first prefix match with bounded names and unchanged output on absence/errors; null empty query is absent, nonnull empty query matches first entry. Original guard for no submesh remains a caller concern (empty supplied list). verify_static_tags.py executes unmodified503220/501220/53c23f and original comparator in default locale with supplied model arrays, comparing516 cases to PC and actual compiled NXDK code. Cases cover duplicates, prefixes, missing/longer queries, empty arrays/queries and non-ASCII names. Both builds and21 CTests pass. No native XEMU, attachment loader ownership, tag pose or glare creation claim. Next retain first-submesh attachment names/poses in shared static resources and bind factory lookup through registered model ownership.

Owned static attachment records (2026-09-12): rf_static_model_tags owns decoded first-submesh first-LOD attachment names, quaternion/position and parent index. It uses the existing bounded file attachment reader, retains no archive/directory pointers and provides prefix lookup through verified rf_model_find_static_tag. One allocation, budget includes owner plus104-byte records; partial reads free without publication, close zeros ownership and supports repeat calls. This separate owner is ready to be included in shared scene model accounting; it is not yet instantiated by campaign loading. verify_static_tag_owner.py passes all427 installed V3M models with242 records, matching serialized record bytes and first-prefix indices after archive close/directory poisoning, exact/one-byte-short budgets and repeat close. Maximum owner3340 bytes. PC/NXDK builds and21 CTests pass. Compiled NXDK allocation/read failure injection, native scene ownership, static parent transform evaluation and glare creation remain open. No new visual output.

Shared campaign tag residency (2026-09-12): verify_static_tag_owner_nxdk.py passes1173 cases over all427 installed static models using actual compiled decoder/owner, only supplying VPP/heap. Exact/short budgets, every allocation and every attachment-read failure leave output unchanged and no retained allocations. Scene shared models now own/close rf_static_model_tags within the existing256KiB combined model budget; body retirement still precedes resource close. Independent verify_clutter_scene_tags.py derives opening12 owners/seven attachment records/872 owner bytes/hash4162697305 directly from serialized first-submesh first-LOD data. Model-slot capacity and tag records increase combined retained to206212 and peak215004 bytes. PC and stock64MiB XEMU replay-20260912-164711 pass180 actor-pair frames with matching telemetry and existing body/collision/material checks; all five native x87 controls0x27f. All21 CTests pass, both builds succeed. PC image remains byte-identical. No factory glare instance, parent tag evaluation or additional rendering is claimed. Next connect retained lookup/pose services to original factory glare requests.

Static attachment world placement (2026-09-12): rf_static_model_tag_place composes retained attachment quaternion/position with existing world placement math. Disassembly503230 ->5012a0 kind1 ->53c1c4 establishes first-submesh attachment index validation, direct position copy and5194c0 quaternion conversion; static parent index is ignored, with no normalization. Existing rf_model_attachment_transform plus rf_model_place_tag matches this complete valid static5034f0 path exactly. verify_static_tag_placement.py executes unhooked original wrapper/query/quaternion/world helpers against512 shared PC/compiled NXDK poses at x87CW0x27f, including zero/nonnormalized quaternions and arbitrary matrices; parent123 does not affect results. Five NXDK index/nonfinite-input guards preserve output. Invalid original wrapper indices are not claimed to have the same safe error contract (original query can return without initializing wrapper scratch). Both builds and21 CTests pass. This is an owned-data service; registered scene dispatch and glare creation remain open.

Registered prop tag dispatch (2026-09-12): rf_scene_clutter_tag_find/place resolves the campaign registry entry to an exact owned clutter state/handle, validates shared model attachment and selects its retained tag owner. Placement uses current owned matrix/position. Errors preserve caller output. Startup diagnostic checks eight factory tag names plus every stored tag name per retained prop; it creates no glare instances. verify_clutter_scene_tag_queries.py builds original static wrapper data from serialized model attachments and authored placement matrices/positions and executes actual503220 lookup and5034f0 placement. PC agrees for1541 lookups,394 matches/poses,hash551080237,zeroerrors. Stock64MiB XEMU replay-20260912-165410 passes180 actor-pair frames with the same summary and unchanged combined model budget206212/215004 bytes; existing collision/body/material gates pass. Both builds and21 CTests pass; PC frame hash unchanged. Lookup/placement borrowers still need binding into full factory glare creation, ownership and rendering.

Parented glare constructor evidence (2026-09-12): verify_glare_create_original.py executes full413d20 with original descriptor/vector initialization/destruction and supplied random-radius40a4a0, parent40a0e0, tag5034f0 and generic allocation486da0 boundaries.60 cases verify signed class bounds, full untouched/modified768-byte object footprint, ordered calls, low-byte flag handling, allocation failure and ordered glare-list insertion. Class table5c9e98 has52-byte stride, count5cab98; radius bounds atclass+28/+2c. Successful generic allocation is type10, identifier-1,parenthandle,allocationflags30000,final0. Descriptor has unit+14,zero+4,radius+84 and queried world pose+3c/+48. Parent/tag stored at200/204; active28c=1; timer290=-1; six words294..2a8=0; classpointer/index2ac/2b0; flags2b4=2 only for nonzero low byte; next2b8 points sentinel5c9ba8,previous2bc oldtail5c9e64,oldtail.next and globaltail update; lastposition2c0 is(-1000,-1000,-1000);2cc=0,byte2d0=0,two vectors2d4/2e0 zero. Byte28d and padding remain allocator-owned. Invalid class has no service calls; failed allocation leaves list/object untouched, but radius and tag calls already occurred. This establishes constructor orchestration and write scope, not random distribution, actual generic type10 ownership, shared C equivalence, destruction or rendering. Next implement shared constructor with these real service boundaries and reconstruct type10 cleanup.

Shared parented glare constructor (2026-09-12): rf_glare_create composes sampled class radius, parent/tag world pose and generic allocation backend, then initializes compact104-byte glare state and appends in factory order. Class view borrows definition and radius range; descriptor carries parent/radius/world pose with fixed type10 allocator requirements documented. No heap allocation in constructor. Out-of-range class returns NULL without services; null allocation does not insert; service errors stop without publishing output. Active/timer/samples/class/parent/tag/low-byte flag/lastposition/vector fields preserve the audited footprint and reserved allocator bytes. Generic allocation, RNG implementation, resource ownership and destruction remain backend obligations, not stubbed successful services. verify_glare_create_shared.py compares PC/actual compiled NXDK against60 full original constructor cases using normalized compact state and ordered service evidence, plus18 radius/pose/allocation failure cases preserving output/list. Existing original oracle now exports canonical records for comparison. Both builds and21 CTests pass. Next implement actual type10 ownership and glare retirement, then bind registered prop pose and real sampled radius/class resources to campaign factory.

Glare generic ownership evidence (2026-09-12): verify_glare_base_original.py executes486da0 type10 with actual constructors, registry and physics setup; supplied heap/string/parent lookup, no model.32 cases allocate exactly748 bytes without sphere allocations, publish type10/parenthandle and generation handle, propagate parent byte/group, use no-model radius fallback1 for<=0, and retain objectflags6030000 for allocationflags30000. Unlike ordinary clutter, the20000 allocation bit suppresses ordinary room-binding flag400000; do not reuse the clutter owner unchanged. Captured base bytes remain in ignored artifact for future owner comparison. Original4153b0 only unlinks glare next/previous at2b8/2bc and clears those fields; other object bytes stay unchanged. Original4867b0 then removes generic linkage, frees and recycles slot. Four sentinel/interior neighbor arrangements and existing/empty generic lists are covered. This does not execute complete486670 destruction dispatch, implement shared type10 owner or prove live effect lifetime. Next implement distinct compact glare base ownership using recovered flags/parent/body contracts and existing registry/list services.

Concrete glare base owner (2026-09-12): rf_glare_base_open allocates528 bytes, publishes a registered glare state and generic object link, consumes UID, prepares the no-sphere physics body, and retains type10 flags6030000, parent handle/byte/group, identifier-1, health100, authored pose and no-model radius fallback. Seed word14 explicitly preserves descriptor mass1; negative descriptor radius resolves to fallback while zero remains zero for physics input. No ordinary room binding or model resource is introduced. Close rejects a still-linked glare family node, then releases generic link/body/heap and recycles registry slot; repeats and stale-handle removal pass. verify_glare_base.py matches32 original body-state and defined generic-field cases after identity normalization, plus exact/short budget and empty registry. Original oracle material0 coefficient setup corrected to actual fallback table649f50, ensuring nonzero material comparison. Both builds and21 CTests pass. Compiled NXDK heap/read failure injection, full factory adapter, RNG/class resources and live glare rendering remain open.

Glare base compiled verification and owned factory adapter (2026-09-12): verify_glare_base_nxdk.py executes actual compiled owner/physics/registry/list routines with only calloc/free supplied.35 cases match PC complete normalized owner bytes and lifecycle:32 original-boundary fixtures, one-byte-short budget, empty registry and heap failure. Linked-family close rejects without release; unlink/close/repeated close retire handles and all allocations. rf_glare_owned_open now binds shared413d20 orchestration to real528-byte base allocation through a stack context, leaving radius RNG and registered tag pose as explicit external services. rf_glare_owned_close validates family membership before unlink and base retirement. PC --glare-owned fixture creates three owners, verifies parent/class/flags/mass and callback order, rejects wrong-family close, handles radius/tag/budget failures without altering live lists and retires middle/head/tail with stale handles absent. Both builds and21 CTests pass. Composed adapter still needs compiled NXDK failure/lifetime checks; campaign class resource/RNG binding and native rendered glare remain open.

Correction and full owned adapter verification (2026-09-12): direct disassembly establishes40a4a0 is maximum(first,second), not a random sampler. Earlier supplied-radius boundaries proved constructor ordering only and did not establish that service; prior radius/RNG descriptions are superseded. rf_glare_create now computes the larger finite class size directly (second operand on equality); class fields renamed size_first/size_second and the false RNG callback removed from factory/services. Original constructor oracle now executes unhooked40a4a0, with first<second,first>second,equal class cases; all60 cases match PC/compiled NXDK and12 pose/allocation callback failures pass. verify_glare_owned_nxdk.py verifies full actual compiled adapter with only pose/heap supplied: three live objects, insertion, wrong-list close rejection, pose/heap/short-budget failures, invalid classes, empty registry, middle/head/tail retirement and repeat close;14 checks,zero leaks. Both builds and21 CTests pass; PC composed lifecycle passes. Glare construction consumes no RNG draw at40a4a0. Next bind actual class size/resource data and registered prop poses to campaign creation, then rendering.

Glare authored metadata (2026-09-12): rf_glare_definition_read reads the first exact name inside#Glares into owned300-byte metadata: class/corona/volumetric/reflection names, RGB, cone degrees, intensity, radius distance/scale factors, diminish distance, volumetric height/length and optional-field presence. Does not load bitmaps or convert degrees. Disassembly413920 establishes class+28/+2c are Volumetric Height/Length;413d20 takes their maximum. Optional missing fields zero in this fresh metadata owner; no claim of equivalent behavior when reusing original nonzero class storage. Bounded63-byte names, finite numeric parser, required color and conditional corona/volume numeric fields; errors preserve output. verify_glare_definition.py passes all56 installed definitions plus9 absence/malformed/duplicate cases on PC and actual compiled NXDK versus independent table extraction. Both builds and21 CTests pass. Full original parser oracle, runtime class ownership, bitmap handles/pixels, degree conversion and live factory binding remain open.

### Owned glare class definitions

rf_glare_classes_open retains effects.tbl #Glares rows in authored order,
including duplicate names, in one definitions/views allocation. Each factory
view borrows its corresponding definition and authored volumetric height/length.
The transient table is released before publication; all failures preserve the
empty destination and release partial allocations. The authored 64-row limit
is enforced. Bitmap names remain metadata; renderer IDs/residency and live
campaign glare creation remain open.

verify_glare_classes.py passes13 checks on actual compiled NXDK code with only
archive I/O and heap boundaries supplied: all56 authored rows, exact/short
budget, duplicate ordinal mapping, empty/64/65-row tables, malformed later
rows, both allocation failures, read failure and repeated retirement. PC
compares all authored owner bytes after archive close. Retained17492 bytes,
peak39157 bytes. verify_glare_definition.py independently rechecks all56
authored definitions plus9 guards on PC/NXDK. Both builds and21 CTests pass.
No native XEMU or bitmap/rendering integration claim for this change.

### Glare bitmap decoder and residency audit

Original413920 calls50f6a0(name,-1,1) at4139c1,413a73 and413add
for corona, volumetric and reflection slots respectively; absent blocks store
zero at class offsets0c/24/30. The50f6a0 wrapper preserves the global bitmap
mode for the supplied nonzero third argument and calls50f6e0. That function
first calls50f580 and returns an existing nonnegative result before loading.
This disassembly establishes a shared bitmap-loading boundary, not full cache
or filename/format resolution equivalence. Original cache retirement and
animated glare frame selection still require reconstruction.

inspect_glare_bitmaps.py reads all56 owned authored definitions and resolves
all38 unique bitmap names against every installed archive. No missing names,
duplicate-archive matches or decoder failures. Existing PC all-frame ownership
passes176 checks including every frame, exact/short budgets and hashes against
the direct decoder. This is a decoder-composition check, not an independent
image decoder oracle. Archives close before the probe hashes retained pixels.

Two textures require animation: thruster02_cor.vbm has5 frames at15 fps and
retains82040 bytes; thruster02_vol.vbm has21 frames at15 fps and retains344504
bytes. Across all38 textures the existing animation owners retain1771472
bytes, excluding glare binding arrays, allocator metadata and GPU overhead.
Do not bind glare resources through the static-only material path. Reuse the
all-frame decoder with bounded shared ownership, then reconstruct frame
selection and bind campaign resources. No Xbox residency or live rendering
claim is made by this PC audit. Full results: artifacts/glare-bitmaps/report.json.

### Owned glare materials

rf_glare_materials_open maps each class to corona/volumetric/reflection slots,
using UINT32_MAX for absent blocks. All authored bitmap names are owned and
ASCII-insensitively deduplicated; first matching caller archive wins, including
errors. The existing particle animation decoder retains every base-mip frame.
This is port resource ownership, not original bitmap-handle/cache equivalence.
No animation clock, renderer binding or campaign glare creation is added yet.

One zeroed slot allocation reserves count*12 binding bytes and count*3*84
texture bytes plus24-byte owner on x86. Decoder budgets credit the embedded
20-byte animation descriptor once. Partial failure closes every decoded
animation and frees slots without publishing output. Inputs cap at64 classes.
The loader rejects occupied destinations, malformed present filenames and
unknown bitmap presence bits. Definitions and archives may close on success.

verify_glare_materials.py passes all56 class mappings,38 unique textures and
62 decoded frames against separately loaded texture hashes after definition
and archive retirement. PC retained1785520 bytes; exact budget passes, one
byte less and missing archive fail cleanly. verify_glare_materials_nxdk.py
passes48 actual-compiled binding/lifetime cases, including every one of38
texture-load failures, slot allocation failure, mixed-case deduplication,
malformed inputs, empty/64/65 classes and repeated retirement. That harness
supplies animation-loader/heap boundaries with audited PC sizes and accounts
for compiler-inlined cleanup; it does not verify Xbox image residency or
swizzling. Both builds and21 CTests pass. Native Xbox resource residency and
campaign binding remain required before claiming integrated glare support.

### Native campaign glare resource residency

Campaign startup now owns all56 authored glare classes and38 shared animated
textures/62 frames, under a combined2MiB budget. Class parsing releases its
table scratch before image loading; retained and peak1803012 bytes. Shutdown
closes materials then class definitions. No glare instances or drawing yet;
future instance teardown must precede these borrowed resources.

rf_scene_glare_resources records class/texture/frame counts, retained/peak
bytes, complete definition hash, binding/name/frame-header hash, decoded
pixel bytes and pixel hash. Image hashes walk rf_image_pixel so Xbox swizzled
storage compares in logical pixel order. verify_glare_scene_resources.py
derives the reference from standalone owners and direct archive decodes.

PC and native stock64MiB XEMU replay20260912-173935 pass180 actor-pair frames:
[56,38,62,1803012,1803012,838438285,822613702,1769472,2493138017].
Guest memory base67108864, plugged0; available pages7941 at completion.
All five sampled x87 control words remain0x27f. Existing scene replay checks
pass and21 CTests pass; harness restored its flags and completed its final
build. This verifies native decode/swizzled access and co-residency, not
visual glare fidelity, frame selection or actual-hardware behavior.

### Reusable clutter corona/glare factory stage

rf_clutter_create_glares extracts the existing4104a0 stage without duplicating
its control flow. Full rf_clutter_create now invokes it in the original place:
after emitter/timer initialization, before rods/lights/screens/explosions.
For glare!=-1 and class flag0x400 clear, query corona_1 onward until the first
negative tag, retaining at most four indices while still scanning to the
first miss. Set0x400 only after successful completion. Issue GLARE for every
cached tag, including existing caches when glare==-1; the glare constructor
handles invalid class indices. TAG requests use the parent model, GLARE uses
parent handle/tag/class/flag0. Failures preserve prior cache/effect progress.
No allocate callback is needed by this stage and no rollback is invented.

verify_clutter_factory.py --shared still matches full original4104a0 across
512 cases on PC and compiled NXDK, plus10 NXDK errors. The new direct
verify_clutter_glare_stage.py passes251 compiled cases for zero/multiple tags,
four-entry saturation, cached class reuse, glare-1 requests and failure at
every TAG/GLARE callback, checking exact class/state write footprints. Both
builds and21 CTests pass. Scene instance creation remains to bind, including
skin glare reassignment and retirement before parents and shared resources.

### Authored skin glare metadata

The existing rf_clutter_assets_read exposes model/replacement names only; it
did not retain +Glare. rf_clutter_skin_assets_read now extends the same
internal class/skin parser with a64-byte owned override name and presence
flag. It reads the optional +Glare immediately following the skin replacement
list, keeps first case-insensitive class/skin selection, and commits all
outputs only on success. An absent override publishes empty name/present0;
an explicitly empty override publishes empty name/present1. Existing public
entity/clutter texture-reader behavior and struct layout remain unchanged.
Name-to-class resolution and applying the override to created glares remain
separate from metadata reading.

verify_clutter_skin_glare.py passes504 cases on PC and actual compiled NXDK
with only mapped-stack growth supplied:429 selectable classes,494 authored
base/skin selections and10 synthetic cases. Independent table extraction
compares every model, texture and glare-name byte, including16 authored
+Glare overrides, first duplicate selection and output preservation on errors.
The existing verify_clutter_assets.py also passes494 authored selections,
6 guards and430 original lookup cases. Both builds and21 CTests pass.

RunwayLight01_Larger/Yellowish selects WarmWedge01_NoVolume; its Blue skin
selects CoolWedge01_NoVolume_Small. The existing opening appearance audit
contains67 Yellowish placements. Preserve these after base glare creation
when binding live instances. No native instance or rendered override claim.

### Native authored prop glare instances

Campaign now invokes rf_clutter_create_glares for every retained prop after
registered parent/NPC storage exists. TAG resolves the registered parent model;
GLARE composes rf_glare_owned_open with real registry/global-object/family
lists, parent byte/group, material0 coefficients and registered tag world
pose. At most four slots per authored prop are reserved. Successful instances
retain actual type10 bodies and shared class pointers. This remains diagnostic
creation order, not proof of full original global object load scheduling.

Skin overrides are resolved once per appearance from the shared metadata
reader and exact glare-name catalog. The table and metadata scratch are
released before instance allocation. After each parent creates its glares,
4153e0/48ac00 semantics change only matching child class index/definition;
radius and initial pose remain as created from the base class. Missing skin
or unresolved override leaves the base glare unchanged. Full prop class
effects/flags/family/slot creation beyond this glare stage remain open.

Shutdown retires every glare family/global-list link and registry handle
before releasing shared resources and parent props. Partial failures release
created owners. The scene checks registry identity at creation and stale
handle rejection at retirement. No update, visibility, frame clock or draw
submission for these glares yet.

verify_clutter_scene_glares.py derives counts/cache selections/skin classes
from authored records and verified metadata and executes original503220 and
5034f0 on raw static attachments. Every instance UID/tag/class/flags/radius/
world-pose hash matches the PC scene. Native stock64MiB XEMU
replay20260912-175811 passes180 actor-pair frames with identical telemetry:
[168,9,157,157,67,85644,171598,1729083959,157,0].
Fields: parents,tag queries,requests,created,skin changes,retained,peak,hash,
retired,errors. Peak includes transient skin metadata; retained85644 bytes
includes slots/list/counters and157*528-byte owners. Native available pages
7908, memory base67108864/plugged0; five control words0x27f. Existing replay
checks and21 CTests pass; harness restored flags and completed its final
build. No new screenshot because glare instances are not rendered yet.

### Glare draw-pass orchestration

Original4154a0 only marks base flag2 if a nonnegative parent handle no longer
resolves; it does not itself update attachment pose. Original4141a0 is a
volumetric draw routine. Original4154f0 is the corona/reflection pass: first
matching view in7c75e4/count7c7634 versus current7c763c, render-state enable
431950(1), traverse family list5c9e60, skip hidden base flag1, and dispatch
414860(owner,view) only when glare flag0x80000000 is set. Otherwise clear
that view at29c/2a4. Optional low-byte reflection argument calls4155a0, then
consume0x80000000. Finally431950(0). Volume pass and marker production are
separate from this routine and remain to bind.

rf_glare_render_pass implements that ordered traversal over concrete owners
and the two represented per-view sample slots. Required renderer callbacks
must not change list ownership. Port validation rejects malformed lists and
more than two views. After successful enable, callback errors disable the
backend and preserve prior progress; initial enable failure returns directly.
A failed draw callback leaves its owner marker unconsumed. Renderer callbacks
are not bound to campaign yet; no dummy draws or visibility substitutions.

verify_glare_render_pass.py passes197 cases:192 full original4154f0 versus
PC and actual compiled NXDK, four callback-error paths and the view-count
bound. It compares ordered callbacks and state footprints across hidden/base
flags, duplicate/missing/current views, reflection low-byte values, sample
clearing and marker consumption. Only original enable/corona/reflection
implementations are supplied; full dispatcher executes. Both builds and
21 CTests pass. No native geometry or integrated rendering claim.

### Glare visibility marker and volume queue collection

Original488467..4884fa traverses the glare family. For each owner,40a490
room must match the current room token (EBP) and active byte28c must be
nonzero. Positive class bitmap24 queues callback488b00 with sorted1;
nonpositive bitmap24 supplies callback0/sorted0. Both use object handle2c,
position3c, radius78, plane/min/max0, lighting0 and lighting_flag1.
Accepted4d3560 culling ORs glare marker0x80000000. Rejected/skipped owners
keep any existing marker; the later4154f0 pass consumes it. No hidden-base
flag filter is added here. Null-callback culling accepts without a queue slot,
even at capacity; positive-volume submission rejects a full queue.

rf_glare_collect composes these rules for one resolved owner with the
existing rf_render_queue_append and frustum. Room lookup, list traversal and
resolved world/instance cull position remain caller inputs. Queue callback
is an opaque identifier and must be nonzero for positive volume. No GPU
submission, live collector binding or synthetic visibility bypass is added.

verify_glare_collect.py executes the entire original glare-list block with
actual4d3560 and5186a0; only40a490 room resolution is supplied.336 cases
compare PC/compiled NXDK acceptance, marker, all2048 queue slot bytes and
owner footprints. Includes11 appends,37 accepted-without-append and207
room/inactive skips, signed radii, tangency/outside planes, full queue and
preexisting markers. The world offset is zero/no instance transform for
this oracle. The original collector does not draw callback geometry here.
Both builds,197 draw-pass checks and21 CTests pass. Next bind collection to
live room scheduling and recover414860 corona geometry/occlusion,4141a0
volume drawing and4155a0 reflection behavior before visible integration.


Glare candidate occlusion415280 (2026-09-12)
------------------------------------------
rf_glare_occluder_test composes actual shared segment-box and model query
interfaces. Original415280 requires candidate flag0x10 and model80, skips
the object pointer stored at5cb054 and the glare parent handle30, then
tests candidate AABB190/19c against glare3c -> camera using508b70. On a
box hit it constructs a model query with candidate position3c/basis48,
camera start, glare-minus-camera displacement, radius0, flags1 and reset1.
The5031f0 return low byte determines the normalized blocking result.
The excluded object's identity remains a caller input, not a guessed role.

Original414e00 first resolves glare290 as an object handle through40a0e0
and retests it with415280; a stale/nonblocking cache is reset to -1. The
shared state field previously called timer is therefore renamed occluder
without changing layout or constructor bytes. Full414e00 traversal/cache
publication is still unreconstructed. Original28d caches visibility, and
414860 refreshes on alternating handle/frame parity; neither is live yet.

verify_glare_occluder.py passes420 PC/compiled NXDK cases:408 execute full
original415280 with actual box/vector/query constructors and only5031f0
supplied, plus12 callback-error cases. Checks all80 query input bytes,
filters, tangency/misses/degenerate segments, low-byte return semantics and
unchanged candidate/glare owners;157 model calls total. Port errors preserve
blocked. This does not prove model collision or complete world visibility
by itself. Both builds,72 constructor checks,197 pass checks,336 collector
checks and21 CTests pass. No native replay or new visual is claimed here.


Full glare visibility search414e00 audit (2026-09-12)
--------------------------------------------------
verify_glare_search_original.py executes full414e00 with actual AABB,
vector/matrix helpers and constructors. Supplied services are40a0e0 lookup,
415280 candidate test (separately verified),40a490 room,4290d0 state,
426fc0 association and4df1c0 geometry results.2048 cases cover empty/one/two
item lists, cached/stale actors and solids, world/solid hits, signed result
counts, same/different rooms, low-byte state and blocking values, associated
actor exclusion, parent fallback and owner footprints. This is an original
orchestration audit, not a shared implementation or native gameplay test.

Search order and state:
- Original290 is the cached actor handle. Resolve/retest first; retain on
  block, reset to -1 on stale/miss before proceeding.
- Original294 is a cached moving-solid object pointer. Require flag0x10
  and glare-to-camera AABB hit, transform both endpoints into its local
  coordinates using position3c/basis48, query its solid294 with reset1.
  Positive signed count blocks and retains it; otherwise clear this cache.
- Query global solid6460e8, origin0/basisidentity, camera start, glare-minus-
  camera displacement, radius0, flags5 (0x85 if5cab9d nonzero), reset1.
  Original298 seeds preferred face only when nonzero. Positive signed count
  stores result face and returns hidden; miss preserves the old face cache.
- Traverse movers64e96c to sentinel64e6e0 via28c. Each eligible AABB hit
  uses local endpoints and its solid294, reusing query flags/preferred face.
  First positive count stores the moving object pointer in glare294.
- If5cb054 exists, traverse actors5cb2ec to sentinel5cb060 via28c. Compare
  rooms, skip actor state lowbyte nonzero, and when selected object state
  lowbyte equals1 skip its associated426fc0 object. First415280 hit stores
  actor handle2c in290. Finally resolve/test glare parent30, then visible1.
  The actual415280 rejects a candidate matching that parent handle; the
  supplied-service audit preserves the fallback call instead of removing it.

The shared layout now names cached_solid/cached_face as uint32 tokens,
followed by four sample floats. Previously all six words were called float
samples. Constructor zero bytes and sample clearing offsets are unchanged.
No object allocation size changes. The tokens require stable scene-owned
solid/face identity when binding; they are not float values or actor handles.

Confirmed original defect: before the first cached-solid query, constructor
4161f0 clears only preferred-face word0; its vector/matrix constructors do
not initialize data.414e00 sets start/displacement but leaves origin, basis,
radius and flags as previous stack contents.361 exercised cached queries
retain either0xa5 or0x3c poison in all these fields. The ordinary world and
subsequent mover queries are explicitly initialized. A C port must explicitly
initialize the cached query using the ordinary local-ray policy (origin0,
identity basis, radius0, flags5/0x85) and record that intentional correction;
it must not manufacture byte parity by reproducing uninitialized reads.

Audit totals:465 cached-actor returns,212 cached-solid,483 world,87 scanned
solid,239 actor/fallback and562 clear;1371 world queries and583 mover queries.
Next implement complete shared search against this oracle, separately test
the deliberate cached-query initialization, then bind real scene services.
Both PC and NXDK builds pass after the field correction;72 constructor,
197 render-pass,336 collector,420 occluder and21 CTest checks remain green.


Shared full glare visibility search (2026-09-12)
---------------------------------------------
rf_glare_visibility_search implements414e00 in shared C with actual shared
rf_glare_occluder_test/segment-box math. Borrowed96-byte object views retain
geometry, stable nonzero token, handle and solid token. Mover/actor lists
must preserve original traversal order. Backend lookup, cached solid-owner
resolution, room/state/association, solid and model queries bind real owners.
No heap allocation or frame scheduling is performed.84-byte solid queries
retain preferred-face token plus existing80-byte collision input. Missing
cached solid resolution clears that cache; caller guarantees live identity
for original-valid owners. Callback errors leave visible output unchanged
and preserve earlier cache invalidations, rather than undoing prior work.

The confirmed original cached-query defect is corrected explicitly: set
origin0, identity basis, radius0 and flags5/0x85 before its local ray query.
Preferred face is0 there. The subsequent world query uses cached_face;
scanned movers reuse it. Three orientations, including a non-axis matrix,
match the original endpoint transform order. No other search-order change.

verify_glare_search_shared.py executes full original414e00 and415280, with
actual constructors, box, vector and matrix routines. Only external lookup,
room/state/association, solid-query and model-query services are supplied.
1537 PC/compiled NXDK cases compare ordered callback payloads (including
all query input bytes), three caches, visible result and immutable other
owner bytes.161 failures cover all seven backend operations and compare
original callback-boundary cache snapshots, including failed association.
342 original cached-query payloads contain the proven stack poison; only
origin/basis/radius/flags are normalized to the documented corrected values
for comparison. Remaining bytes and every other query compare unchanged.
Original baseline outcomes793 hidden/744 visible; shared runs perform1748
solid and192 model callbacks. Standalone PC probe is --glare-search.

Both builds and21 CTests pass;420 candidate-occluder and197 render-pass
regression checks pass. No native gameplay/glare rendering claim is made:
next bind stable scene solid/face/actor identities, preferred-face handling,
alternating visibility refresh and actual corona/reflection/volume drawing.


Glare dependency: preferred-face collision (2026-09-12)
---------------------------------------------------
Scene inspection found existing solid queries lacked the preferred-face
path required by glare's cached_face. rf_collision_solid_preferred now
implements4df302..4df31b and then the existing uncached room/flat traversal.
Active motion and query bit0 enable the supplied preferred4dec10 face test;
an accepted face returns immediately. It does not need to be in this solid's
active lists, and bypasses room skip/bounds tests. Miss/disabled preference
continues ordinary traversal without changing limit. The face still applies
its own shared collision filters, bounds, plane/polygon and swept tests.

rf_collision_preferred_face borrows face geometry and resolved room/index;
the caller resolves stable tokens, without treating a token as an unchecked
original address. Output remains unchanged on miss/error; matched is set
only on success. Existing transform and nonzero-motion semantics remain.
No allocations, world-token remapping, global face-cache list or special
0x1000 room mode is introduced. Texture-sensitive flags0x80/0x100 remain
explicitly unsupported by the existing face helper; glare's optional0x85
mode therefore still needs reconstruction before fully binding that mode.

verify_collision_preferred.py:2400 complete original4df1c0 comparisons with
actual4dec10 and unchanged no-room fallback, no supplied callees.309
preferred early hits,897 total hits,183 multiple updates and274 edge hits;
plus6 invalid numeric-input guards. Covers independent preferred faces,
flags disabling preference, misses, direct/transformed coordinates, retained
limits, zero motion and swept radii. PC/NXDK query result bytes match.
verify_collision_preferred_rooms.py:2000 complete original4df1c0 comparisons
with actual preferred face and room/tree/face traversal, no supplied callees.
234 preferred hits,687 total hits,168 multiple updates and272 edge hits.
Four single-face trees, primary/child ordering, room skip/bounds bypass for
preferred hits, transformed/direct inputs and swept radii. PC/NXDK compare
geometry, counts and room/face indices. These geometric oracles use the
existing collision harness x87 control0x37f; no native gameplay claim.
Both builds,21 CTests and1537 glare-search checks pass. Next bind stable
scene face identity and reconstruct texture-sensitive query behavior; do not
drop preferred-face semantics when composing scene collision callbacks.


Glare dependency: texture-alpha thin collision (2026-09-12)
--------------------------------------------------------
Original4def23..4def9e samples texture only after a polygon hit and retained
fraction gate. Bitmap face30 must be >=0 and either query80/face40 or
query100/face80 must both be set. The predicates4d4db0/4d7d60 read those
face flags directly.4e1ad0 resolves UV at the hit point;50e330 samples the
bitmap. Its return value is ignored; sampled color high byte below128
rejects the hit, otherwise the ordinary fraction/point/normal is committed.
Negative bitmap IDs or unmatched flags bypass sampling.50cc00 constructor
is a no-op, so the sampling service must actually supply its color.

rf_collision_thin_face_textured composes full existing zero-radius geometry
with this post-polygon alpha gate. It borrows an unchanged72-byte face view,
separate bitmap identity and an8-byte sample backend. The backend receives
original face view, bitmap and computed hit point, and returns packed color;
it will compose actual UV interpolation and bitmap sampling later. No heap
allocation or face-owner layout change. Missing backend reports NOT_FOUND
only when a real hit requires sampling. Callback failure preserves result
and matched; rejected alpha preserves result and sets matched0. Existing
non-textured entry behavior remains unchanged for compatibility.

verify_collision_textured_thin.py passes6060 PC/compiled NXDK cases against
full original4dec10, using actual filter, box, plane, polygon and constructor
code. Only UV4e1ad0 and bitmap50e330 are supplied. Checks exact output and
sampler call/hit point, all flag pairings, signed bitmap bypass, alpha127/128
boundary, geometry misses and60 coplanar misses.282 sampling paths include
155 transparent original outcomes;66 port callback failures and67 missing
backend cases preserve outputs. Original bitmap return alternates without
changing the alpha decision. Harness follows collision control0x37f.

Both builds,21 CTests,6062 existing thin checks and2406 preferred-flat checks
pass. Actual UV interpolation, bitmap decoding/sampling, swept edge behavior
and propagation through preferred/room/flat scene queries remain open. No
native gameplay or new visible effect is claimed by this helper milestone.


Glare dependency: collision UV interpolation (2026-09-12)
------------------------------------------------------
rf_collision_texture_coordinates reconstructs full4e1ad0 with unchanged
projection-axis behavior4fa6d0. It selects the first accepted projected
triangle in an ordered fan anchored at corner0. UV pairs correspond to the
original edge+4/+8 values, separately borrowed from vertex positions. The
original dominant-axis ties/signs and strict +/-0.0001 near-zero branch
are preserved. The general branch rejects zero determinant. Weights retain
original float stores and acceptance comparisons; interpolation order is
weight2*corner2 + anchor*corner0 + weight1*corner1. No coplanarity test,
clamping, alternative triangulation, bitmap access or allocation is added.
The similar model-polygon routine has different intermediate comparisons
and cannot simply stand in for this UV routine.

Input views require3..65536 finite vertices and UV pairs. The port rejects
invalid bounds/data, preserves output UV on miss/error, and reports matched
separately. The original assumes a valid circular edge list; contiguous
ordered arrays avoid reproducing invalid-list dereferences.

verify_collision_texture_uv.py executes original4e1ad0 and4fa6d0 without
supplied callees and matches8192 PC/compiled NXDK results exactly, plus6
finite/count guards.3583 accepted points;2777 near-zero branch visits and
24264 general branch visits. Includes all normal-axis signs/ties, zero
normal,3..8 corners, irregular and degenerate fans, vertices, edge midpoints,
outside points and points away from the face plane. Miss/error preservation
and original edge-list immutability are checked. Original uses control37f;
compiled NXDK also passes at control27f, preserving the control word. The
initial compiled37f run also passed before the27f check was added.

Both builds,6060 texture-alpha regression cases and21 CTests pass. Next
resolve authored corner UV ownership into the sampler and reconstruct50e330
bitmap addressing/alpha sampling, then propagate the composed service into
scene solid queries. This is not yet an authored/native visibility binding.
