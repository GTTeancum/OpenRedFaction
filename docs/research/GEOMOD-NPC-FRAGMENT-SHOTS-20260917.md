# NPC bullets now apply detached-fragment contact

The live NPC hitscan path previously used `combat_shot_obstructed` only after its spread ray intersected the intended actor. A chunk could block that actor hit without receiving direct damage, and a ray missing the actor never tested chunks at all.

`combat_enemy_fragment_shot` now queries retained fragment geometry up to the intended actor fraction, or the full ray for an actor miss. World/mover obstruction is checked only up to the nearest fragment contact. This distinction matters: a wall behind a fragment must not suppress damage to that nearer fragment. A front wall still prevents damage. A fragment hit consumes the shot even if that hit retires the fragment. Pickup and visibility uses retain the original read-only helper.

The actual NPC call retains its existing provisional damage10 for both actor and fragment targets. This change does not claim authored NPC weapon balancing. The shared kind3 damage contract from GEOMOD-DETACHED-BLAST-DISPATCH-20260917.md admits health loss only for amounts greater than100, so these ordinary10-point hits set the contact flag without removing health or waking the body. A synthetic400-point control proves the same adapter can retire a chunk; it does not prove a currently equipped NPC weapon fires that amount. Ordinary radial blasts remain excluded by the existing binary evidence. No recursive breakup or universal blast wake was added.

The scene regression uses real owned piece registries and collision geometry. It verifies read-only visibility, a nearer actor limit, front-wall preservation, rear-wall contact, below-threshold survival/contact-state change, high-damage retirement, and subsequent ray passage through the retired piece. All123 PC tests pass. Stock-profile NXDK compilation, linking, XBE and ISO generation pass. Logs: artifacts/npc-fragment-build.log, npc-fragment-tests.log and npc-fragment-xbox.log. No live NPC firing scene or XEMU visual behavior was tested in this change; those remain qualification work. Existing multi-source source-qualified damage tests remain passing.

The shared parser now retains both authored AI damage scales and defaults both to1. Original getter4c8b10 uses a separate runtime scalar at class+120; its selection remains open, and the scene retains provisional10. See [AI damage evidence](WEAPON-AI-DAMAGE-SCALE-20260917.md).

## Authored damage integration follow-up

Normal table setup4c2a20 is now verified to select the first pair member;
alternate4c2ac0 selects the second. Both complete loops pass520 field/boundary
comparisons with unequal pairs and multiple class counts. The live scene now
uses primary damage times the normal AI scale for both actor and fragment
requests: handgun40, rifle24, riot stick6 before downstream target modifiers.
Unsupported weapons retain10. The prior provisional10 statement describes the
initial adapter revision, not current supported-weapon behavior.

The fragment regression now sends a computed rifle24 through front/rear wall
controls and computed synthetic heavy400 through retirement. All123 PC tests
and the stock NXDK build pass. These ordinary supported weapons remain below
the original strictly-greater-than100 fragment health threshold. No wake,
subdivision or radial damage behavior was changed. Live NPC rubble shots still
need qualification; the actor8456 PC encounter verifies five live handgun hits
and player death with the normal combat scheduler.

Native qualification: artifacts/xemu/render-20260917-222646 passes64 PC/Xbox
comparisons on stock64MiB. Five bullet hits, firing clip starts, audio-event
counts and player death match exactly. The final framebuffer was inspected:
textured mine corridor, armed NPC, death/respawn overlay and zero-health HUD
are present. Audio was disabled in the emulator, so audible quality is not
verified. Endpoint free memory5268 pages (20.578MiB); disc restored and the
owned emulator closed. This run contains no live NPC rubble impact.

Reproduce PC acceptance with `python -B tools/check_npc_authored_damage.py`.
The replay uses neutral input and verifies no liquid-damage contamination.
