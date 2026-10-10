# Player Riot alternate lethal-contact death hint

Source-written against clean baseline
`d2e85a808e8670b0aebce7ec679c3a27a289ca0f`, staged outside the active
repository, independently reviewed and parent-integrated. Compilation and runtime behavior remain unverified. This is a
bounded addition to `PLAYER-RIOT-ALTERNATE-PULSES.md`, not a reconstruction of
the original persistent nonlethal hint, stun behavior or melee hit volume.

## Original evidence and timing

Read-only RF.exe inspection used SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- `0x4c6052..0x4c6061` requires the resolved Riot weapon identity and the
  projectile's captured alternate bit `0x20`. Current held input is not the
  authority for this branch.
- `0x4c6063..0x4c607a` copies the contact point and calls `0x4fb990` with the
  target's published position at `+0x3c` and model basis at `+0x48`.
- `0x4fb990..0x4fb9c6`, through subtraction at `0x409fa0` and rotation at
  `0x4fac60`, computes local Z from contact minus target position, dotted
  with basis elements 6, 7 and 8. The final sum is ordered Z, Y, then X.
- `0x4c607f..0x4c60a6` writes target `+0x824`: positive local Z selects
  backward death action 7; zero or negative selects forward action 6.
- This original write precedes generic damage at `0x4c6132 -> 0x4892c0`.
  It is not conditional on applied damage or lethality. The bounded port
  intentionally does not reproduce that persistent nonlethal side effect.

## Bounded implementation

Only `scene_player_riot_contact.inc` changes. After all obstruction and physical
shield returns, and after a non-consuming Nano result, the accepted contact's
captured weapon and alternate mode qualify the hint. The currently resolved
target slot must have a loaded model owner. The calculation uses
`npc->published` and `campaign_model_owners[target].basis`, matching the
published-origin/model-basis convention already used by death clearance.

The contact point is the existing ray approximation, eye plus the existing
2.6-unit ray delta times the chosen hit fraction. This does not reconstruct the
original melee projectile's swept volume or exact contact point. The hint is
computed before generic damage, so damage and impact-sound callbacks cannot
replace its directional inputs. A nonfinite projection leaves the existing
death selection unchanged rather than inventing a direction.

The result stays in one stack-local integer through the existing damage,
feedback and flesh-sound calls. A successful positive applied amount follows
the existing ticket/live checks and reacquires the entire generation-bearing
target handle after damage and again after flesh sound. No callback occurs
between that last revalidation and the new write. Immediately before the
existing death-entry call, the hint is published only if health is nonpositive,
applied damage is positive and death has not already been entered. An already
entered death is never overwritten.

Misses, obstruction, accepted physical shields, consuming Nano contacts,
zero-applied damage, errors, canceled owner tickets, removed or replaced
targets and surviving targets publish no hint. Primary contacts retain their
existing behavior. No selected weapon, fresh input or post-callback target
orientation is consulted to reinterpret the accepted alternate action.

Existing death entry, animation selection and clearance remain authoritative.
`rf_entity_death_select` still checks forward clearance for action 6 and backward
clearance for action 7, rejects blocked directions into its existing random
selection, preserves crouch/controller overrides, and retains missing-motion
fallback to action 5 or no action. No death is forced through those checks.

## Persistence and verification boundary

Living RFNC capture does not serialize `death.action_824`; the existing capture
only writes its death action for a terminal dead actor. Publishing this hint
on surviving hits would therefore create an unrepresented living state and a
save/load divergence. This slice deliberately supports only the lethal-contact
case. It adds no retained state, allocation, save field, format version or new
save guard. Existing dead-actor persistence remains unchanged.

All work was staged outside the active repository. No build, test, syntax
check, new fixture, XEMU run, capture, cleanup or original-input modification
was performed. The parent owns integration, TO-DO reconciliation and the next
scheduled Xbox validation batch. The ordinary ray approximation, actual death
animation and clearance outcomes, interruption behavior and save/load behavior
remain runtime-unverified.
