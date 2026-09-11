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
