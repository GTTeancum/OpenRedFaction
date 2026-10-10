# Ordinary player collision with authored solid props

Source-written on 2026-10-09 from frozen parent HEAD `6c7a3af7` in a separate local worktree. The helper owns only `scene_player_clutter_collision.inc` and this note. Parent-owned `scene.c` integration is supplied separately; the active checkout and its 05:00 jump/landing batch were not changed.

No build, test, native execution, new gameplay fixture, emulator launch, capture, cleanup, PC execution or remote publication was performed. This is an unverified implementation, not a working-runtime or completion-percentage claim.

## Missing live consumer

At the base commit, `campaign_physics_body_sweep_for` composes the ordinary player against world/mover geometry, detached pieces, the active Driller and passive vehicle hulls. `actor_ground_query_state` similarly lacks retained clutter. Existing projectile contacts and vehicle-entry contacts do not make authored solid props block normal walking or support the player's downward probe.

The new `scene_player_clutter_compose` adds a bounded read-only contact consumer for stationary authored type-4 owners. It borrows their existing registrations, class definitions, physics metadata, pose and retained exact model collision geometry. It creates no physics, damage, destruction, attachment or save owner.

## Original source contracts

- `src/core/entity_assets.c`, clutter flag parser, maps `collide_weapon` to class bit 2 and `collide_object` to bit 4.
- `src/core/model.c`, reconstructed `40f360` full clutter factory (`rf_clutter_create`), maps any class bit 2 to object flag `0x40000`. A class containing both bits is therefore still excluded from ordinary actor collision. The partial scene base factory does not publish that full-factory flag, so the composer derives it from the retained class.
- `src/core/collision.c`, `rf_collision_pair_reject` / original `48be00`, requires a physical endpoint (`body_flags & 0x20`) and rejects `0x40000` against nonprojectiles. Its type-0/type-4 branch accepts the player only for extent greater than 0.5 and rejects a prop parented to that actor. This does not authorize general miner/guard collision: ordinary nonplayer actors need the separate use-kind-1 gate.
- The `extent_180` size gate means `owner->body.state.bounds.radius`, not the model radius at `attachment.radius`. `rf_physics_spheres_bounds` derives it from each body sphere's center length plus radius. The original allocation audit in `docs/DEATH-LIFECYCLE.md` records this distinction.
- `campaign_clutter_registered` and `rf_scene_clutter_collision_query` validate generation-bearing registration, placement/model index and shared model identity. The latter dispatches retained static mesh geometry through reconstructed `5031f0`, `54e000`, `54daa0` and `54de40` paths; flags 2/0x4000 suppress retired/hidden owners.
- `scene_group_control_children.inc` retains authoritative group-child membership. `campaign_attachments_pass` supplies actual attachment nodes, with `UINT32_MAX` parent for current ordinary static owners. The dirty-pose bit is not membership.
- `rf_collision_mover_contact` / original output conversion `498fb4..499011` rotates the mesh-local normal/contact into the current owner pose. Its zero-initialized contact payload also supplies zero inverse mass when used here for a stationary object.
- `scene_clutter_damage_live.inc` sets object flag 2 on ordinary health retirement. Collision admission reads that same live owner on each query, so the next query omits the obstruction without a second damage or retirement path.

## Admission and contact policy

1. The source must be the exact registered type-0 player view with object flag 8. Hidden, retired and projectile-only sources are not admitted. NPC consumers remain unchanged.
2. The target must be a live generation-matched retained clutter owner, with exact class, model/shared-resource and authored UID identity. It requires class bit 4 without bit 2, the original physical-endpoint gate, and physics extent greater than 0.5. Source/linked-parent identity, hidden/retired flags and an explicit projectile-only object flag exclude it.
3. This slice excludes group-owned and attached targets, even while stopped. Factory parent properties must retain their ordinary 0/1 values; they are not misinterpreted as handles. Attachment-node absence before the first ordinary attachment pass is allowed because the current base factory is unparented.
4. Authored, published, current and predicted positions/bases must agree. Linear/angular velocity and angular momentum must be zero. This deliberately excludes moved, moving or carried props rather than inventing a static-support contract for them. It follows the existing RFPC authored-pose restriction.
5. Each query sphere uses the same fixed-basis center transformation as the ordinary body query. An allocation-free slab rejection unions existing physics bounds with the model's origin-centered bound. The model bound is conservatively transformed as an enclosing cube, including basis roundoff. Physics spheres alone are not assumed to enclose the mesh. Radius expansion and slab arithmetic use double precision with exact zero-delta handling; these bounds never become final contacts.
6. Every admitted candidate uses the actual retained finite-radius mesh query. Model flags remain zero: world query bits must not become model `any-hit` or `already-local` flags. The existing nearest fraction seeds `mesh_hit.time`, and reset 0 preserves it. A strictly nearer contact wins; equal contacts retain the previous source, prop and sphere order. Full displacement stays unchanged.
7. The published hit carries world-space point/normal, the real class material, query sphere index, generation-bearing object handle, model triangle token and one-based-to-zero-based owner room. There is no fabricated world face or texture. `solid` and `face` are `UINT32_MAX`; velocity and inverse mass stay zero.
8. Outputs are committed only after successful completion. A miss preserves the existing contact and matched state. The optional `replaced` output identifies a successful nearer prop contact so a ground caller can clear an obsolete loose-piece support reference.

There are no per-query or retained allocations. Iteration follows existing authored placement and query-sphere order; exact geometry is reached only after eligibility and conservative broad-phase rejection. Existing mesh-query diagnostics may change, but no gameplay owner is mutated.

## Parent integration

The separate patch adds the include immediately before `campaign_physics_body_sweep_for`, after all required clutter, attachment and group-control declarations. No forward declaration is needed there.

- Append the composer in the existing player-only branch of `campaign_physics_body_sweep_for`, after world/piece/vehicle composition. Pass the real player handle and `campaign_player_view.linked_handle`; use no replacement-output pointer.
- Append it in `actor_ground_query_state`, after the same existing providers. If it reports replacement, clear only that query's `scene_piece_support` output. This prevents `actor_support_commit` from treating a nearer stationary prop as the superseded loose piece.
- Keep `solid == UINT32_MAX`, zero contact velocity and the current support commit. `rf_physics_support_commit` clears dynamic-support identity for this case; no movable support registry or save format is added.

Suggested parent-owned open milestone: Ordinary player solid-prop collision has a source-written exact-mesh sweep/ground composer with original eligibility and existing retirement ownership; integrate after the frozen jump/landing batch and verify a bounded authored grate contact/removal sequence on Xbox.

## Authored candidate and verification boundary

The independent read-only contract review identified original `L3S1` Duct Grate Cover UID 6913 at approximately `(-53.00034, -3.12500, -19.23451)`, around 2.77 units from the original player start. Its class uses only `collide_object`, metal (material 2), life 30 and the existing `grate break` effect. Its compiled `grateduct1.v3m` has no CSPH list, so its approximately 1.05661535 model radius supplies the body fallback and passes the size gate. Its thin local box is approximately X/Z +/-0.7470607 and Y +/-0.01556378. The authored record has no group membership or links.

These are original-data/source facts, not runtime proof of a usable approach, equipped weapon, camera alignment, contact normal or successful destruction. Prefer a bounded ordinary walk/contact/shoot/clear check at that original section start if the parent verifies the approach. Do not grant equipment, teleport into a fabricated test layout, trigger fake events or run a campaign route.

The retained basis makes this grate a thin vertical X plane, not a horizontal platform. Its center is approximately 93.65 degrees left of the original start forward; a default forward walk/fire will not target it, and a direct left approach meets its edge. Original startup supplies a 40-damage handgun against its 30 life, but actual runtime inventory and unobstructed fire still need verification. The existing break-effect binding does not instantiate the named `grate break` VFX, so do not mistake missing grate debris for failure of retirement and released collision.

The opening `L1S1` scene contains weapon-only placements and is a useful exclusion/regression case, not a positive solid-prop check. At the next parent-authorized Xbox batch, observe actual body/ground contact ownership, material, nearest ordering and released obstruction after existing retirement. Existing contact telemetry is sufficient; this slice adds no fixture or audit system.

## Explicit limits

- Compilation, stock-64-MiB cost and runtime contact/destruction behavior remain unverified.
- Static prop tops can supply live ground contacts. Ordinary checkpoint placement currently accepts support from world/movers and conservatively checks prop spheres; saving/restoring on a prop top may still reject. Check a real selected pose if exercising save continuity. No checkpoint guard is weakened and no prop-top save support is claimed.
- Group-owned controls, moved/stopped props, hauling, dynamic props, NPC contact, model switching and coupled prop response remain outside this slice.
- Zero-motion body placement and deep initial-overlap recovery are not added. Triangle sidedness and finite-radius edge behavior remain those of the retained original mesh tracer.
- Existing jump/landing sounds, movement response, damage/removal, world/GeoMod geometry and vehicle ownership are unchanged by the helper.
