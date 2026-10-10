# Bounded live player / living-NPC contact

Source baseline: `97e37677e8d4b9ed326ee6b7d5569ecd71ac13ad`. Staged and source-reviewed 2026-10-10. This change has not been compiled or exercised by its author/reviewer.

## Scope and integration

`scene_player_npc_contact.inc` composes a qualified living humanoid contact only in the real per-pass `actor_tick` movement path, after the existing `actor_sweep` and before contact advancement. A typed per-pass stack receipt chooses the existing `rf_physics_dynamic_contact` response instead of the static branch of `actor_contact`. The original world/mover, detached-piece, Driller/passive-vehicle and stationary-prop sweep remains the first consumer; a strictly nearer NPC contact may replace its result, while ties and existing zero-time contacts remain unchanged.

This is a practical one-way player movement adapter, not activation of the original global actor-pair scheduler. It neither publishes a reciprocal NPC contact nor pushes, damages, moves or advances animation for the NPC. A stationary player is not protected against a subsequently moving NPC by this adapter. Airborne, swimming, climbing, mounted, cutscene, corpse, nonhumanoid and original non-`0x20` pair routes are outside its scope. It adds no deep-overlap repair.

## Admission and shape

- Require a living ordinary on-foot player and living, visible, unlinked humanoid target, each with movement descriptor index 1 and retained physical bodies. Join both full generation-bearing handles to their exact object-registry wrapper and entity-registry view. Reject hidden/noncolliding/dead/rigid or attached targets and require a currently owned, loaded, non-handed-off NPC pose.
- Reuse `rf_collision_actor_pair_reject` with the actual body flags and physical `bounds.radius`, player first, ordinary single-player arguments. Require its exact `0x20` result. Reuse `rf_collision_pair_response_allowed` for original linkage/hidden policy. Humanoid `use_kind != 1` is intentional: guard1 is none/0, while miner1 is ai-response/9; original vehicle/1 is excluded.
- Retain original strict three-dimensional body-bounds overlap admission before the horizontal shape. Player bounds cover its full proposed segment; NPC bounds cover its committed position.
- Read the maximum radius from each body's actual retained collision spheres, rather than using model bounds, class navigation radius or sphere-center distance. Flatten the relative ray to Y=0, use the existing `rf_collision_ray_sphere`, and preserve the original zero-motion and inward-only initial-overlap gates. Normal Y is zero. This is lateral contact, never an NPC floor/top/support query.

## Explicit timing adaptation

The original `508e40` ray helper already returns normalized segment time. Original `49ab00` divides that result by ray length again at `49ad33`; the original consumer uses that value directly for remaining time and movement. The reconstructed original helper deliberately preserves this quirk and is unchanged.

This live adapter deliberately uses the first, geometric normalized fraction so it can compare contacts with the port's existing world-style `actor_sweep` fractions and use `rf_physics_contact_advance`. It preserves the grounded horizontal shape and admission/approach policy, not original double-scaled timing or whole-scheduler behavior. Existing contact advancement retains its distance backoff. This distinction must remain explicit in future claims and documentation.

## Sequential endpoint and response policy

The player tick precedes `campaign_script_step`. For every query, the target's end equals its current committed body position. Never reuse NPC `next_position` and never add `velocity * dt`: either could double-project a stale or not-yet-simulated NPC interval. NPC velocity plus existing support velocity is copied only as response metadata. The player query uses the state/endpoints passed to the live tick, not global player endpoints.

An accepted contact carries its full target handle, real material and copied contact velocity. A typed stack receipt separately retains positive inverse mass; `rf_collision_body_hit.reserved_20` remains an integer reserved field and is not type-punned into mass. The responder requalifies source/target identity, original pair eligibility, target position, maximum radius, mass, material and velocity before calling `rf_physics_dynamic_contact` for the player. That helper is the reconstructed positive-inverse-mass branch at `49dcf6..49ddef`; it changes only current-player velocity and impact.

An expired or changed receipt remains recognized/handled with impact zero. It must not fall through into static cover or mutate an obsolete NPC owner. The already consumed translation is conservatively retained and an ordinary next movement pass requeries. Currently the interval is callback-free: contact advancement is pure, grounded horizontal contact makes landing-audio observation a no-op, and impact recording is deferred. No callback retry, rewind, persistent receipt, pair allocation or new reset protocol is needed.

## Save and existing-query boundaries

The adapter is not called by generic body sweeps, stance clearance, teleport probes, ground/support queries, or candidate restore validation. The receipt is local to one movement iteration and is not serialized. Existing player pose, room, velocity, support and impact publication remain their established owners; no NPC support handle or save-format extension is introduced.

Existing ordinary checkpoint scope remains authoritative, including its grounded/on-foot and supported-state restrictions. Existing restore overlap allowances are untouched: live shallow player/NPC overlap may be admitted up to 0.5 units, or 0.75 for the qualified active-pursuit case (`scene_world_player_restore.inc:44–49`). This source integration does not prove a new contact-and-save continuation, change old-save acceptance, or authorize bypassing clearance.

## Integration and validation status

Parent integrated the independently reviewed include (SHA256 ea3f8b914f1b4d83413f744258ed1349825a2470d907fc3714399a956fd465e7) and scene hook (SHA256 ecba65398f3b8e18925a92a7b0b1133bd3e6b541bc7565309d99597a7e3d8a48) after the completed 14:00 batch. See PLAYER-NPC-CONTACT-EVIDENCE.md for the original UID109 facts and addresses. Source review found no blocking issue; compilation awaits the single 15:00 Xbox batch. The existing old-save regression is not contact-action coverage.

No author/reviewer build, test, syntax check, emulation, fixture, event injection, route, campaign scan or PC work was performed. Ordinary player contact, dynamic response and subsequent save continuation remain runtime-unverified. No new save layout or persistent allocation is added.
