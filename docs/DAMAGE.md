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

Raw42eb20 attachment lookup tries `lowerleg_l`, then `tech__leg_l_lower`;
`lowerleg_r`, then `tech__leg_r_lower`; and `spine01`, `spine03`,
`tech__1spine`, `tech__1spine01` in order. Its fourth name and42ec80 lookup
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
