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
