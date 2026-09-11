# Damage reconstruction

The campaign does not yet have a complete live damage/death backend. Shared
Continuous_Damage request dispatch is documented in TRIGGERS.md; the work below
recovers the numeric entity phase needed by that backend.

## Original routing evidence

4892c0 first resolves the target and rejects amounts below binary32 .001. It
marks object flag0x200000 before later gates. Continuous_Damage supplies final
argument1, which bypasses the ordinary invulnerability/difficulty/network gate
block. Object kind0 delegates to41a350; kinds2/3 directly change the field+34
(kind3 requires amount>100); kinds4/7 delegate to other backends. These routing
observations are decompiler/disassembly evidence, not yet wrapper execution
comparisons or reconstructed behavior. The remaining entity routine includes
kill attribution, burn, sound, AI and other effects.

## Single-player health and armor calculation

rf_entity_damage_vitals_sp reconstructs41a350 entry through41a44d, including
complete original helper41a7c0. It accepts health, armor, prior last-damage-time,
amount, damage kind, caller-resolved class multiplier and game-clock bits.
Kind-1 skips the multiplier; other kinds store the scaled amount as binary32.
With positive armor, kinds5/6 absorb nothing, kind4 absorbs up to the full
amount, and other kinds absorb up to amount*.52f. Absorption cannot exceed
available armor. Negative numeric inputs retain original arithmetic rather
than being interpreted here as gameplay healing rules.

Armor is stored as binary32 after subtraction, while the unrounded absorbed
quantity still participates in the health calculation. The game-clock bits
from6460f0 replace last_damage_time. Health in the closed interval[0,.5] becomes
-.1f (bitsbdcccccd). This is the original numeric phase; it does not itself
perform later kill attribution or lifecycle transitions.

The helper uses53-bit x87-compatible intermediates and caller-owned storage,
with no allocation. Nonfinite inputs/results return RF_FORMAT without changing
state or scaled_amount. Those are explicit port guards. The multiplier is
ignored for kind-1. Caller must enforce outer damage eligibility and resolve
the class-table entry; the helper is not a complete replacement for4892c0 or
41a350, and multiplayer armor arithmetic is not implemented here.

## Verification

verify_damage_vitals.py runs8,192 original numeric prefixes with41a7c0 unchanged,
then compares exact health,armor,clock and scaled-amount bits on PC and actual
NXDK-linked code in Unicorn. Cases include all kinds-1..10, zero/negative armor,
class multipliers, absorption limits and the half-health boundary. Four
nonfinite port guards bring each compiled suite to8,196 cases. Source executable
SHA256 is checked; generated artifacts/damage-vitals.json records the NXDK hash.
Both builds and eight CTests pass. No new native gameplay behavior is claimed.

Next recover the remainder of41a350 and its required effects, verify outer
4892c0 routing, and connect owned entity/class data to the event backend.

## Single-player lethal attribution

rf_entity_damage_credit_sp reconstructs41a44d..41a505. Positive health leaves
the previous responsible_handle unchanged. At nonpositive health, the supplied
source normally replaces it. Only kind4 with source-1 takes the fallback:
an existing burn object supplies its+34 source via42f5a0; otherwise a non--1
auxiliary UID searches entity list order with425210. First matching UID wins
and contributes object+2c; absent UID leaves-1. This is an authored-UID lookup,
not a slot/generation handle lookup. No deduplication or sorting is allowed.

The helper takes a stable ordered UID/handle view and a resolved burn-source
snapshot, without allocation or callbacks. Nonfinite health and invalid used
list arguments are explicit output-preserving port guards. It does not create
a burn effect, register entities, perform scoring or transition into death.

verify_damage_credit.py compares4,096 original prefixes, with42f5a0 and425210
executing unchanged and no intercepted calls, against PC/NXDK output. Cases
cover duplicate/missing UIDs, source precedence, empty lists, burn ownership,
positive health and signed zero. Two nonfinite guards bring each compiled
suite to4,098 cases. Both builds,8,196 vitals cases and eight CTests pass.
Generated evidence is artifacts/damage-credit.json; no native gameplay
attribution claim is made until entity ownership and the full backend connect.

## Pain and death sound routing

rf_entity_damage_sound reconstructs complete4196f0. Nonpositive health with a
death descriptor other than -1 resolves the death class and requests playback
once, then ORs flag4 after the callback. This preserves callback flag changes;
the original requests death playback even if resolution returns -1.

The other path rejects the combined427020/42a8e0 predicate, actions1/17 and an
unexpired timer. It sets a 1000ms deadline before resolving the pain class,
even when the sample is missing or a voice is already playing. Fraction above
binary64 .3 chooses the heavy class; binary32 .3f is above that threshold.
A valid sample plays only when the existing voice is not playing.

The backend supplies resolution, voice status and entity-owned positional
playback corresponding to48a9c0(entity,position,sample,1,0). Predicate bytes
are supplied snapshots. Nonfinite health, fraction or position is an explicit
port guard. State/backend storage must remain alive during callbacks; only
flags may change through playback. This helper does not load sounds or own
entities, and is not yet connected to the campaign damage backend.

verify_damage_sound.py executes complete4196f0 with original timers and vector
copy, intercepting only predicates and external sound calls. It compares
4,096 cases against PC and actual NXDK-linked code, including timer wrap,
signed zero, byte predicates, threshold boundaries, missing samples and flag
mutation during playback. Five nonfinite guards bring each compiled suite to
4,101 cases. Exact state and ordered sound arguments match; generated evidence
in artifacts/damage-sound.json records original and NXDK hashes. This verifies
routing, not audible playback or the remaining damage/death lifecycle.

## Outer single-player eligibility and object dispatch

verify_damage_wrapper_trace.py executes complete original4892c0 for8,192
cases, with SP globals64ecb9/ba and6fc4d8 zero. Lookup, immunity/player
predicates and delegated effects are supplied; exact call order, arguments,
health, flags and return value are checked. This harness produces original
behavior evidence; the compiled wrapper comparison is described below.

Missing targets and amounts below binary32 .001 return zero without setting
flags. Otherwise object+7c gains0x200000 before any subsequent rejection.
A nonzero low byte in the eighth argument bypasses immunity and difficulty
checks; Continuous_Damage supplies1 here. With a zero low byte, object flag4
rejects; an entity resolved by426fc0 also rejects when42cca0 returns a nonzero
low byte. For damage kinds other than9,48aaf0-selected targets scale damage by
the selected593dd4 table entry: .1f,.25f,.4f,.55f. Kind9 bypasses this scaling.
The meaning of6fc4d8 and its alternate path remains outside this SP fixture.

Object kind0 calls41a350(object,amount,source,damage_kind,auxiliary_uid) and
returns its float-rounded result. Kind4 calls410270 with argument6 forwarded
as the fifth argument; kind7 calls417c60 with four arguments. Kind2 subtracts
health directly; kind3 does so only for amount strictly above100. Other object
kinds have no delegated effect. All eligible routes then query48aaf0 again;
selected targets with health in(0,.5] become zero, including after a delegated
effect modifies health. Non-entity routes return zero regardless of health
change. Argument4 is unused in this verified SP path. Keep these distinctions
when connecting the event backend; direct subtraction is not entity damage.

The trace covers all object kinds0..9, threshold boundaries, all four
difficulty entries, low-byte predicates, forced hits and mutations performed
by the supplied effect callback. It records3,257 accepted routes. Generated
artifacts/damage-wrapper-trace.json includes the checked original SHA256.

rf_damage_dispatch_sp now implements this path in shared C. Its backend owns
object lookup, the three predicate stages and per-type delegated effects.
The borrowed object remains alive across callbacks; effect callbacks may
change health and flags, and the final player predicate is queried anew.
The request stays stable. No allocation or actor lifetime management occurs
inside this dispatcher. Missing targets return successful zero damage.

Finite values are required. Malformed arguments return errors; errors after
marking flags or invoking an effect do not roll back earlier changes. The
output result is committed only on success. The multiplier is caller-supplied
from the selected original difficulty row. Multiplayer and the alternate
6fc4d8 path must not use this entry point without further reconstruction.

verify_damage_dispatch.py compares8,192 original results to both the PC probe
and actual NXDK-linked function, checking full callback order, effect
arguments, health/flags and returned damage. Two nonfinite input guards bring
each compiled suite to8,194 cases. Both builds and eight CTests pass. This
establishes the shared entry point but does not yet connect campaign entity
ownership or the remaining41a350 effects to Continuous_Damage.

## Remaining entity effects: original trace evidence

verify_damage_effects_trace.py executes41a505..41a7ab after the vitals/credit
phases. All external effect functions, predicates, UID/entity lookups and
random returns are supplied. Across8,192 cases it checks exact ordered calls,
arguments and the original writes to burn pointer13d8, voice854 and flags814.
Every intercepted function is reached. This establishes orchestration rules;
it does not implement those callees or prove a live entity lifecycle.

Incoming damage above5 requests428740 pain animation. Incoming damage divided
by class health(+44), above binary64 .001, requests4196f0 except for kind10.
The original stores a float quotient but compares the still-wide x87 value;
only the argument sent to4196f0 is rounded to float. Class-scaled damage must
not replace incoming damage in these two decisions.

For kind4, an existing burn skips creation. Otherwise zero class armor, an
armor ratio below .5, or flag814 bit8000 enters the burn eligibility path.
Predicates429990,4290d0,42a8e0,429a80 reject in that order on nonzero low bytes.
Source-1 plus a supplied auxiliary UID attempts425210; a found object supplies
its handle.426fc0 then resolves the source entity. A missing source requests
42e910(target_handle,-1). With a source, creation requires either its42a8e0
selection with target bit2000 clear, or differing object fields1f8. The exact
meaning of that field is not established by this trace.

A successful burn allocation stores13d8 and, unless class flag728 bit10 is
set, calls504e40(5,10) then4089f0(entity+2a0,random_float,0). When armor remains
at least half, class armor is nonzero and bit8000 is clear, bit2000 instead
requests504e40(3,5) then4085f0(entity+2a0,source,random_float). Kind4 always
clears bits2000 and8000 afterward, including rejection and existing-burn paths.
The harness covers388 creation requests,91 successful-burn reactions and143
armor-protected reactions with a supplied random return. It does not establish
the random distribution or downstream reaction implementation.

Kind6 polls voice854 unless flags810 bit80000000 is set. If inactive, it calls
5056a0(0x23,entity+3c,1,173c378,0) and stores the returned voice. Next42a8e0 is
queried afresh; selection, positive pre-hit health and positive class-scaled
damage request4a7520 except for kind10. Finally4895d0 is queried; a zero low
byte and positive current health request407fb0(entity+2a0,source,incoming,0).
Reconstruct these effect owners before claiming campaign damage support.

## Entity-state predicates for integration

rf_entity_armor_immunity reconstructs the normalized low byte of42cca0 for a
present entity: class724 bit02000000 must be set, current armor must be
strictly positive, and entity814 bit20 must be clear. A missing entity is
handled by the dispatcher's lookup stage. The predicate preserves original
floating comparison behavior, including false for quiet NaN and signed zero.
It requires class and entity flag ownership before live use.

verify_armor_immunity.py compares complete original42cca0, without hooks,
against PC and NXDK code for4,096 cases. Inputs include random complete flag
words, positive/negative armor, signed zero, subnormal, infinities and quiet
NaN. The report records executable hashes in artifacts/armor-immunity.json.

Other predicate dependencies overlap existing entity/trigger reconstruction:
429990 checks class type1;4290d0 resolves linked handle200 and checks class1.
427020 returns low-byte1 for a missing entity, otherwise entity810 bit1.
42a8e0 requires object7c bit8 plus nonnull1430;429a80 instead requires bit8
with42a8e0 false. Consequently the sequential burn gate excludes both forms
of bit8, but its original predicate call order must still be preserved.
These findings come from the exported original functions; the new compiled
comparison here covers42cca0 only. Shared burn orchestration and live owned
predicate adapters remain open.

## Shared effect orchestration

rf_entity_damage_effects now reconstructs41a505..41a7ab in shared C. The
backend supplies owned predicate/lookup access, burn allocation, random values,
notifications and kind6 audio. Separate input values retain incoming damage,
class-scaled damage and pre-hit health. The state carries current health,
armor, class limits, flags, affiliation field, burn token and voice handle.
UID lookup returns a handle orUINT32_MAX; burn allocation returns a nonzero
token or0. This layer allocates nothing itself.

Notifications preserve the original order and value choice. Pain sound gets
the float-rounded fraction after a double-precision threshold comparison;
burn and armor reactions get the random duration; AI gets incoming damage.
The kind6 playback adapter must implement the fixed5056a0 arguments and obtain
the current entity position. Entity and backend storage must survive all
callbacks. Stable identity/class inputs are required; effect callbacks may
change health, flags and burn/voice state. The comparison now includes mutations at all nine effect callback boundaries;
coverage is described below. Arbitrary ownership/lifetime changes remain outside
the supported contract.

Nonfinite inputs/state, zero class health and nonfinite generated float
arguments return explicit port errors. Errors after effects have begun do
not roll back prior callbacks or writes. These guards are not original game
behavior and must not be used to conceal invalid class/resource ownership.

verify_damage_effects.py compares8,192 original traces against PC and actual
NXDK-linked C, including exact state and ordered downstream arguments. Seven
nonfinite guards bring each compiled suite to8,199 cases. Both builds and
eight CTests pass. Generated artifacts/damage-effects.json records hashes.
Next assemble vitals, attribution and this sequence, then connect actual
burn/pain-animation/AI owners and campaign entity registration. The existence
of this backend does not imply those downstream effects are implemented.

## Effect callback mutation coverage

The effect comparison now includes8,192 stable-state cases plus8,192 mutation
scenarios. A chosen callback replaces health and XORs selected flags810/814,
burn token and voice bits. In1,251 exercised callback mutations, PC and actual
NXDK code preserve the original final state and subsequent callback sequence.
All nine boundaries are exercised: pain animation369, pain sound401, burn
creation45, random26, burn reaction12, armor reaction13, kind6 playback22,
player feedback62 and AI reaction301. Cases whose selected callback is gated
out also check that no mutation occurs. The suite totals16,391 compiled cases
including seven nonfinite guards; no core changes were needed.

This verifies reads/writes around callbacks, including overwriting a callback's
burn/voice mutation with its return value and observing changed health before
AI notification. It does not verify destroying entities during callbacks,
changing class/identity fields or running the actual downstream effect code.

## Composed single-player entity damage

rf_entity_damage_sp joins the numeric prefix, lethal attribution and effect
orchestration into one shared entry point. It captures pre-hit health and the
incoming amount, applies class/armor damage, commits health/armor/time, resolves
lethal responsibility, then dispatches effects with both incoming and scaled
amounts. Its return is the class-scaled damage, independent of later callback
health mutations. Outer4892c0 eligibility remains a separate dispatcher.

The caller owns a persistent rf_entity_damage_state. Before entry it refreshes
burn_source from the currently owned burn token. Attribution uses that source
for an existing burn; otherwise the backend's authored-UID lookup supplies
the fallback handle. Effect callbacks operate on the embedded live effect
state. The backend must not destroy or replace that storage during the call.
Errors preserve the return output but do not roll back committed state or
effects. This is not a transaction or a complete entity lifetime manager.

verify_damage_effects.py --full runs16,384 original paths from41a350 entry
through return preparation41a7ab. Original armor41a7c0 and burn-source42f5a0
execute unchanged; lookups and downstream effects remain supplied. PC and
actual NXDK code match exact health/armor/time/credit, effect state, prepared
float return and ordered callback arguments. Half the cases offer mutations
at the nine effect boundaries. The existing effect-only suite also passes,
as do both builds and eight CTests. artifacts/damage-full.json records hashes.

Next bind persistent class/entity/burn ownership and implemented downstream
effects to this composed entry point, then connect the event damage callback.
No live campaign death, burn animation or AI behavior is claimed by this test.

## Burn creation and pool ownership evidence

verify_burn_creation_trace.py executes complete42e910 for576 cases, including
72 accepted allocations. It supplies descriptor construction/copy, target and
eligibility results, attachment lookup, emitter allocation and sound calls.
The original list operations and slot writes execute unchanged. Free and
active circular lists of sizes0..3, every rejection stage, partial emitter
failure and audio failure are checked. artifacts/burn-creation-trace.json
records source identity and scope. This is original evidence, not shared C
burn allocation or cleanup yet.

42e910 first constructs a temporary132-byte particle descriptor. A null free
head62f76c returns0. Otherwise it resolves the target via426fc0, requires
40a1e0, rejects42cca0 immunity and rejects the class-name comparison against
`masako`.42eb20 must resolve all four attachment slots. If it fails, the
free record's14/18/1c/20 fields become-1 and the lists remain unchanged.

On success it copies the particle template indexed by595f28, requests three
497ca0 emitters, copies the595f2c template and requests a fourth. All four
requests use(target_handle,descriptor,0,0,1); returned handles occupy00..0c.
It stores target at10, resolves sound class595f24 and starts positional audio
with(sample,entity+3c,1,173c378,0), storing the voice at24. Neither emitter
failure nor sound failure aborts the record. The remaining initialized fields
are float1 at28, byte0 at2c, word0 at30 and source handle at34; padding2d..2f
is not cleared. Links38/3c remove the record from the free ring and append it
before the active head62f770, leaving an existing active head unchanged.

Raw42eb20 attachment lookup tries `lowerleg-l`, then `tech- leg-l-lower`;
`lowerleg-r`, then `tech- leg-r-lower`; and `spine01`, `spine03`,
`tech- 1spine`, `tech- 1spine01` in order. Its fourth name and42ec80 lookup
semantics still need verification. These raw attachment findings were not
executed by the allocation harness, which supplies the four results.
Next recover actual pool initialization, release/update ownership, attachment
resolution and particle templates before binding create_burn in gameplay.

## Burn pool initialization and release

Complete42e8a0 initializes exactly eight64-byte records at62f778..62f978,
clears active head62f770, builds the free ring62f76c in array order and clears
timer62f768 through4fa3e0. The512-byte figure covers records only; emitter,
texture and audio memory are separate. Initialization calls42ed20(record,1)
for each existing slot rather than blindly clearing all bytes.

42ed20 handles each nonzero emitter in00/04/08/0c in order:4973d0 with the
emitter inECX, then497d80(emitter), then zeroes its slot. Emitter absence is0;
voice absence is-1. Voice24 is stopped via505a40 unless-1, including voice0.
It resets target and attachment indices10..20 to-1, voice24 to-1, scalar28
and word30 to0, and byte2c to0. Padding2d..2f and source34 remain unchanged.

Cleanup searches the entity list5cb2ec for the first13d8 pointer matching
this record. It clears that pointer and stops searching. Only if no entity
matches does it search list5cae44 for the first matching2d0 pointer. Duplicate
references are not all cleared; the owner adapter must preserve this order
and must not create duplicate ownership accidentally.

A nonzero low byte in argument2 resets fields/ownership without changing
the pool lists. A zero low byte removes the record from the active ring and
appends it before the free head. Existing heads are retained except when
removing the active head, which advances or becomes null for the last record.

verify_burn_release_trace.py verifies432 release cases and complete eight-slot
initialization, executing original list traversal and timer clear unchanged.
Only emitter reset/free and voice stop are supplied. It covers head/interior/
tail release, empty/populated free rings, duplicate owner matches and low-byte
argument behavior. The creation harness was corrected to use0 for supplied
emitter allocation failures; all576 creation cases still pass. No shared pool
implementation, actual emitter cleanup or per-frame42ee80 burn update is
claimed yet. Those remain the next ownership work.

## Burn owner update and spread timer

verify_burn_tick_trace.py verifies4,096 original owner-tail paths at
42f1dc..42f2a2, with427020 unchanged and supplied random/effect calls. It
checks audio arguments, damage requests, elapsed time and action824;1,011
paths issue damage and2,053 request fade processing. Thirty original timer
epilogues execute4fa3f0,40a0d0 and4fa360 unchanged, covering cleared timers,
expiration, future deadlines and wrap. Evidence is artifacts/burn-tick-trace.json.

The tail always calls5058c0(voice,owner+3c,owner+144,scalar28), even voice-1.
If the low byte at record2c is nonzero, it adds frame delta5a4014 to elapsed30
and calls42f2f0. Otherwise, if427020's low byte is zero, it requests a random
divisor in[5,8], then4892c0(owner_handle,dt*(class_health/divisor),-1,-1,4,0,-1,0).
The quotient/product remain wide until the outgoing float argument is stored.
This is an ordinary, non-forced damage request. It then calls rand and assigns
action824 to5,14 or15 according to the nonnegative result modulo3. Delta0
still issues the zero-amount request and advances that random action choice.

The end-of-pass timer62f768 is rearmed for225ms only when expired or cleared.
It does not gate the owner-damage tail. In the earlier raw42ee80 path it gates
nearby-entity spread checks; that geometry and its mutation/iteration behavior
remain unverified. The original per-frame path also skips records lacking any
of their four emitter pointers, which makes partial creation failure relevant
to lifetime behavior. Verify that whole-loop behavior before reproducing it.

Raw42f2f0 handles fading and eventual release, including direct emitter-field
scaling and an owner reaction. This helper and its time thresholds still need
an executable comparison; the current owner-tail test intercepts it. Shared
pool creation/release/update and actual particle attachment remain open.

## Shared fixed burn pool

include/rf/burn.h and src/core/burn.c now implement42ed20 release and42e8a0
initialization. Eight64-byte records retain source/padding semantics; circular
links are1..8 slot tokens with0 for an absent head. Heads and the spread
deadline bring pool storage to524 bytes. There is no heap allocation here.
Particle and voice tokens remain owned by external backends.

Release preserves reset/free/stop ordering and invokes the owner adapter after
clearing payload fields. The adapter must search entity references first,
then other references only on an entity miss, clearing the first match only.
Callbacks must retain storage and must not mutate pool records or links.
New pool storage must be zero-initialized before its first initialization;
later initialization cleans existing resources before rebuilding list order.

Normal release validates ring links, membership and disjoint active/free
lists before effects. Malformed topology or invalid tokens return explicit
port errors without mutations. The nonzero-low-byte reset mode leaves links
unchanged. Validation does not add rollback semantics to external callbacks.

verify_burn_pool.py normalizes original pointers into slot tokens and compares
432 original release cases plus complete initialization on PC and NXDK.
It checks every record byte, both rings, deadline, owner references and call
order. Four malformed-input guards bring each compiled suite to437 cases.
Both builds and eight CTests pass; artifacts/burn-pool.json records hashes.
Burn allocation, real emitter/audio adapters and per-frame update remain open.

## Shared burn creation

rf_burn_create now reconstructs42e910 on the fixed pool. It prepares the
temporary descriptor even on exhaustion, preserves ordered target/class/bone
gates, requests three emitters from the first template and one from the second,
then requests sound and transfers the accepted record into the active ring.
The backend owns the temporary descriptor and may change it between emitter
calls; pool records and links must stay stable during callbacks. It supplies
ordered attachment results and fixed original emitter/audio call semantics.

Rejection/exhaustion returns successful token0. Accepted allocations return
1..8; zero emitter results and voice-1 remain valid stored failure results.
Malformed or overlapping rings return errors before backend effects. This
does not introduce a fallback emitter, sound, heap pool or altered capacity.

verify_burn_create.py compares612 original cases on PC and actual NXDK code,
including an all-eight-free ring, pool exhaustion, every eligibility failure,
attachment failure and partial emitter/audio failure. Two ring guards bring
each compiled suite to614 cases. All payload bytes, links, return tokens and
callback order match; callback arguments/template selection are also checked.
The437 release/init cases, both builds and eight CTests pass. Generated
artifacts/burn-create.json records executable hashes. Next implement update/
fade and bind actual attachment, particle, audio and entity ownership adapters.

## Burn fade and expiry evidence

verify_burn_fade_trace.py executes complete42f2f0 for8,192 cases using the
original shared timer. Emitter stop, typed owner lookup, entity reaction and
release are supplied. All bytes of four distinct emitter fixtures are checked,
along with volume28, owner flags29c and ordered calls. Cases include exact
thresholds and their next floats, cleared/future/expired timers and byte wrap.
The trace records2,391 release requests,2,143 scaling passes and3,831 groups
of three emitter-stop calls; evidence is artifacts/burn-fade-trace.json.

After elapsed30 exceeds12, if any of the first three emitter140 bytes is
nonzero,42f2f0 stops all three and resolves4174c0(target), then changes its
flags29c to(flags & ~0x200) |0x100. Disassembly confirms4174c0 accepts only
object type7. This result is dereferenced without a null check: actual owner
conversion/identity is a required lifecycle dependency, not an optional input.
The harness supplies a valid result rather than claiming that transition.

Elapsed above17 requests entity426fc0, optionally407ee0(entity+2a0), then
42ed20(record,0). Otherwise only an expired shared timer permits fade scaling.
After elapsed exceeds5, that tick first decrements fourth-emitter byte87.
A resulting0 takes the same release path before scaling; starting0 wraps255.

On a scaling tick, the first three emitters'44/48/30/34 float fields multiply
by binary32 .95f, while24/28 multiply by .9f. The fourth emitter's44/48/24/28
multiply by .75f; its30/34 fields are unchanged. Record volume28 multiplies
by .95f. The semantic names of these emitter fields still need reconciliation
with the reconstructed particle owner. Their exact offsets and arithmetic are
verified, not a guessed visual interpretation. Original stop/release callees
and owner conversion remain outside this fixture; shared fade is still open.

## Shared fade routine

rf_burn_fade now implements42f2f0 using four borrowed emitter views and the
shared timer. It preserves the six field updates in original field/emitter
order, the byte87 wrap, owner flags, volume scaling and early release paths.
The backend supplies emitter stop, type7 flag access, optional entity reaction
and release. Release may invalidate storage; the function returns immediately
after it. Stop callbacks may update emitter views, but identity/storage must
remain valid until release. Current oracle coverage uses stable callbacks.

The function requires four distinct finite emitter views. Nonfinite values,
missing views and aliases are explicit port guards. Missing type7 owner returns
RF_NOT_FOUND after the stop calls instead of the original null dereference;
this does not replace the required owner lifecycle transition. Earlier effects
are not rolled back. The caller still owns particle-field synchronization and
must not use detached snapshots as a substitute for persistent emitter state.

verify_burn_fade.py compares8,192 original cases to PC and actual NXDK code,
checking compact emitter fields/bytes, complete record, type7 flags and ordered
callbacks. Six nonfinite guards bring each compiled suite to8,198 cases.
Creation614 cases, pool437 cases, both builds and eight CTests also pass.
artifacts/burn-fade.json records hashes. Next implement the full per-frame
loop and bind actual particle, model-attachment, audio and entity ownership.

## Missing-owner iteration defect in the original

verify_burn_iteration.py audits90 synthetic traversal scenarios in original
42ee80, with real42ed20 release, constructors and timer epilogue. The valid
owner body is skipped at42ef3e to isolate list traversal; lookup and external
emitter/audio release are supplied. Of these,72 missing-emitter paths complete
normally, but all18 missing-owner paths with four nonzero emitters enter a
cycle through the free ring after releasing the selected record.

At42eee1 the original saves the next active record inEBP. Normal completion
uses42f2a2 (`mov esi,ebp`) before the loop test. Missing-owner release instead
jumps from42ef39 directly to42f2a4, retainingESI as the released record. That
record now belongs to the free ring; its zero emitter fields skip work, and
subsequent iteration follows the free ring without reaching a null next or
the active head. The audit bounds execution to10,000 instructions and verifies
repeated zero-emitter free records, exactly one release, and an unrearmed timer.
It does not hang a live game or prove this state occurs in normal campaign play.

The shared update loop must deliberately resume the saved next active token
after missing-owner release, matching normal traversal's rule. Do not reproduce
this original branch's infinite loop. This is an explicit defect correction,
not a claim of byte-for-byte behavior in the invalid-owner case. Confirm the
corrected traversal with head/interior/tail removal before live integration.
The current shared pool functions do not yet implement that update loop.

## Shared traversal with missing-owner correction

rf_burn_pool_update now implements42ee80's list traversal, four-emitter gate,
missing-owner release and225ms timer epilogue. Its body callback remains the
boundary for attachment/spread/audio/damage/fade work. The function saves the
next active token before dispatch and resumes it after release, deliberately
correcting the original free-ring cycle. It validates initial ring topology
and checks for repeated/out-of-range traversal tokens rather than looping.

The body may release its current record, but cannot remove/reorder other active
records or reuse released tokens during the pass. Lookups are read-only.
Failures do not roll back previous records' effects and do not complete the
timer epilogue. The normal epilogue checks the current shared deadline and
sets225ms only if expired or cleared. No record or emitter storage is allocated.

verify_burn_update.py compares90 reference paths with only42ef39 redirected
to42f2a2 in Unicorn; installed RF.exe is unchanged. Real original release and
timer code execute, while the live-owner body is supplied. All90 paths now
terminate, including the18 formerly cycling missing-owner cases. PC and actual
NXDK code match pool bytes, callback order and deadline. Two malformed-list
guards bring each compiled suite to92 cases. Both builds and eight CTests pass.
The unmodified audit still confirms the18 original cycles. This verifies the
explicit branch correction, not bit-identical behavior in that defective path.

Remaining work is the full body, including normal fade-triggered current-record
release in this traversal, attachment/emitter geometry and spread targets, plus
live adapters. The existence of this traversal callback is not a completed
per-frame burn simulation. Evidence is artifacts/burn-update.json.

## Shared burn owner update

rf_burn_owner_tick reconstructs42f1dc..42f2a2 in shared C. Positional audio
receives the current voice, owner position/velocity and record volume first.
An active burn on a living owner requests kind4 damage every frame, using
frame_seconds * (class_health / random(5,8)), with the original float store
and extended intermediate arithmetic. It then consumes one random integer
and writes reaction5,14 or15. The225ms spread timer does not gate this damage.
A fading record instead advances elapsed time by the frame delta and dispatches
fade; the callback may release the record and no later record access occurs.

verify_burn_owner.py compares4,096 original owner-tail cases against PC and
actual NXDK-linked code, checking complete record/owner bytes and ordered
audio vectors, damage arguments, random calls and fade requests. Six nonfinite
input guards bring each compiled suite to4,102 cases. The original predicate
427020 executes unchanged; random values and downstream audio/damage/fade
calls are supplied. Callback mutation is permitted by the API in documented
fields but is not covered by this oracle yet. Both builds and eight CTests pass.
Evidence and executable hashes are in artifacts/burn-owner.json.

This remains an isolated verified phase, not a live campaign burn effect.
Attachment evaluation and spread targeting precede it in the original body;
they must be reconstructed and connected with persistent particle/entity/audio
ownership. Existing rf_particle_emitter_update already recovers4972f0 and
should be reused when those adapters are connected. Full traversal with fade
releasing its current record also remains an integration check.

## Burn attachment and spread body evidence

verify_burn_attachment_trace.py executes original42ef3e through42f0bd, or
the age skip to42f1dc, across1,024 scenarios. Model tag vectors and emitter
position/update calls are supplied; original vector helpers execute unchanged.
Attachment index2 (record1c, spine) positions and updates emitter3 first at
every age. Through elapsed12 inclusive, it then evaluates attachments0,1,3
and updates emitter0 at the leg midpoint, emitter1 at the spine, emitter2 at
attachment3. Older records skip those three updates and spread. Ordinary
midpoints differ from direct averaging by at most1.1920928955078125e-6 in
this suite because the original subtracts, normalizes, computes distance,
scales twice, then adds. Coincident leg tags produce NaN in all three midpoint
components: the original normalizes a zero vector. A future port guard must
be identified as a deliberate correction; do not claim a simple average is
bit-identical original behavior. Room relocation4972a0 and emission4972f0
are intercepted, so this does not verify their ownership integration.

verify_burn_spread_trace.py executes original42f0f7..42f1dc across3,125
scenarios with243 damage requests. It traverses owner then target, verifying
self-exclusion before predicates. Ordered predicates429990,427020,40a110,
4290d0 reject only a low-byte result exactly1 (not any nonzero value).
The supplied predicate words include0,1,2,256,257 in every combination.
Real4faf00/40a180 calculate squared distance from the supplied world spine.
Constant589418 is4.0, so the inclusive spread radius is2 world units, not4;
positions immediately below, at and above2 verify the boundary.

A qualifying target gains flags814 bit2000 before random(5,8). Damage is
float((target class health / random value) *0.25), where5893d4 is0.25; it
is not frame-delta-scaled. The complete4892c0 arguments are target handle,
amount, burn record target, global87243c, kind4,0, owner UID,0. The exact
arguments and flags match. Predicates, random and damage are supplied; the
list is stable and world spine already transformed. Timer gating, world
transform, callback mutation and live integration remain outside this trace.
Evidence is artifacts/burn-attachment-trace.json and burn-spread-trace.json.
These traces establish the body behavior for the next shared implementation.

## Shared burn attachment phase

rf_burn_attachments now implements42ef3e..42f0bd with model-tag evaluation
and combined4972a0/4972f0 callbacks. It preserves spine-first emitter3 update,
then re-reads elapsed for the inclusive12-second gate. Eligible records query
left/right/fourth tags and update emitters0,1,2 in original order. The midpoint
retains separate float stores for subtraction, normalized components, distance,
scaling, halving and addition. The result supplies local spine and age eligibility
for subsequent world-transform/timer/spread work. No allocation occurs.

Finite values and four nonzero emitter identities are required. Coincident leg
points return RF_RANGE after the prior spine update and tag queries, deliberately
preventing the original NaNs from reaching emitter0. This is an explicit error
boundary, not a repaired complete burn simulation. Callers must handle the error
and own cleanup; previous callbacks are not rolled back and result is unchanged
on failure. Model identity and record storage must survive all callbacks.

verify_burn_attachments.py passes1,025 cases on PC and actual NXDK-linked code:
1,024 original placement cases (including explicit degenerate divergence), plus
a nonfinite-age guard. Non-midpoint output and call order are exact; midpoint
comparison allows2e-6, with zero measured error in this suite. Both builds and
eight CTests pass. Evidence is artifacts/burn-attachments.json. Callback mutation,
room relocation and emission remain untested at this combined boundary.
Next connect spread selection and world transformation, then assemble the
complete per-record update with persistent model/particle/audio/entity adapters.

## Shared burn spread loop

rf_burn_spread reconstructs42f0f7..42f1dc using a caller-owned linked list of
persistent target views. It excludes the owner by identity, calls the four
predicates in original order, rejects only low-byte1, and compares squared
distance to4 using individually stored float differences and double products.
Qualifying targets gain flags814 bit2000 before random(5,8); the damage
callback receives the original source/global/UID values and quarter-health
formula. It reads the current target next link after callbacks rather than
saving it before damage. No allocation or frame-delta scaling occurs.

The list ends atNULL and a caller-provided visit limit bounds corrupt cycles.
Current target storage must survive callbacks until its next link is read;
the caller must synchronize these views with actual damage/entity state.
World spine, global value and UID are stable inputs. Nonfinite consumed fields
or an invalid divisor return an error without rollback; bit2000 can already
be set. These guards do not establish a complete campaign ownership lifecycle.

verify_burn_spread.py passes3,128 PC and actual NXDK cases:3,125 original
stable-list paths plus nonfinite-world-point and two visit-limit guards.
Target flags, predicate order, random arguments and complete damage arguments
match exactly. Both builds and eight CTests pass. Evidence and hashes are in
artifacts/burn-spread.json. Live callback mutation, multi-target damage changes,
world transformation, timer/age gating and actual campaign adapters remain
to verify and connect before this becomes a full per-record burn update.

## Complete original burn-body ordering and mutation

verify_burn_body_trace.py executes the full live-owner body42ef3e..42f2a2
without skipping its attachment, timer, world-transform, spread or owner-tail
phases. Across54 scenarios it varies age0/12/above12, disabled/expired/future
spread deadline, identity/quarter-turn owner basis, and two damage mutation
modes. Real vector helpers,4facb0/40a350 world placement,4fa3f0 timer and
427020/40a110 flag predicates execute. Tag vectors, room/emitter update,
class/link predicates, random, audio, damage and fade implementations are
supplied. The list contains the owner and two targets, with the second target
at the inclusive two-unit boundary. Twenty spread hits and four fade calls
are observed; complete ordered calls, elapsed time, reaction and target flags
match expectations. The shared deadline is unchanged by this body; rearming
belongs to the outer traversal.

In one mutation mode the first target's damage callback sets the second
target's810 bit1. Its subsequent real427020 rejects it before distance/random/
damage. In another mode that callback sets the burn record fading byte; the
later owner tail dispatches fade and advances elapsed instead of applying
owner damage and consuming reaction RNG. Positional audio still precedes that
tail decision. These are executable original-code evidence that composed
phases must read persistent mutable target/record state at the original
boundaries. Precomputed eligibility or stale owner snapshots would be wrong.

Evidence is artifacts/burn-body-trace.json. This is original-body verification,
not a compiled shared composition or live campaign test. Next join the shared
attachment, timer/world-transform, spread and owner phases under this same
mutation-aware oracle, then verify traversal with actual fade release and
connect persistent model/particle/audio/entity adapters.

## Composed shared burn body

rf_burn_body joins the shared attachment, spread and owner phases in the
original42ef3e..42f2a2 order. After attachment updates, eligible age checks
the current shared deadline. An expired deadline transforms the local spine
through the current owner basis, stores the rotated float components, then
adds owner position with another float store. It reads the live spread-list
head and runs spread before owner positional audio and damage/fade dispatch.
The body never rearms the deadline; outer pool traversal owns that action.

rf_burn_body_context holds references to persistent owner/list/deadline/basis
state. Damage mutations must synchronize those views immediately so later
targets and owner decisions see them. Stable frame/UID/global inputs and the
existing phase storage contracts apply. Errors preserve prior side effects;
there is no transaction rollback. The final fade callback may release the
record. No heap allocation or per-frame pool copies are introduced.

verify_burn_body.py compares the entire shared body against the54 original
scenarios on PC and actual NXDK-linked code. Complete record and owner bytes,
target flags, unchanged deadline and every normalized callback match exactly,
including attachment/update order, all spread filters, full damage arguments,
audio vectors and reaction/fade calls. Both damage mutation modes pass.
Both builds and eight CTests pass. Evidence is artifacts/burn-body.json.

This verifies composition through supplied model/particle/audio/damage/fade
callbacks, not native gameplay. The next integration boundary is pool traversal
with real shared fade/release, followed by persistent campaign adapters. The
standalone phases remain reusable; they are now joined by a concrete shared
body rather than requiring each platform to reproduce the ordering.

## Pool traversal through real fade and release

verify_burn_retirement_trace.py executes original42ee80 traversal, owner tail
42f1dc, full42f2f0 fade and actual42ed20 release across540 retirement patterns.
Every subset of active counts1,2,3,8 is tested with cleared and expired shared
deadlines, including removal of head/interior/tail and all eight records.
The attachment/spread portion is skipped; non-retiring owners have flag810
bit1 set, while retiring records fade past17 seconds. Emitter fields are zero
and inactive, and entity reaction/owner lists are absent. External lookup,
audio and resource callbacks are supplied. The normal fade path needs no
branch correction: the original resumes the saved next active record.

Across2,082 record retirements, every original pass visits the initial active
records once, appends released records to the free ring in traversal order,
preserves remaining active order, and rearms the deadline to1225. This is
distinct from the previously documented missing-owner branch defect.

verify_burn_retirement.py compares the same cases through shared
rf_burn_pool_update -> rf_burn_owner_tick -> rf_burn_fade -> rf_burn_release.
PC uses actual C callback adapters; NXDK uses callback thunks that invoke the
actual linked functions without emulating their implementations. Complete
pool bytes, heads, ring links, deadline and ordered external calls match
exactly in all540 cases. Evidence is artifacts/burn-retirement.json.

This closes normal fade-triggered current-record removal at the traversal
boundary under the stated fixtures. It does not yet combine active emitter
updates, live owner-reference clearing or campaign resources with this path;
persistent model/particle/audio/entity adapters remain the next work.

## Live adapter audit: burn bone lookup

The campaign currently registers compact predicate views, not persistent NPC
damage/burn owners. Its level-particle state owns an emitter pool, but the
model, entity and damage bindings needed by burn callbacks are not yet joined.
Do not connect burns by inventing temporary NPC owners or silently borrowing
level emitter identities.

An additional model distinction was recovered from42ec80: it resolves the
owner model through503c00, requires positive model48 bone count, then invokes
51d690. This is not the existing51d5b0 tag lookup.51d690 searches only bone
names at model4c with stride4c and calls573930 (strstr), returning the first
case-sensitive substring match. Prefixes and suffixes are accepted; case is
not folded. Empty query matches the first bone. Constant595fd4 is "head",
resolving the fourth42eb20 burn attachment query. The other fallback query
strings remain lowerleg-l/tech- leg-l-lower, lowerleg-r/tech- leg-r-lower,
and spine01/spine03/tech- 1spine/tech- 1spine01.

rf_model_find_bone_substring now provides the bounded shared lookup using
caller-owned rf_model_name views. It preserves first-match order, leaves
output unchanged on absence/error and performs no allocation.
verify_bone_substring.py runs unchanged51d690 and real573930 against PC and
actual NXDK code in2,048 cases, including actual burn strings, duplicates,
prefix/suffix, case mismatch, empty names/query and missing bones. All match.
Both builds and eight CTests pass; artifacts/bone-substring.json records
evidence/hashes. Original42ec80 model resolution and42eb20 fallback orchestration
still need integration with the live model owner; this helper alone does not
bind burn emitters to a character.

## Resolved burn bone fallback adapter

rf_burn_resolve_bones now joins42eb20 fallback order with shared51d690 bone
substring lookup over a resolved caller-owned bone-name list. It writes all
four indices and reports RF_NOT_FOUND if any remains absent, preserving
partial matches like original42eb20. The existing creation phase resets
all four indices when its attachment callback fails. Model resolution and
pose evaluation remain caller responsibilities; no allocation is introduced.

Executing42eb20 exposed an earlier documentation error: Ghidra string SYMBOL
labels had sanitized punctuation into underscores. The actual executable
strings are "lowerleg-l", "tech- leg-l-lower", "lowerleg-r",
"tech- leg-r-lower", "spine01", "spine03", "tech- 1spine",
"tech- 1spine01" and "head". Earlier prose and the substring fixture now
use these byte-verified literals. Never derive lookup strings from symbol names.

verify_burn_bones.py runs original42eb20 plus real51d690/strstr, supplying
string construction and the model resolution boundary. It covers all512
subsets of nine names under four ordering/case/prefix/duplicate variants.
All2,048 cases match PC and NXDK indices/success; every NXDK lookup query
order also matches the original. Independent substring tests pass2,048 cases
with corrected literal strings. Both builds and eight CTests pass. Evidence
is artifacts/burn-bones.json and bone-substring.json. Live model owner lookup,
attachment pose evaluation and persistent campaign damage ownership remain.

## Installed-model burn bone binding coverage

verify_burn_model_assets.py structurally walks every installed V3C BONE
section from the inventory and checks the actual names against original
51d690 search with verified fallback priority. The PC path uses the shared
BONE payload decoder followed by rf_burn_resolve_bones; the NXDK path runs
the actual linked resolver over the same names. All95 model bone sets match.
Thirty-one contain all four required bones;64 return the expected missing-bone
result. These are model-binding outcomes, not gameplay eligibility: creation
still has character/immunity/class gates, including the masako exclusion.

miner.v3c has25 bones and resolves indices11,12,15,8 respectively:
park-bdbn-lowerleg-l, park-bdbn-lowerleg-r, park-bdbn-spine03 and park-bdbn-head.
This validates the substring requirement against real prefixed names and the
spine03 fallback used by the current diagnostic miner. The report records all
model names, matched indices/names and payload hashes in
artifacts/burn-model-assets/report.json. Game assets remain untracked.

The file probe now supports --burn-bones after its payload path for repeatable
real-asset checking. This does not yet bind a live model owner or evaluate
animated burn positions; those remain separate integration requirements.

## Burn bone query from evaluated animated poses

rf_model_query_bone now exposes the position and3x3 basis from an already
evaluated bone matrix, matching the skeletal kind2 route503230 ->5012a0 ->
51c590 ->51b2e0 and the4fec30/4fec50 extraction helpers. Index-1 returns
identity; ordinary indices are bounded to the supplied count<=256. Nonfinite
matrices and invalid indices preserve the output and return an error. The
function allocates nothing and permits input/output aliasing through a local
copy. Lazy pose evaluation and virtual/attachment indices remain external.

verify_burn_pose_query.py executes that complete original route without hooks
using cached matrices. All520 valid cases match PC and actual NXDK bytes;
26 port guards preserve output. The valid set includes264 queries of the four
verified miner burn bones across33 time samples of each standing and crouching
motion, using the existing shared skeleton sampler. This joins actual bone
selection with current animated pose matrices, rather than bind-pose offsets.
Both builds and eight CTests pass. Evidence is artifacts/burn-pose-query.json.

This supplies the model query needed by a burn attachment adapter but does
not yet connect live emitter room relocation, particle updates or persistent
entity/audio ownership. It also does not verify lazy original animation
advancement through this query.

## Emitter movement boundary for burn attachments

rf_particle_emitter_move reconstructs4972a0: call the supplied4cd970 room
locator with previous room, previous position, requested position and flag0,
then commit returned room, position and direction in original order. Room0
is accepted as a successful miss. It does not normalize direction or skip
unchanged positions. Direction can alias the emitter's current direction,
as it does at burn attachment call sites. Parent transforms and4972f0 emission
remain subsequent operations; the locator sees the raw supplied positions.

The locator must retain input storage and not mutate it. Finite positions/
direction are required, and callback errors preserve emitter fields. This
explicit error return extends the original direct room-pointer return without
pretending a failed query succeeded. No allocation is introduced.

verify_emitter_move.py compares complete original4972a0 with its real copy
helpers against PC and actual NXDK code. The room locator is supplied;1,024
cases match exact room/from/to/flag arguments and committed fields, including
unchanged position, zero returned room and direction self-aliasing. Four
nonfinite/callback-failure guards also pass. Both builds and eight CTests pass.
Evidence is artifacts/emitter-move.json. Actual world-room traversal, parent
room synchronization and emitter emission still need to be joined to the
model/burn adapter under persistent campaign ownership.

## Cached room policy required by emitter movement

The existing geometry-world position locator covers4e1630, but it cannot
replace the complete4cd970 movement query indiscriminately. A null previous
room calls4e1630 immediately. Otherwise4cd970 computes squared displacement
through4faf00 and compares it to0. Only positive displacement invokes
4cd9e0(world,0,previous_position,position). A nonzero low-byte result then
permits4e1630; otherwise the cached room pointer is retained. The fourth
4cd970 argument is unused, and the crossing helper receives a null cached
tree even when a previous room exists. A fresh locator miss returns0.

verify_room_tracking_trace.py executes complete4cd970 with actual distance
code and supplied crossing/locator outcomes. All240 scenarios pass, covering
zero, unit, minimum-positive-float and NaN displacement; return words0,1,2,256,
257; missing/present old room; ignored flag values; and located/missing output.
There are156 full position queries and60 crossing queries. NaN retains the
old room in the original; the shared emitter move already rejects nonfinite
inputs explicitly. Evidence is artifacts/room-tracking-trace.json.

Disassembly of4cd9e0 shows a separate face-crossing traversal, including
node/face bounds, segment tests, plane fraction and point-in-face checks.
Its4ce4a0 predicate excludes face flags0x0c. This is not yet a verified shared
world traversal adapter. Recover and compare that query before connecting
moving emitters to the position locator; always re-locating would change
original cached-room behavior.

## Complete original cached-room crossing traversal

verify_room_crossing_trace.py now executes complete4cd9e0 and all geometry
callees without replacements on synthetic cube geometry. It covers1,500
queries over one-node/three-node trees, global root collection and explicit
room-root selection, all three axes and directions, zero movement, and face
flags0/4/8/12/16. All336 crossings agree with analytical off-face segment
intersections and the selected face belongs to the expected axis.
Evidence is artifacts/room-crossing-trace.json.

The original collects nonnull roots from world9c room entries (room3c), then
uses a stack; supplying a room skips collection and uses its root. Node/face
bounds compare against segment bounds expanded by float0.0001, with an
additional508b70 node segment test. Faces with flags0x0c are skipped. Plane
intersection uses508570, not the movement collision506550 helper: a zero
denominator rejects, otherwise a stored denominator feeds the plane fraction.
The traversal requires abs(fraction)<=float1.0001 (589cf4), then constructs
the point with separate multiply/add float stores and tests4e1f50 containment.
It returns at the first accepted face. This must not be replaced with a
generic nearest-hit segment query or different plane tolerance.

Current executable geometry coverage excludes on-face endpoints, oblique
planes, multiple room roots and real levels. Shared traversal implementation
and those boundary checks remain the next steps before cached-room binding.

## Shared cached-room face-crossing traversal

rf_collision_cross_rooms now implements4cd9e0 over caller-owned room/tree
views. It visits supplied roots in reverse order or one explicitly preferred
room, processes node faces before right/left children, ignores room skip
bytes, and skips empty root trees without inventing a flat-face fallback.
It preserves expanded segment bounds, node segment testing, face0x0c filter,
508570 denominator float store, abs(fraction)<=1.0001, and separate point
multiply/add stores before polygon containment. The result names the first
accepted room/tree-face pair; it is not a nearest-hit query.

The implementation uses existing tree scratch without allocation; calls must
be serialized. Bounds, indices, finite arithmetic and traversal limits guard
malformed input. Errors preserve output but may change scratch. Nonfinite
plane-fraction arithmetic returns RF_FORMAT explicitly. Prepared acyclic
trees and ordered valid polygon arrays remain caller requirements.

verify_room_crossing.py compares1,500 original cube scenarios with shared PC
and actual NXDK code. First room/face selection matches exactly; two nonfinite
guards preserve output. Both builds and eight CTests pass. Evidence is
artifacts/room-crossing.json. This establishes the shared implementation under
controlled geometry. Boundary/oblique/multiple-root/real-level tests remain
before connecting cached-room policy to the live world locator and emitters.

## Crossing boundary and angled-segment coverage

verify_room_crossing_boundaries.py extends the complete original/shared
comparison with3,506 cube queries. It covers on-face endpoints, edges and
corners, immediately adjacent float values around both boundaries, zero and
very short movement, and randomized angled segments under face filters.
The original geometry routines supply the reference; shared PC and actual
NXDK first room/face selection match exactly, including1,320 accepted
crossings. Two nonfinite-input guards also pass. The initial1,500-case
analytical suite still passes after the harness was generalized.

Reports are artifacts/room-crossing-boundaries.json and its original trace.
The geometry remains axis-aligned even when segments are angled. Oblique
planes, multiple world roots, and real-level geometry remain unverified;
these checks do not establish live cached-room integration.


## Retained real-level crossing evidence

verify_loaded_room_crossing.py reuses the verified loaded-room materialization
and executes complete original4cd9e0 against the linked NXDK crossing function.
L1S1, L1S2 and L1S3 each pass720 queries over120 sampled faces:2,160 exact
first-room/source-face comparisons, with1,701 accepted crossings. The samples
include255 faces with normals having multiple nonzero components. Each face
is tested across three segment lengths, both through all primary roots and
through its preferred room; the levels have31,35 and27 primary roots.
Existing loaded locator checks also pass for all three fixtures.

Reports: artifacts/loaded-room-crossing-L1S1.rfl.json (and L1S2/L1S3).
This extends coverage beyond cube geometry to actual retained level trees.
The reconstructed loader provides both inputs; the original loader is not
executed, and this harness does not run PC crossings or native XEMU gameplay.
Cached-room policy composition and live emitter ownership remain open.


## Shared cached-room tracking

rf_geometry_collision_world_track reconstructs4cd970 over the retained world:
missing room uses the world locator; a finite unchanged position keeps its room;
otherwise a qualifying crossing through all primary roots triggers fresh lookup.
The fourth flag is ignored. For finite float coordinates, coordinate inequality
preserves the original x87 positive-distance decision without SSE underflow.
The API uses UINT32_MAX for no room and preserves output on errors.
rf_geometry_collision_world_track_emitter adapts this to emitter tokens: index+1,
zero absent. Tokens must belong to the same world lifetime. Both paths allocate
nothing and share serialized tree scratch. Nonfinite coordinates are rejected
explicitly, unlike the original cached NaN path which retained its room.

verify_loaded_room_tracking.py compares complete unmodified4cd970, including its
real crossing and locator callees, with PC and actual NXDK implementations.
L1S1/L1S2/L1S3 each pass2,160 original scenarios, with both direct and adapter
results checked:6,480 original cases and12,960 function results per platform.
Missing rooms, stationary cached rooms, moving cached rooms and ignored flags
are included. Reports: artifacts/loaded-room-tracking-L1S1.rfl.json and peers.
PC and NXDK builds and all eight CTests pass. The original loader remains outside
this comparison. The adapter is available for emitter movement, but persistent
live effect ownership and native XEMU execution of this path remain open.


## Concrete burn attachment backend

rf_burn_attachments_resolved connects the recovered attachment sequence to
rf_model_query_bone, rf_particle_emitter_move with cached world tracking, and
rf_particle_emitter_update. It borrows an already evaluated pose, a fixed emitter
pool, retained world, stable parent view and shared RNG. Four distinct active
slot+1 tokens must have the supplied owner. It preserves emitter direction;
local bone coordinates reach movement unchanged, and emission performs its own
parent transform before the update's final parent-room assignment. No allocation
or animation advancement occurs here. Earlier moves/emissions survive a later
failure. This backend does not create/register persistent entity owners.

verify_burn_resolved.py executes original42ef3e..42f0bd (or its older-age skip),
including cached503230 model extraction,4972a0/4cd970 room traversal and complete
4972f0 phase/emission/parent-room update. Allocation and random routines execute;
only CRT thread-storage lookup is supplied. Fixtures use eight advanced miner
standing poses and four available particle nodes, varying age, owner, global
low-byte enable, timer deadlines, phase flag and room inheritance flag.
The first sampler output is an unevaluated zero cache, so live comparisons use
advanced samples; coincident-leg rejection remains the documented port guard.

PC and actual NXDK agree exactly on placement, all four compact emitter states,
four particle payloads and RNG across72 scenarios each on retained L1S1 and
L1S3 geometry. Each suite creates40 particles; the harness requires nonzero
emission. Three NXDK invalid-token/duplicate-token/owner-mismatch guards preserve
state. Reports: artifacts/burn-resolved-L1S1.rfl.json and L1S3.rfl.json.
Both builds and all eight CTests pass. This is CPU integration evidence, not
native XEMU gameplay: entity registration, burn resource creation/fade ownership,
live scheduling and simulation/rendering of these particles remain open.


## Fade writes into real particle runtime

rf_burn_fade_resolved binds the original six float writes directly to emitter
min/max velocity, min/max spawn delay and min/max radius. Original byte87 is
the high alpha byte of spawn.color at84; original140 is the low enabled byte.
Original4973d0 is exactly a single enabled-byte clear. The live adapter preserves
RGB and all upper enabled bytes and requires four distinct active slot tokens.
The old compact oracle API and live adapter now share one reference-based fade
implementation. No staging copy occurs, so owner callbacks observe earlier
writes and release can invalidate record and slots without a write afterward.
No heap allocation is introduced. Owner lookup/reaction/release remain supplied.

verify_burn_fade_resolved.py compares8,192 full original42f2f0 scenarios with
actual4973d0 against PC and linked NXDK. All four complete compact runtimes,
record, flags and owner callback order match exactly; untouched bytes are checked.
The original trace has a separate actual-stop mode. Earlier8,198 compact fade
checks and540 composed retirement scenarios (2,082 retired records) still pass.
Both builds and eight CTests pass. Evidence: artifacts/burn-fade-resolved.json.
This connects fade to runtime emitter state; real resource release, persistent
NPC ownership and live scheduling still need integration before gameplay proof.


## Burn emitter resource release

rf_burn_release_resolved connects42ed20 to actual shared emitter stop/release.
It clears only the enable low byte, detaches existing particles in list order,
returns each emitter slot to the free-list tail, then continues the existing
voice-stop, payload-clear, owner-clear and burn-ring sequence. Zero emitter
slots are skipped; nonzero slot+1 tokens must be distinct and active. The adapter
checks status-producing pool/token preconditions before callbacks and inherits
the emitter pool's intact-list ownership contract. No allocation is introduced.
Audio stop and owner-reference clearing remain external callbacks.

verify_burn_resource_release.py executes complete original42ed20,4973d0,497d80
and497230 against PC/NXDK across256 emitter-subset/rotation/reset-mode scenarios.
Each starts with eight attached particles and one already detached particle.
Complete particle payloads, ordered detached lists, all128 emitter link/enable/
active states, burn payload/rings and particle/emitter counts match. Surviving
particles remain alive; emitter release is not particle destruction. Audio stop
is supplied and the original entity/other owner lists are empty sentinels;
shared owner-clear invocation is observed. Existing437 pool checks and eight
CTests pass, as do both full builds. Evidence: artifacts/burn-resource-release.json.
Real audio backend and persistent NPC ownership/scheduling remain open before
this resource path can be exercised in live XEMU campaign gameplay.


## Owned class damage factors (2026-09-11)

rf_entity_damage_factors_read loads all eleven class multipliers from entity.tbl
and rf_entity_seeds_open retains them once per loaded class. The original entity
loader41bd0d..41bd7c initializes class+13e8 through+1410 to1.0, then overwrites
named entries in authored order. Damage41a350 reads class+13e8+kind*4; kind-1
bypasses scaling. Disassembly confirms the class loader and consumer offsets.
The similar common-object loader40f72b was inspected first; the final oracle
executes the entity-specific41bd0d block.

Original48ab50 searches pointer table59f7b4 with57c130 case-insensitive matching.
Its first nine names are bash, bullet, armor piercing bullet, explosive, fire,
energy, electrical, acid and scalding. Slots9/10 have null name pointers, not
empty strings; they still receive default1. Unknown/empty names are rejected
by the port instead of following the original invalid-pointer path. Repeated
valid entries use the last value. Finite negative values are not clamped.

All63 installed classes match original numeric initialization and override
stores on both PC and compiled NXDK. There are89 overrides across54 classes;
miner1 and env_guard both scale armor-piercing damage by1.5. Two additional
cases check default-only and mixed-case duplicate overrides. Six port-only
malformed/unknown/nonfinite cases preserve all44 output bytes on PC/NXDK.
verify_entity_damage_factors.py supplies parser token/string/float boundaries,
but executes original class writes, string-wrapper lookup,48ab50 and57c130.
It compares all11 factors and the complete prepared0x1514-byte original class
record, ensuring unrelated original fields remain untouched. This is not a
claim that the original tokenizer itself has been executed.

Owned seed-class residency increases44 bytes per unique class, already counted
in the existing seed resident/peak budget. Table scratch is reused; no new
per-actor factor copy or allocation. Three opening PC replays retain their
body/support results. Both builds and all nine CTests pass. Stock64MiB XEMU
passes180 door frames: artifacts/xemu/replay-20260911-132919/report.json, with
base RAM67108864 and plugged memory0. The native replay exercises class loading;
the direct NXDK oracle checks every factor value. No new visual capture.

Actual NPC damage dispatch still needs persistent damage-state ownership and
pain/death/burn/AI effects. The retained factors should feed rf_entity_damage_sp
rather than an assumed1.0 multiplier; loading them alone does not change health.


## Persistent NPC damage state (2026-09-11)

Skeletal campaign owners now retain rf_entity_damage_state directly. Health and
armor have moved out of the temporary creation-vitals projection into this one
persistent record. Object flags and opaque840 remain separately owned; a local
creation-vitals value is used only during initialization and legacy digesting.
The previous health/armor copy is not retained. This gives future synchronous
damage callbacks the same persistent record consumed by rf_entity_damage_sp.

The state retains class health/armor, class728 flags from physics.flags2, the
registered generation handle and authored friendliness as affiliation. The latter
is loader4647ef..46483c's entity1f8 overlay, the field compared in the burn path.
Voice854 and responsible144c start atUINT32_MAX as assigned by422360; burn13d8
starts absent. burn_source is a port sentinel until a burn owner is installed.
Damage time and810/814 remain zero in this prepared startup projection; complete
factory/AI initialization is not newly proven here. No damage request is fired
by constructing this record, and no effect backend has been replaced by a stub.

Replacing the16-byte vitals member with56-byte damage state plus8 bytes for
object flags/840 adds48 bytes per actor slot. Live Mines adds3744 bytes, L1S2
1872 and L1S3 1344. Body-owner resident/peak values are45348/419988,
23232/397224 and16744/390448, within the existing512KiB owner setup budget.
A three-word diagnostic records count, added bytes and complete56-byte state
hash. The opening hashes are199578601,3714119201 and3553662776. These are port
startup ownership checks, not an original full-constructor equivalence claim.

The three-level support verifier still matches all prior body-content hashes,
which include the original creation health/armor/flags/840 digest. Independent
original/PC/NXDK factory-vitals tests pass1536 cases; the original authored scalar
overlay test passes200 cases. Both builds and all nine CTests pass. The next
integration must resolve the registered owner, supply retained class factors,
keep predicate flag views coherent during callbacks and connect real pain,
death, burn, sound and AI effects before claiming live campaign damage.

Stock64MiB XEMU also matches all78 Live Mines damage records through their full
state digest over180 door-replay frames: [78,3744,199578601]. Evidence is
artifacts/xemu/replay-20260911-133623/report.json, base RAM67108864 and plugged
memory0. This is persistent-state/layout evidence; it does not exercise a hit.
No new screenshot was taken because rendering behavior is unchanged.


## Registered NPC damage dispatch (2026-09-11)

rf_scene_npc_damage now connects a generation-checked skeletal NPC owner to
rf_damage_dispatch_sp (original4892c0) and rf_entity_damage_sp (41a350). It uses
the retained class factor for kinds0..10 and bypasses it for-1. Class724 flags,
current armor and814 feed armor immunity. Player selection uses the current
single-player list and linked actor, refreshed after delegated effects. The
outer dispatch's touched flag is committed to object/predicate ownership before
callbacks; inner health and810 changes are refreshed before final outer cleanup.
Health is retained in one damage record. A transient generic-object projection
exists only for the outer dispatcher and is committed synchronously.

The API requires a complete synchronous effect backend and stable owners.
Unknown/stale targets succeed with zero result and no owner mutation. Invalid
kind/backend/nonfinite request guards precede damage; errors after callbacks do
not roll back committed state. It does not create pain/death/burn/AI effects.
General callback/reentrant actor lifetime remains constrained by the existing
shared damage APIs; no callback may destroy the active owner.

An explicit two-hit fixture targets authored guard8456 in Live Mines. It first
checks a mismatched generation leaves the complete damage record and object
flags intact. Then it submits amount10/kind2 and amount10/kind-1, with source and
auxiliary UID absent, force0 and clock bits3f800000. The first hit scales to15;
the second remains10. Health/armor change from100/100 to92.800003/92.199997,
then88/87. Six pain-animation, pain-sound and AI notification boundaries are
counted only. Other downstream callback attempts fail the fixture; no fake
sound, burn owner or AI reaction is installed. Ordinary runs do not fire hits.

PC enables this only with RF_REPLAY_DAMAGE_UID. The Xbox replay harness accepts
--damage-uid and stages a four-byte campaign-damage.bin, restoring the previous
file afterward. The shared fixture records64 words: status, UID, handle,
notification count, initial14-word damage record, object/class flags,11 factors,
two16-word post-hit records (damage state/flags/result), and unexpected-callback
count. It adds260 static bytes including the disabled-by-default UID; no heap
allocation or persistent second health copy. These diagnostics are test input,
not a weapon, collision, event or campaign damage source.

verify_npc_damage_binding.py executes full original4892c0 and41a350 on the
observed class/actor state, with real object lookup, typed entity lookup, armor
immunity and armor arithmetic. Only unselected-player predicates and downstream
pain/AI calls are supplied. Both complete post-hit damage records, flags and
float results match PC. It observes six original notification calls; ordering
inside the shared effect helper is covered by its existing independent oracle.
This fixture does not prove lethal, burn, electrical, selected-player or general
callback mutation behavior of the scene adapter.

Both builds and all nine CTests pass. Ordinary three-level support and four-level
backlink checks also pass with the fixture disabled. Native stock64MiB XEMU
matches all64 damage-test words over180 door frames: artifacts/xemu/replay-
20260911-134509/report.json. Final health88/armor87 and six notification boundaries
match PC; base RAM67108864, plugged memory0. The staged damage file is removed
by restoration. This proves an explicit test hit on the registered owner, not
live weapon/combat gameplay. No new visual capture was taken.


## Pain-animation orchestration (2026-09-11)

rf_entity_pain_react reconstructs full428740 with caller-owned query/effect
boundaries. It queries selected-player mode only when needed, then cooldown830,
excluded810 bit0, AI state, the idle-to-ready mapping and active fire actions2/3.
A retained action828 wins; otherwise combat readiness byte exactly1 selects
flinch_attack_stand23, and every other byte selects flinch_stand22. An unmapped
action causes no effects. Bounds guards reject an invalid retained action.

Disassembly and original execution exposed an important alias: entity+c14 is
entity+a54+28*16, the idle_to_ready action mapping. It is not an independent
fire-motion field. The C state reads motions[28] directly. AI-enabled query408e90
reads entity+7d0 bit0; blocked query408ef0 reads its bit100. Other owner queries
remain supplied, including408ec0's timer path and428d10's playback lookup.

Accepted order is41ae70(handle,AI value), store chosen action828,
428c90(entity,action,1,0,1), cooldown4fa3b0(1000,2000) at830, current mapped motion
duration5033e0, then4fa360(trunc(duration*1000+0.5)+250) at744. The duration query
rereads model and chosen mapping after the earlier callbacks; changing828 during
start does not change the local chosen index. The backend owns actual AI reset,
action/sound start, timer/RNG and duration lookup. The wrapper allocates nothing.

verify_pain_reaction.py executes complete original428740 and real573528 integer
conversion while supplying those owner query/effect boundaries. All2048 cases
match PC and compiled NXDK, with871 accepted starts. Complete compact state and
ordered query/effect arguments match, including short-circuiting, low-byte zero
versus exact-one distinctions, custom/missing action mappings, fractional timing
and state mutation during begin/start/cooldown/lock callbacks. Double duration
preserves the original53-bit x87 timing calculation until integer conversion.
Finite representable conversion guards are port behavior; errors after effects
do not roll back earlier mutations.

Both builds and all nine CTests pass. This wrapper is not yet invoked by the
campaign damage callback, so no additional XEMU run or visual capture is claimed.
The existing871 positive oracle cases are prepared owner conditions, not proof
that a startup guard's real AI/cooldown gates currently permit a flinch. Connect
those owners, action22/23 resource residency, rf_motion_start_action and sound
ownership before enabling a visible pain reaction. The action tables and shared
action-start helper already exist; they should be reused rather than selecting
an animation directly and bypassing the recovered gates.

## Pain timer prerequisites (2026-09-11)

rf_timer_pending reconstructs408ec0 with the existing expiry helper: a disabled
deadline never blocks AI, while an active, unexpired deadline does. This belongs
to AI+274 (entity+514), separately from pain cooldown830 and animation lock744.
rf_timer_set_random reconstructs4fa3b0 using one shared CRT RNG draw, an inclusive
integer modulo range, and the existing4fa360 timer setter. Equal endpoints still
consume a draw. Both helpers allocate nothing; callers own deadlines and RNG
ordering. Invalid inputs preserve output and RNG state.

verify_pain_timers.py compares2048 pending checks and2048 random timer resets
against the original executable and both PC and linked NXDK code. Original
40a0d0/4fa3f0 and57312d/4fa360 execute directly; only the CRT thread-storage
address is supplied. Complete original object bytes are checked for unexpected
writes. Coverage includes disabled timers, wraparound, half-period boundaries,
equal endpoints and negative ranges. Eight range/clock guards pass on both
targets; four null/alias pointer guards additionally pass on linked NXDK code.
Report: artifacts/pain-timers.json. Both builds and all nine CTests pass.

These are prerequisites for the live pain backend, not evidence of a campaign
flinch or established shared RNG call ordering. No additional XEMU run or visual
capture is claimed for these currently unconnected routines.
