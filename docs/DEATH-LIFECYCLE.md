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
