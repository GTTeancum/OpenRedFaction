# Scripted prop visibility first pass

Switch object routing now recognizes registered static clutter alongside NPCs. Level trigger/event link resolution includes authored clutter UIDs, bound to current generation-checked registry owners. The shared visibility callback sets/clears0x4000 without recreating resources or reviving retired/dead owners. UnHide uses that same callback when its existing deferred runtime dispatch resolves a clutter link.

The existing render dispatcher suppresses hidden props. Raw clutter collision queries now return no hit for hidden/retired owners, covering attached glare occlusion queries. Corona and volume parent visibility now suppresses effects attached to a hidden/retired prop. Ordinary collision visibility already checks hidden flags. This does not implement missing general clutter damage/physics gameplay.

## Evidence

`tools/check_clutter_visibility.py` prepares an isolated copy of CTF06 with a Switch linked to original lantern_box UID13025, preserving its model and authored placement. Original game inputs are unchanged. `artifacts/clutter-visibility-live` contains three90-frame PC replays and inspected final images:

- Baseline: lamp visible,39 contributing props and2253 transient vertices.
- Hidden: Switch900101 activated once; lamp absent,38 props and2211 vertices.
- Shown: same Switch activated again at frame60; lamp visible again,39 props and2253 vertices, matching baseline render hash439329823.

Focused `rf_scene_switch_objects_tests` passes actual core Switch routing, registry generation/identity checks, dead/stale owner preservation, hidden collision rejection and hidden/retired corona suppression. PC and NXDK builds pass. No XEMU run was performed for this prop sequence; Xbox runtime acceptance remains open. Baked illumination is unchanged when a lamp is hidden.

The broader `rf_npc_residency_tests` run fails in `scripted_attack_damage_check` at its expected-health-decrease assertion before reaching the visibility fixture. This turn does not claim that suite passes or that the failure predates these changes; investigation remains separate from the focused visibility evidence.

## Remaining

Live authored UnHide sequence coverage, other object families, state retention across scene revisits/disk saves, broader prop gameplay and native scene verification remain. Existing immutable-clutter checkpoint admission continues rejecting changed hidden flags rather than silently dropping them.

Follow-up: both stale NPC test fixtures were corrected during prop-damage integration; the complete rf_npc_residency_tests executable now passes (artifacts/clutter-damage-live/npc.log).
