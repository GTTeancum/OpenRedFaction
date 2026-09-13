# Firing runtime integration

This is an implementation map derived from the original executable, not evidence
of playable combat. Source SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

## Verified building blocks

The port has retained NPC inventories, selected animation mappings, held weapon
models/materials, hand tags, muzzle placement and target aiming. The standalone
4257c0 ammunition helper matches original execution. These services do not yet
constitute a firing dispatcher, and muzzle queries must not consume ammunition.

## Dispatcher425830

The original dispatcher gates firing on actor state, current weapon, alternate
mode eligibility, timer state and inventory availability before creating shots.
The full function contains ordinary, continuous, special-weapon and network
branches; reconstructing only a normal pistol branch will not complete it.

The ordinary shot-loop trace establishes these integration requirements:

- Advance primary hand510 and wrap against the primary hand-list count before
  asking41b040 for the shot pose. Class flags can request multiple hands.
- Descriptor7c supplies the projectile count per hand; actor814 bit1000 doubles
  this count. Spread can change each projectile direction through RNG.
- Factory4c77a0 can be called again for a linked projectile. Preserve the actual
  dispatcher predicates rather than treating every factory call as one ammo unit.
- The ordinary post-creation path reaches4257c0 after shot processing. Flag814
  bit1000 can cause a second consumption call. Factory failures and special
  branches require separate execution tests before composing this ordering.

These statements are instruction/decompiler trace findings. Full dispatcher
execution and failure-prefix comparisons remain required.

## Factory4c77a0

`tools/audit_projectile_factory.py` decodes583 instructions over4c77a0..4c8030,
checks the original hash, and records direct calls and a reviewable instruction
listing in artifacts. Its AUDITED result proves static boundary presence and
address order only, not conditional runtime coverage.

| Call site | Callee | Required service |
|---|---|---|
|4c7a11|486da0|Generic object creation: kind2, owner handle and prepared descriptor|
|4c7ae4|4c8030|Projectile model setup when an object model exists|
|4c7b71|4d8ed0|Optional dynamic light|
|4c7bc4|497ca0|Optional attached effect|
|4c7bfd|5056a0|Optional positional sound|
|4c7c9b|48c9a0|Collision registration under the original gates|
|4c7eb3|4c2570|Special projectile setup|
|4c7fc8 /4c7fd6|49afe0 /49a420|Immediate model/ordinary collision branches|
|4c8000|4c4b50|Projectile hit processing|
|4c8006|48ab40|Mark object7c bit2 for later retirement|

Creation starts with a zeroed152-byte descriptor and baseline flags80000870;
weapon fields and predicates modify it before486da0. Allocation failure returns
zero after descriptor cleanup. Successful creation publishes weapon ID298,
descriptor pointer294, source-related flags and position, then conditionally
creates resources and links into the projectile list. Every optional owner needs
matching cleanup; a visible mesh alone is insufficient.

Descriptor264 bit20 reaches an immediate collision branch inside the factory.
After processing,48ab40 sets flag7c bit2. **It does not remove the registry entry
or free the object.** The factory subsequently returns its object pointer. Keep
creation, marking and eventual removal distinct; do not free this object before
the dispatcher's remaining accesses. The existing48ab40 export confirms the
single-bit mutation; actual cleanup scheduling must still be connected.

## Next implementation order

1. Reconstruct and verify the152-byte creation descriptor and generic kind2
   ownership, using retained weapon fields and the existing collision registry.
2. Reconstruct the full factory with explicit optional resource ownership and
   rollback/marking behavior; test immediate-hit and persistent projectile paths.
3. Connect the dispatcher to muzzle/aim, spread RNG, factory, ammunition,
   animation, timers and sound in the original order, including failure exits.
4. Verify a representative shot end to end in stock64MiB XEMU: registry state,
   target damage, ammo delta, timers and cleanup, then present a new visual.

Outstanding scope also includes live player/clutter targeting, replacement-class
hand lists, all campaign weapons and continuous projectile simulation.

## Creation descriptor verified (2026-09-13)

rf_projectile_descriptor_prepare now builds the full152-byte descriptor and
boost decision at486da0. It preserves name-length>4 selection, model token,
raw descriptorbc/ac fields, position/basis, scaled forward velocity, special
spin and flags. Important boundaries: factory accepts weapon==weapon_count,
but4c90f0 spin predicate excludes it; special-weapon spin still applies.
Speed<50 tests the original unscaled speed, even when boost changes velocity.
The scale product is stored as float before multiplying forward components.

verify_projectile_descriptor.py executes original4c77a0 to the486da0 call
with real string/vector/spin/4c90f0 code. Only owner lookup and player/powerup
predicates are supplied, with arguments/order checked.2048 cases match all
152 bytes plus boost on PC and compiled NXDK at027f:250 boosted,1264 spinning.
Five PC/NXDK invalid/nonfinite guards and two NXDK null guards pass. Both
builds and22 CTests pass. Evidence artifacts/projectile-descriptor.json.
This does not allocate a projectile; generic kind2 ownership and all factory
post-allocation effects remain open. No new native scene replay or visual.
