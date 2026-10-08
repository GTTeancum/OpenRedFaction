# Grounded riders on independently simulated vehicles

## Source and status

Written on 2026-10-08 against attack revision
`64f3c0a42e65b63d6d62cb08e7dbef5ae055e59e` in an isolated source snapshot.
No build, executable run, fixture, or gameplay test was performed for this
slice. Parent integration and the scheduled 14:00 UTC batch own verification.

Original reference: RF.exe, 1,773,568 bytes, SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Read-only disassembly establishes these boundaries:

- `41e370..41e3d9` visits the actor list, selects movement modes 1 or 3,
  resolves the actor's support handle at `+8ac` with `40a0e0`, and copies the
  resolved object's `+144` velocity into the actor's `+8a0` support cache via
  `409f40`. There is no mover-group ownership requirement in this consumer.
- `40a420..40a442`, called by that consumer, marks the body at `+1a8` with
  `80000000` and the object at `+7c` with `06000000`.
- The frame path calls `46bbe0` at `487bf8`, then `41e370` at `487bfd`, before
  `487770` at `487c0e`. Support refresh follows host motion in this phase.

The existing reconstructed `rf_physics_support_refresh` implements the cache
and wake flags. The port's already integrated rotating-support point math and
full-body/roof checks supply the practical transport path. The clearance veto
and exact-host tangent retry below are explicit port policies, not claims of
retail coupled-body or crush-response parity.

## Implemented transport

An admitted secondary owner remains the sole rigid authority for its existing
APC, submarine, Fighter, or Masako hull. The hull no longer pauses merely because
a player or NPC has grounded support on it. A transportable rider must have the
exact live support handle and registry generation, grounded body/movement state,
no fragment support or seat/link, a living visible actor, and an ordinary active
body. Unsupported/frozen NPC bodies remain ordinary obstacles.

Before the candidate hull pose is published, the code maps each qualifying
rider's point from the accepted old host frame to the candidate new frame. It
sweeps every real body sphere through the existing world/mover/vehicle/actor/
fragment/clutter query. The carrying hull and same-host riders are omitted from
that transit query; actual sphere pairs are checked at their proposed endpoints,
including the candidate hull and the other riders. No actor is moved by this
preflight. If a rider cannot fit, the entire hull step is held at its old pose and
its linear velocity/angular momentum are stopped. No damage, crush response,
fractional multi-body solve, or second rigid owner is invented.

The accepted hull start/end position and basis now publish through the existing
passive-host interval. The player consumes its point velocity through the normal
actor integrator. The NPC's existing carry proposal consumes the same interval
once before ordinary idle or scripted movement. Catatonic NPCs with this exact
support still perform the idle physical support step without gaining AI intent.
Ground acceptance and the close-roof retention path recover velocity at the
accepted end-frame point. Existing jump/fall release keeps its inherited velocity
while clearing the support binding.

For a player query that encounters its exact independent support with an upward
normal, the complete destination must first be clear of the actual hull. Only
then is the same full sweep retried without that one self-contact, matching the
existing NPC tangent-support approach. Other obstacles remain in the query and
the temporary collision link is restored before returning. The underlying
ray-sphere function already rejects pure retreat; this retry addresses carry
chords that approach a published support sphere despite a clear endpoint.

## Ordinary save/load

The existing RFSV2 owner rows and RFVA2 poses remain the wire authority. RFNC
retains a rider NPC's support UID and point velocity. RFEN7 retains a rider
player's support UID and point velocity, including pure translation and zero
motion, because RFVA does not store that player cache.

Restore borrows the already admitted secondary candidate stage. An unparented
support requires an exact RFSV/RFVA/RFPV join: same UID, handle, slot, live class
owner, visible/living body and detached pose. A detached parked model cannot gain
support eligibility just by matching a UID. RFNC and RFEN still require actual
roof contact and complete-world placement. Secondary hull admission keeps every
player/NPC/hull pair test; support grants no overlap exemption. Independent
player support without its RFEN7 cache is rejected rather than silently replacing
that cache with zero. Attached-group and legacy narrower helper contracts remain.

After publication the existing invalidated interval is rebuilt by the next
normal host step. No saved interval is replayed, and no event is re-fired.

## Integration dependency and remaining boundaries

The parallel authority-transfer patch owns removal of the initial `has_rider`
rejection in `scene_secondary_vehicle_script_move`. This patch intentionally
leaves that shared admission line to its owner. Promotion keeps the conservative
rider dependency rejection. This slice introduces no heap-owned rider state.

Existing driver/passenger seat gates are unchanged. Independent Jeep admission,
new seat families, moving/occupied promotion, swimming passengers, corpse carry,
fully coupled contacts/crushing, and exact retail solver order remain outside
this first pass. The candidate preflight uses a straight point chord for each
fixed-duration step; it does not reconstruct a continuous rotation arc. General
walking/AI motion after carry retains the engine's existing actor-collision scope.

No new fixture was added. Overall implementation remains approximately 88%;
vehicles remain approximately 95%, pending integration and runtime evidence.
