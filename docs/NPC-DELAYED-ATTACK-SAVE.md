# NPC delayed-attack checkpoint ownership

Source-written against `6d2ffd0e5a8ec273dc69472be8aeb0f7b72c0235` in the external
implementation directory. Compilation and gameplay are unverified. No builds,
syntax checks, tests, fixtures, emulator runs, images, HDD operations or
real-repository edits were performed. Parent owns integration and hourly
compile/package rollups; functional coverage belongs to testers.

## Implemented bounded scope

Ordinary composed saves retain the implemented TriBeam, Small/Big Rock Snake
Spit and Capek Cane attack owners, including:

- Phase2 prepaid windups, exact paired reserved flight slots and remaining
  millisecond deadlines, including hidden-overdue deadlines.
- Already-debited inventory, remaining fire/navigation cadence and shared
  spread RNG, joined to the candidate NPC inventory and existing RFNC owners.
- Released flights, immutable accepted damage/kind/liquid/radial payload,
  source identity, remaining double-precision life, velocity and render basis.
- Cane's unacquired scan deadline or retained player/NPC homing target role,
  remapped by authored identity without rerunning acquisition.
- Existing source cancellation and due-time aim consumers. No release, hit,
  debit, cadence start, forced request or launch presentation is replayed.

The source-controller receipt remains present for living selected delayed
actors with combat awareness after their projectile has retired. This matters
because ordinary RFNC otherwise restores a new30-frame fire deadline, clears
navigation timing and clears burst state. The receipt also covers a remaining
windup after a target/order change clears awareness. It does not claim generic
ordinary NPC combat-state persistence for other selected weapons.

This capability is limited to composed RFAP ownership. Standalone RFNC, RFCP
and old composed export paths keep their refusal. Existing world, movement,
actor, player, script and unsupported attack guards remain in force.

## Original evidence and unchanged consumers

The underlying implementations and provenance are documented in
`SPIKE-TRIBEAM-FIRST-PASS.md`, `ROCK-SNAKE-SPIT-FIRST-PASS.md`,
`BIG-ROCK-SNAKE-SPIT-FIRST-PASS.md` and `CAPEK-CANE-FIRST-PASS.md`.
`NPC-DIRECT-FLIGHT-SAVE.md` explains why these owners cannot be serialized by
merely relabeling their released flights as immediate Laser/Sonar attacks.

RF.exe426197..426273 owns nonmelee acceptance, action2, fire cadence and
actor+4c0 delay.4257c0/42580e..425827 debits the clipless reserve.409340 checks
the delay,4093aa clears it before factory4c77a0. The original due path does not
recheck ammo, refund, repeat range/LOS or retry a paid factory release.

TriBeam retains current eye-basis launch through409557..40956b: damage225,
energy5, speed30, radius0.2, life20s, cadence5s and delay2500ms. Spit's actual
drools-slime classes use primary_1/current-pose/current-target aiming through
41b350..41b4c0. Small Spit has damage80/AP2, speed30, radius0.5, life10s,
cadence3s, delay1000ms and no radial damage. Big Spit independently has
cadence2.5s, delay800ms and radius3/kind3 radial damage after direct contact.

Cane uses actual held-model muzzle41b040/41b25d..41b5a0 with current eye basis;
damage225/energy5, speed20, radius0.5, life2.5s, cadence3s and delay1500ms.
Its4c6dc8..4c6e83 scanner,200ms cadence,4c6f60 retained full-handle target and
4c6fca..4c7064 renormalized steering remain unchanged. A just-acquired target
still first steers on the following update. Missing targets remain locked and
ballistic in gameplay; a save cannot invent a replacement target.

All three reuse real reserve200 with zero magazine. Their exact profile
loaders remain strict. Finite positive-reserve onset and paired-slot allocation
are documented port policies, not claims about retail save bytes or retail
single-player exhaustion behavior. Small/Big Spit class identity remains exact.

## RFAP8 wire format

RFWC3 and the16-byte RFAP header/112-byte row width remain unchanged. Version8
is emitted only when at least one delayed controller or flight exists. Kinds1–14
retain their old layouts; RFAP1–7 retain previous semantics and reserved-byte
rules. Both full resource preflight and the early shared-charge preflight admit
version8 without weakening old Flame/debt/equality checks.

Kind15 is one source-controller row:

- +0 kind15; +4 compact unique controller ordinal0–95; +8 positive source UID
- +12 family0 TriBeam,1 Small Spit,2 Big Spit,3 Cane
- +16 paired pending slot, or UINT32_MAX when no windup
- +20 signed remaining windup milliseconds; zero when no windup
- +24 fire cadence remaining frames; +28 navigation remaining frames
- +32 burst state, required zero for these exact single-shot profiles
- +36 shared spread RNG; +40 paid reserve snapshot for the RFNC equality join
- +44 combat awareness; +48 combat mode0 ordinary,1 authored,2 reactive
- +52..111 reserved zero

No inventory is granted from the reserve snapshot. It must equal the candidate
RFNC inventory. Fire/navigation bounds are300/15 frames. Millisecond remainders
are signed within the timer half-period and cannot exceed the exact family delay;
negative hidden-overdue values survive rather than restarting the delay. Pending
start_frame is diagnostic only and rebinds to the restored frame. Runtime tickets
and handles are never serialized.

Kinds16–19 are TriBeam, Small Spit, Big Spit and Cane flights:

- +0 kind; +4 independent pool slot; +8 positive source UID; +12 reserved zero
- +16 position3×float32; +28 velocity3×float32
- +40 radius; +44 query flags; +48 remaining float64; +56 accepted raw damage
- +60 damage kind; +64 accepted liquid weapon flags; +68 blast radius
- +72 render basis9×float32; +108 blast kind

TriBeam has64 paired slots. Both Spit variants share one16-slot bank. Cane has
its own16-slot bank. Duplicate flights, duplicate pending source identities,
duplicate paired reservations and windup/flight collisions are refused.
Finite bounded state, loaded exact profile, accepted damage/radial/query
properties and right-handed orthonormal basis/velocity agreement are checked.
Float bits are retained; tolerances cover ordinary normalization roundoff.

Kind20 is required exactly once for every Cane flight:

- +0 kind20; +4 Cane slot; +8 same source UID; +12 reserved zero
- +16 target role0 unacquired,1 player,2 authored NPC
- +20 target UID for role2 only; otherwise zero
- +24 signed scan remainder for role0 only; otherwise zero
- +28..111 reserved zero

The unacquired scan remainder cannot exceed200ms. A retained target never
reacquires during restoration. Role2 resolves the full candidate handle and
authored slot. Registered hidden/dead targets may remain eligible because the
existing steering consumer still reads their body position. A removed/retired
source or target lacks a historical identity receipt and still refuses.

## Source, candidate and callback boundaries

Capture reuses RFAP6's metadata-independent full registration/entity/body/damage
identity and current-level authored nonretired history. Flights may outlive a
weapon switch or source death while that authored registration remains admitted.
Each incoming flight and each source controller must join exactly one complete
candidate RFNC entry with the same UID, full handle and authored slot.

Controller joins additionally require the living selected owned weapon,
zero-loaded/finite reserve equality, candidate awareness/mode, absent reload,
exact Spit class, and admitted muzzle resources for a pending Spit/Cane release.
Existing scripted combat cadence and all RFNC combat/queued-shot/guard RNG owners
must agree. RFAP8 cannot omit a currently selected living alerted delayed actor's
controller receipt and silently fall back to RFNC's constructor cadence.

Phase1 presentation reservations and servicing/presentation/update callbacks
remain save refusals, even after the final flight retires. Existing flags survive
world resets until their executing caller returns. Preparation does not inherit
the live pending-save guard. Existing point/once/vehicle-target onset refusal is
unchanged; these delayed consumers do not gain forced-fire admission.

Fresh monotonic tickets are budgeted before publication and assigned only after
final storage success. Whole-world load is the existing serialized frame-zero
operation. Restored tickets cannot equal a suspended outgoing callback's ticket.
The complete new delayed stage remains private until outgoing transient resets
have retired the previous timeline; final publication is assignment-only. Failed
preparation or storage close does not publish these new pools/controllers. This
is a narrow new-owner guarantee, not retroactive whole-world rollback for older
save components.

## Resource and cosmetic demand

NPC-only saved resource bits27/28/29 demand TriBeam/Spit/Cane flight visuals.
Bit26 separately demands the actual Cane held-world model for a selected source
controller. This covers constructor weapon changes without loading a held muzzle
merely because a dead/switched source has an independent Cane flight. The bits
never enter player view demand or grant ownership. Existing strict loaders,
256KiB weapon-model budget and optional Spit/Cane visual budgets remain unchanged.

Empty Spit/Cane cosmetic emitters bind at the resumed flight endpoint without
incrementing accepted-attack counters. Old particle history, emission credit,
private cosmetic RNG and Cane flare phase are not serialized. Cane's old flight
loop is retired and is not automatically restarted by load. Pending release
still starts its ordinary new flight loop when actually due. These are cosmetic
continuity limits; gameplay RNG, paid ownership, lifetime, target and damage
remain separately retained.

## Stock64 and whole-file limits

The structural RFAP maximum grows from282 to490 rows:96 bounded controllers,
96 combined flight slots and16 Cane target rows are added. Maximum component
size grows from31,600 to54,896 bytes, an increase of23,296 bytes. This is a
component grammar bound, not a claim that the largest component fits every world.

The whole-file cap remains110,524 bytes and the restore-stage cap remains2MiB.
The parent's actual18 fresh-world record was107,936 bytes, leaving2,588 bytes.
One new pending source needs a single112-byte row, including its controller;
first-component overhead adds16 RFAP bytes and12 RFWC3 prefix bytes, totaling
140 bytes before changed RFNC animation lengths. A current-source released
TriBeam/Spit flight plus controller needs252 bytes with that overhead; Cane
needs364 including its target row. Those individual additions fit that reported
headroom, but many simultaneous owners/flights or larger other components can
still correctly refuse saving. No file or stock64 memory budget was enlarged.

Temporary stage growth is:
`96*sizeof(scene_ai_laser_projectile) + 96*sizeof(scene_ai_delayed_checkpoint_owner)
 + 96*4 + 16*sizeof(scene_ai_cane_homing) + 16*4*4 + 10*4`, plus ABI alignment.
The declared owner is52 bytes and the homing record12 bytes. This is roughly
16–17KiB depending on projectile/double alignment. No live attack pool expands
and no new permanent gameplay state is allocated. Actual Xbox layout/peak use
has not been measured by this source-only pass.

## Remaining implementation

Removed-source/removed-target historical receipts, other NPC secondary and
delayed-melee owners, robot-death consumers and unrelated guarded world states
remain outside this bounded slice. Dense-world checkpoint capacity remains a
separate implementation constraint. Cosmetic fidelity and functional coverage
are deferred to subsequent refinement and testers.
