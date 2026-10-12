# NPC Laser and Sonar paid-flight persistence

Source-written against `5ff931bc6c6cd6c2b173050bfe82e0a3b0ae181e` in the external
implementation directory. Compilation, actual saving/loading and resumed
attacks are unverified. No tests, fixtures, builds, emulator runs, images, HDD
operations or authoritative-repository edits were performed. Parent owns
integration and the hourly build; feature verification belongs to testers.

## Implemented scope

The ordinary composed world save retains already-paid Meca Laser and Sea
Creature Sonar flights. Each independent 64-slot pool preserves slot, source,
position, velocity, remaining double-precision lifetime, render basis, query
flags and accepted direct-damage properties. Restore neither consumes ammo nor
repeats launch, cadence, a forced-fire request or damage. These two immediate
families share the existing direct-flight implementation but retain separate
pools and profiles.

Other NPC, player, vehicle and world admission remains in force. This closes
one omitted-flight family; it does not claim unrestricted combat saving. A
removed/retired source has no historical-source receipt and still refuses.
A source that changed weapons or became an admitted registered corpse can keep
its independently paid flight.

## Original evidence and current ownership

The existing source reconstructions in `MECA-LASER-FIRST-PASS.md` and
`SEA-CREATURE-SONAR-FIRST-PASS.md` establish this implementation's inputs:

- Installed Laser: bullet kind1, raw damage80, speed20, radius0.2, life20s,
  fire wait0.4s, clipless power-cell reserve200, zero damage radius and no delay.
- Installed Sonar: energy kind5, raw damage80, speed20, radius0.5, life3s,
  fire wait2.5s, clipless 12mm reserve200, zero damage radius and no delay.
  Accepted liquid weapon flags are0x10600, independently of per-flight queries.
- RF.exe `4257c0`, including `42580e..425827`, owns clipless reserve debit.
  `425b89..425c0c` does not establish ordinary SP NPC exhaustion refusal; the
  existing finite-positive-reserve port policy is preserved.
- `42d040 -> 4c8770` reads the NPC-primary spread descriptor. The port retains
  its admitted zero AI-spread default, without deriving it from undeviating.
- `4c4ea1..4c4eb7` terminates a torpedo at an admitted liquid contact. An active
  Sonar row therefore retains0x1004; Laser may retain0x1004 or post-entry4.
- `42c3c0` starts nonfreezing fire presentation. The independently verified
  mapped action2/playback can outlive the instantaneous attack. Exact Laser
  and Sonar now join the existing residual-clip admission with clipless,
  zero-loaded inventory and a bounded real reserve including zero. They gain
  no action39/reload ownership or unqualified nonloop admission.

This patch serializes the implemented accepted payload, not the original
save-file format or an invented late descriptor/source lookup. Existing direct
contacts, shield/Nano priority, difficulty, liquid stepping and expiry remain
unchanged. Flight source is independent of the current selected weapon.

The remaining refused families were kept separate for concrete ownership
reasons: TriBeam, Spit and Cane combine paired reserved slots with prepaid
windup/deadline, due-time aim and cancellation owners; Drone/Tankbot secondary
fire has an independent cooldown bank; Drone's actual missile is straight,
not homing. Those owners cannot be encoded merely by relabeling their released
flights as Laser/Sonar. Delayed melee and robot-death consumers also stay guarded.

## RFAP7 wire format and compatibility

RFWC3, the16-byte RFAP header and112-byte row size are unchanged. Version7 is
written only if a Laser or Sonar flight exists. Kinds1–12 preserve their old
layouts and RFAP1–6 preserve their previous semantics and reserved-byte rules.
Both the full reader/resource preflight and the early shared-charge preflight
accept version7; no old fractional-debt or Flame equality rule is relaxed.

Kinds13(Laser) and14(Sonar):

- +0 kind; +4 independent pool slot0–63; +8 positive authored source UID
- +12 reserved zero
- +16 position3×float32; +28 velocity3×float32
- +40 radius float32; +44 per-flight liquid-query flags
- +48 remaining float64; +56 accepted raw damage float32
- +60 damage kind; +64 accepted liquid weapon flags; +68 reserved zero
- +72 render basis9×float32; +108 reserved zero

The kind resolves the exact independently admitted catalog weapon. No runtime
handle, pointer, particle/native audio ID, mutable selection or inventory grant
is stored. Admission verifies resource presence, loaded profile, exact damage,
kind/radius/flags, finite bounded position, positive bounded life, orthonormal
right-handed basis and straight velocity/basis agreement. Actual accepted float
bits survive; tolerances only allow ordinary normalization roundoff. Duplicate
slots are rejected independently for each64-slot pool without32-bit bit shifts.

## Source and candidate joins

Capture reuses RFAP6's metadata-independent source qualifier: full NPC
registration/entity/damage/body identity, authored positive UID, persistence
slot, current level and nonretired history. This does not demand Fusion resources.
Preparation resolves the UID but cannot use a pristine constructor alone as
permission to restore. Each staged flight must join exactly one candidate RFNC
entry with the same UID, full handle and authored slot, registered and nonretired.
The existing subsequent RFNC validation still seals source lifetime/registration.

Only composed export receives the capability. Standalone NPC, RFCP and older
composed exports retain active-Laser/Sonar refusal. Existing guards for all
other pending attack families are unchanged. The ordinary on-foot and supported
seated composed-player capture paths admit this same complete RFAP owner.

## Acceptance and callback boundary

A small common fire adapter invokes the existing immediate Laser/Sonar launch,
which prepares/debits/publishes without callbacks. It computes cadence privately
before launch and publishes accepted cadence plus forced-fire FIFO consumption
before action/audio presentation. A full pool consumes neither ammo, spread RNG,
accepted cadence nor a once request; the parent's existing short retry remains.
No old source/FIFO pointer is accessed after presentation returns.

A stack-owned service counter covers this acceptance/presentation interval;
Laser and Sonar update flags cover all contact callbacks, even after the final
flight retired. World reset never clears an executing service counter. Captures
refuse these partial endpoints, while restore preparation remains free of live
pending-state gates.

## Publication, visuals and resource demand

The new pools remain staged until final world/storage success. Existing
successful-load resets retire the outgoing transient pools and bump their epochs;
only then an assignment-only finish copies the admitted flights and live counts.
Absent/legacy RFAP rows leave the new pools empty. Failed preparation or storage
close never publishes these new pools. This is a narrow ownership guarantee,
not a claim of general whole-world rollback for older components.

Saved kind13/14 rows set NPC-only resource bits30/31. Fresh scene reconstruction
loads the exact Laser mesh or Sonar PART even when the current constructor NPC
is dead, changed weapon, or otherwise no longer demands that visual. These bits
never enter player weapon/view demand and grant no inventory. The normal strict
resource loaders and their byte budgets remain unchanged.

Laser's visual phase already derives from retained remaining lifetime. Sonar's
cosmetic particles, emission credit and private presentation RNG are intentionally
not serialized: final-success publication binds empty emitters at the resumed
flight endpoint. The previous particle trail is discarded and new particles
resume on subsequent ticks. This callback-free, allocation-free rebind does not
increment accepted-attack counters, restart launch audio or affect gameplay RNG,
damage, flight lifetime or paid ownership. Exact cosmetic continuity is deferred.

## Bounds and remaining implementation

Maximum RFAP rows grow from154 to282; maximum wire component grows by14,336 bytes
to31,600 bytes. The existing whole-file and2MiB restore-stage budgets still apply.
Temporary staging adds128 existing projectile structs,128 source UIDs and two
counts: `128 * sizeof(scene_ai_laser_projectile) + 520`, subject to compiler
alignment, roughly14–16KiB. No live flight pool expands; only the4-byte service
counter is new permanent gameplay bookkeeping. Actual compiler/stock64 layout
is not measured by this code-only pass.

Still missing: historical removed-source receipts and independent delayed
attack/secondary-fire checkpoint ownership. Existing unsupported actor motion,
script and other world conditions remain honest refusals. Cosmetic save parity,
feature verification and broader campaign coverage remain tester/refinement work.
