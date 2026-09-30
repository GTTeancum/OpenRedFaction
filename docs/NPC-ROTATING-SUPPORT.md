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

There is a separate support-loss limitation: `campaign_npc_refresh_support_tick`
runs before the next controller commit and replaces the cached velocity with
the host-center velocity in both grounded and falling modes. Scripted ground
loss uses the new point velocity in the same frame; idle ground loss starts
falling on the next frame, after that refresh. Reliable inherited tangential
velocity after idle support loss therefore remains open. The existing fall
transition also retains the old support handle. This change does not claim to
solve detachment velocity or alter those ownership semantics.

## NPC vehicle seats

The earlier candidate task was seat ownership. Original `0x427240` attachment,
`0x427380` release and `0x487630` follower publication are documented in the
existing vehicle research. However, the installed event handlers expose entry
and attempted-exit *polls*, not an authored NPC boarding command. No concrete
authored NPC assignment source was identified in this pass. A standalone
unused seat owner or automatic proximity boarding would not close that gap,
so this change addresses the existing rotating-support path instead. NPC
drivers/passenger ownership remains open.
