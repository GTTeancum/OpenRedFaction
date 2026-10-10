# Candidate-owned stationary prop checkpoint support

Source-written 2026-10-09 for the parent 09:00 Xbox batch. Compilation and
runtime behavior remain unverified. No build, test, gameplay fixture, emulator,
campaign traversal, image, grant, event injection, cleanup or Git publication
was performed by the implementation helper.

## Practical missing integration

Ordinary movement already composes stationary solid props in
`scene_player_clutter_compose` and `actor_ground_query_state`. Their contacts
have `solid == UINT32_MAX`, zero velocity and no dynamic support handle.
The ordinary RFEN player restore instead used static-world-only standing
admission, then world/mover-only final ground selection. A legitimate prop-top
standing pose could therefore be rejected as unsupported.

The existing conservative prop-sphere clearance gate is a separate restriction.
This slice does not relax it, replace it with a zero-motion sweep, add a shallow
overlap exception or promise saves on every prop mesh.

## Concrete original surface

Read directly from the unmodified installed archives:

- `levels1.vpp/L2S3.rfl`, clutter section `0x50000`: bench01 UID 2031 at
  `(65.2717056, -0.474375308, 74.3808136)`. No links, common properties or
  membership in either list of the original moving-group section `0x3000`.
  It is approximately 8.883 units from that section's original player start.
- `tables.vpp/clutter.tbl`: bench01 uses `bench.V3D`, metal, life 40 and
  `collide_object` without `collide_weapon`. Its class therefore admits ordinary
  player collision through the existing 40f360/48be00-derived policy.
- `meshes.vpp/bench.v3m`: one submesh, one LOD. Two stored collision triangles
  form the seat plane at local Z `0.367666960`, with local normal `(0,0,1)`.
  They span local X `[-1.543435216, 1.543435216]` and local Y
  `[-0.283409923, 0.283409923]`. The authored matrix maps local +Z to world +Y
  (scale within binary32 roundoff), giving world seat Y approximately
  `-0.106708304` and a 3.08687 by 0.56682-unit upward surface.
- Its three original CSPH spheres have radius `0.164139479`, local Z
  `-0.001385779`, and local X approximately `0.889174`, `-0.027715`, `-0.989675`.
  The seat is approximately 0.369053 above those sphere centers. Its physical
  extent exceeds the original 0.5 gate; the CSPH geometry leaves room for a
  sphere resting above the seat without automatically requiring an overlap
  exemption.

These are source/asset geometry facts, not a demonstrated approach, jump,
standing-body fit, save or reload. The route is not authorized for replay.
No test fixture or forced placement was created from this evidence.

## Implementation boundaries

- `scene_checkpoint_world.inc` gains an optional static-support provider and
  a bridge to the existing `rf_checkpoint_world_standing_check` callback. The
  original full-body static-world fit and upright-basis checks still run first.
  The same provider then composes after the final world/mover ground query.
- `scene_world_player_restore.inc` installs it only on its synchronous local
  context for an on-foot player with no persisted dynamic-support UID. NPC,
  seated, mover and passive-vehicle placement contracts are unchanged.
- `scene_checkpoint_clutter_support.inc` reads the prepared RFPC candidate.
  Compact UID-sorted rows map through each prepared entry's authored slot;
  traversal remains in authored placement order to preserve exact prop ties.
- Generation-bearing handle, authored UID, class, body-sphere identity and
  shared model resource are checked against the already prepared stage and
  immutable original owner. Each query first repeats
  `scene_clutter_checkpoint_targets_valid`, which checks registry generation
  and owner identity without consulting live visibility/death state.
  Eligibility uses candidate hidden/retired flags.
  Class bit 2 and object `0x40000` still exclude weapon-only collision.
  Immunity bit 4 does not remove a supporting surface.
- Group-owned, attached, moved, moving and non-authored poses are excluded.
  The attachment comparison uses the original owner identity, since the staged
  candidate is a copy. Live hidden/dead flags are never substituted for saved
  state. Terrain mutation does not discard an otherwise unchanged prop's
  stage-owned geometry identity.
- The provider reuses the live composer's conservative slab rejection and the
  same retained `rf_collision_model_trace` geometry. It does not call
  `rf_scene_clutter_collision_query`, whose wrapper intentionally reads live
  flags. Each sphere receives a fresh local query, `flags=0`, `reset=0` and the
  existing nearest fraction. Strictly nearer contacts alone replace a result.
- Local contact point/normal are converted through
  `rf_collision_mover_contact`. Static support retains `solid == UINT32_MAX`,
  zero velocity, the real material and final support handle zero. Nothing new
  is serialized and no current owner is temporarily modified.

No per-query or retained allocation is added. The authored-slot-to-RFPC lookup
is bounded save-time work; it is not a new per-frame traversal.

## Parent integration and remaining scope

The helper deliberately leaves `scene.c` unchanged. Insert
`#include "scene_checkpoint_clutter_support.inc"` after `scene_world_restore.inc`
and before `scene_world_player_restore.inc`; the supplied parent hook patch
contains that insertion.

The full world, mover, prop-sphere and player/NPC body-clearance gates remain.
Large coarse-sphere props may still reject a valid mesh-top pose; that broader
exact-volume work is not part of this slice. No dynamic-prop support ownership,
RFCP developer-terrain support expansion, crouched/airborne save admission,
schema migration, or new NPC collision policy is added.

Until the parent batch, report this as written source only. A later bounded
original-input check must establish actual standing and successful ordinary
save/fresh-load before calling prop-top continuation working.
