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
