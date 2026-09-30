# NPC carry on rotating platforms

The existing NPC carry path moves actors only by support linear velocity times
the frame duration. A platform rotating around its own center therefore does
not carry its riders sideways. `scene_npc_rotating_support.inc` adds the missing
position transform using the already retained frame-local mover interval.

The transform is `local = (actor - old_origin) * transpose(old_basis)`, then
`target = new_origin + local * new_basis`, in the engine's row-vector basis
convention. The support velocity becomes `(target - actor) / duration`, giving
the same-frame ground query the point velocity rather than only the host-center
velocity. Actor facing and independent walking remain unchanged. This is a
practical port implementation, not a claim that the original executable uses
this exact algorithm. Existing vehicle attachment evidence at original
`0x487630` establishes host-local to world placement, but this implementation
is ordinary ground support, not seat ownership.

## Integration

1. Include `scene_npc_rotating_support.inc` immediately before
   `campaign_npc_mover_carry` in `scene.c`.
2. In that function, after the existing loop computes `proposal.next_position`
   from support velocity, call
   `campaign_npc_rotating_support_target(owner,elapsed,proposal.next_position)`
   and propagate an error. Leave its existing body sweep, clipping, commit and
   position publication in place.
3. Reset `rf_scene_npc_rotating_support` alongside NPC diagnostic counters on
   scene setup.

The helper resolves the exact full-generation support handle in the registry,
then matches it to the mover interval. It does not seat nearby NPCs, acquire a
new support, change checkpoint identity, allocate a pose history, or act on
detached fragments/passive vehicle supports. Their existing paths remain.

`campaign_script_step` calls either idle ground or scripted ground once per
NPC, and both feed the same carry function. `campaign_controller_commit` captures the
start/end interval before that call. The helper requires the full interval
duration; a future substep caller is rejected instead of applying cumulative
rotation again. The helper must continue to be called only once per completed
interval. No persistent transform cache is introduced, so checkpoint restores
use the newly captured interval and the restored support binding.

## Validation scope

The exported `rf_scene_npc_support_interval_target` accepts the compiled
`scene_mover_interval` layout (start3f, end3f, matrix9f, end_matrix9f,
handle u32, changed u32; 104 bytes). It stages outputs and rejects nonfinite
input/output. A parent-run compiled-NXDK check can cover translated origins,
quarter-turn carry, nonidentity initial orientation, point velocity and
invalid-input output preservation without a campaign playthrough. The prepared
runner is `python tools/check_xbox_npc_rotating_support.py`.

The parent integrated the hooks and built NXDK. The helper then ran the probe
against that compiled `build/xbox/main.exe`: all eight cases pass, including
quarter-turn/off-center carry, a translated pivot, a nonidentity old basis,
unchanged and translating support, point velocity, and invalid-input output
preservation. Evidence: `artifacts/xbox-npc-rotating-support.json`.
This executes compiled Xbox arithmetic under Unicorn; it is not an XEMU/live
NPC or rendered-content claim. Live obstruction, walking carry, rendered
motion and broader rotating-support coverage remain unverified.

## Support loss and landing

The next implementation pass adds `scene_npc_support_lifecycle.inc`. Previously
`campaign_npc_refresh_support_tick` overwrote the point velocity with the old
host-center velocity on the next frame, even after the NPC entered falling
mode. The fall transition now releases the old support handle/material and
fragment ownership, retaining its final cached point velocity. The existing
`rf_physics_support_refresh` does nothing for a missing support, so the cached
launch contribution stays constant through the fall instead of following the
old platform's subsequent motion.

`rf_physics_fall_propose` adds that cached velocity to each frame's displacement
without accumulating it into `body.velocity`. The handoff therefore does not
add the launch impulse repeatedly or require a new persistent state field.
Repeated fall entry preserves the same cached contribution. After collision
admits ground, a static contact clears it and a moving contact replaces it with
the new support's contact velocity. Existing falling-state caches are retained;
this change does not erase an airborne launch velocity merely because its old
support no longer resolves.

Scripted ground loss previously applied carry and then immediately integrated
fall using the same support velocity, duplicating support travel in that frame.
The new scripted transition starts fall integration on the next tick, matching
the existing idle transition. Collision checks and ground acceptance still own
the decision to detach or land.

Parent integration hooks:

1. Include `scene_npc_support_lifecycle.inc` before `rf_scene_npc_fall`; call
   `campaign_npc_support_release(owner)` before changing its movement mode.
2. After successful `rf_physics_support_accept` in `campaign_npc_land`, install
   `campaign_npc_support_contact(owner,contact->velocity,moving)`.
3. Do the same in the nonfalling successful-contact branch of scripted ground,
   using `hit.contact.velocity`, and after the idle successful-contact branches
   when `!falling`, using `contact.velocity`.
4. After newly entering fall in scripted ground, jump to `grounded` publication
   instead of integrating a second full interval that tick.
5. Reset `rf_scene_npc_support_lifecycle` alongside NPC counters on setup.

The prepared runner `tools/check_xbox_npc_support_lifecycle.py` uses the compiled
NXDK transform, release, refresh, fall prediction and contact-cache helpers for
one rotating timeline and a translating control. Each carries once, loses its
support, falls for three steps, resets its carry cache at static contact, and
acquires another moving support. After parent integration and a successful
Xbox build, both compiled timelines pass; evidence is
`artifacts/xbox-npc-support-lifecycle.json`. The rotating launch contributes
`(-4,0,-4)` throughout all three fall steps and the translation control
contributes `(2,0,4)`, with no accumulation into the actor's own velocity.
Released handles remain zero, static-contact cache becomes zero, and the next
moving contact replaces it with `(3,0.5,-2)`.

This probe executes the actual compiled helpers and fall math; it supplies
support loss/contact admission and ordinary landing's vertical-velocity reset.
It does not execute the full collision/landing scheduler or inspect live motion.
Falling actor saves remain subject to the existing unsettled-NPC capture
restrictions.

## NPC vehicle seats

The earlier candidate task was seat ownership. Original `0x427240` attachment,
`0x427380` release and `0x487630` follower publication are documented in the
existing vehicle research. However, the installed event handlers expose entry
and attempted-exit *polls*, not an authored NPC boarding command. No concrete
authored NPC assignment source was identified in this pass. A standalone
unused seat owner or automatic proximity boarding would not close that gap,
so this change addresses the existing rotating-support path instead. NPC
drivers/passenger ownership remains open.
