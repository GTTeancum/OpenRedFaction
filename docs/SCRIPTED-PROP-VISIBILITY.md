# Scripted prop visibility first pass

Switch object routing now recognizes registered static clutter alongside NPCs. Level trigger/event link resolution includes authored clutter UIDs, bound to current generation-checked registry owners. The shared visibility callback sets/clears0x4000 without recreating resources or reviving retired/dead owners. UnHide uses that same callback when its existing deferred runtime dispatch resolves a clutter link.

The existing render dispatcher suppresses hidden props. Raw clutter collision queries now return no hit for hidden/retired owners, covering attached glare occlusion queries. Corona and volume parent visibility now suppresses effects attached to a hidden/retired prop. Ordinary collision visibility already checks hidden flags. This does not implement missing general clutter damage/physics gameplay.

## Evidence

`tools/check_clutter_visibility.py` prepares an isolated copy of CTF06 with a Switch linked to original lantern_box UID13025, preserving its model and authored placement. Original game inputs are unchanged. `artifacts/clutter-visibility-live` contains three90-frame PC replays and inspected final images:

- Baseline: lamp visible,39 contributing props and2253 transient vertices.
- Hidden: Switch900101 activated once; lamp absent,38 props and2211 vertices.
- Shown: same Switch activated again at frame60; lamp visible again,39 props and2253 vertices, matching baseline render hash439329823.

Focused `rf_scene_switch_objects_tests` passes actual core Switch routing, registry generation/identity checks, dead/stale owner preservation, hidden collision rejection and hidden/retired corona suppression. PC and NXDK builds pass. Baked illumination is unchanged when a lamp is hidden.

The stock-64-MiB Xbox-only check in `tools/xemu_clutter_visibility.py` stages
that same disposable level copy without running PC gameplay. Its two 90-frame
XEMU runs toggle the linked Switch once and twice. The final guest detail
identifies clutter UID13025 with `0x4000` hidden after one activation and
visible after two. Hidden versus shown clutter submission differs by one
placement and 42 vertices (69/1503 versus 70/1545); the per-frame render
dispatcher counts 90 hidden placements in the first run and 60 in the second,
consistent with the later reappearance. Both runs had 3,675 free pages, the
disc was restored, and the report is
`artifacts/xemu/clutter-visibility-20260929-223610/report.json`. These are
target-specific state and draw checks; no image was captured or visually
inspected, so native pixel appearance remains unverified.

The newer mutable-prop checkpoint already serializes the hidden flag; only
the older immutable profile rejects that mutation. A focused 90-frame Xbox
save/load fixture now activates the Switch once, saves the hidden lamp at
frame40, activates it again at frame60, and loads at frame70. The final guest
state reports both save and load successful with the same 17,340-byte payload,
and the linked lamp is hidden again with one restored Switch activation and
69 contributing clutter placements. The run retained 3,474 free pages and
restored the test disc (`artifacts/xemu/clutter-visibility-save-20260929-224309/report.json`).
The separate two-activation control verifies that the frame60 dispatch shows
the lamp when no load intervenes. This establishes one native same-level
ordinary-save continuation, not cross-level or arbitrary prop-state coverage.

The broader `rf_npc_residency_tests` run fails in `scripted_attack_damage_check` at its expected-health-decrease assertion before reaching the visibility fixture. This turn does not claim that suite passes or that the failure predates these changes; investigation remains separate from the focused visibility evidence.

## Remaining

Live authored UnHide sequence coverage, other object families, state retention across scene revisits and wider save cases, broader prop gameplay and native visual-content verification remain. Existing immutable-clutter checkpoint admission continues rejecting changed hidden flags rather than silently dropping them.

Follow-up: both stale NPC test fixtures were corrected during prop-damage integration; the complete rf_npc_residency_tests executable now passes (artifacts/clutter-damage-live/npc.log).
