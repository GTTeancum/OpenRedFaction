# Ordinary NPC/clutter checkpoint pair admission

Source-written 2026-10-09 after the parent's 15:00 stock-64-MiB fresh-load
batch. The correction has not been compiled or run. Parent owns the next
hourly build/load; no helper test, fixture, emulator, image, asset mutation or
new saved payload was created.

## Concrete native blocker

The unchanged 10:00 original L11S3 save reaches `npc_stage`, saved row 7,
guard1 UID 10493. Native placement rejects its sphere 0 against stationary
Console_Thin01 UID 10678, prop sphere 0. The overlap diagnostic reports
0.26061302423477173 units, with both authored-prop identity and unchanged
static terrain established. Status is `RF_NOT_FOUND` (-3), not `RF_FORMAT`.
The result is `/workspace/shared/rf-frozen-npc-20261009-1500/load/result.json`.
This precedes the two frozen owners, saved rows 15 and 16; their native
restoration remains unverified.

Read-only comparison with original `levels2.vpp/L11S3.rfl` establishes that
10493 retained authored X/Z and yaw 0, settling only from authored Y
-73.01814270019531 to saved Y -73.12557983398438. The prop retains its
original placement and class. Its original model has no CSPH section, so
the base factory supplies a coarse construction sphere. A coarse overlap
alone does not establish a collision pair under original gameplay policy.

## Recovered rule, rather than a larger clearance allowance

`rf_collision_pair_reject`, reconstructed from original 0x48be00, handles
kind 0 versus kind 4 in `src/core/collision.c`. After common exclusions:

- Player object bit 8 has its own parent/extent branch.
- A nonplayer can form this pair only when its class `use_kind == 1` and the
  prop's physical extent exceeds 0.5.
- Every other nonplayer use kind rejects the pair, independently of position.

Original `tables.vpp/entity.tbl` explicitly gives guard1 `$Use: "none"`.
`rf_entity_class_physics_read` initializes that to 0; only the eight named
use actions map to nonzero values: vehicle 1, switch 2, command 3, turret 4,
monitor 5, medic 6, ai response 9 and play_sound 10. `none` matches none of
those names and preserves 0. The ordinary live prop composer is also
explicitly player-only, so the checkpoint obstacle loop was adding a contact
that this NPC does not acquire through ordinary movement.

Prior original/PC/NXDK classifier evidence is documented in
`DEATH-LIFECYCLE.md`, "Full object-family pair classification", and
`tools/verify_pair_classification.py`. Those historical results are not a
new test of this scene adapter.

## Source change and preserved boundaries

`scene_npc_clutter_checkpoint_pair.inc` qualifies a living, nonretired,
non-corpse NPC through exact saved UID/class/position, immutable sphere owner,
type-0 entity view and generation-bearing registry identity. It excludes
player bit 8, vehicle use 1 and unknown use codes. Saved RFNC hidden and
physics masks replace corresponding live bits in a private pair view.

For each prop, it checks the prepared RFPC row, candidate pointer, authored
slot/UID, generation-bearing handle, exact class/definition, body-sphere owner
and shared static model identity. Visibility and physics facts supplied to
the shared classifier come from the staged candidate, not a live collision
wrapper. Incomplete proof returns false, retaining the prior sphere guard.

The optional callback lives only on `scene_world_restore_place`'s local
ordinary NPC context. Seated and supported-vehicle branches return before
it is installed. The stored world context, player, vehicle, corpse and
default callers retain their prior behavior. The pair context is stack-owned
for the synchronous placement call and is never retained in the world stage.

Only proven noninteracting kind-0/kind-4 pairs omit the overlap rejection.
The loop still validates prop sphere transforms and positive finite radii.
Static-world volume classification, room location, support requirements,
mover clearance, NPC/NPC pair fit and every remaining interacting prop check
are unchanged. No overlap depth tolerance, authored-position exception,
zero-motion trace, mesh approximation or save format is added.

Native success, continued frozen-owner assignment and any later independent
restore blocker remain for the parent's next hourly batch.
