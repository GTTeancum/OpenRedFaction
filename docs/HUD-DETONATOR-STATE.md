# Detonator HUD state

Source-written after the parent21:00 pass on2026-10-08, based on tested4a1ccb8.
No helper builds, tests or captures. Parent owns the22:00 batch.

## Concrete gameplay confusion

Before this change, both Remote Charge slot8 and Remote Detonator slot9 showed
the carried inventory reserve. After throwing the last charge, the detonator
therefore showed red0 even while a deployed owned charge remained actionable.
The actual scene_remote_input mode2 needs no ammunition: either trigger's
rising edge requests all matching live owned class0x40 records. The reserve
display described the wrong owner for this mode.

## Bounded fix

scene_remote_hud_counts performs a read-only pass over the existing32-slot
projectile owner. It selects active records with the exact current player
owner, remote weapon type and class0x40; retired flag2 records are excluded.
Positive fuse/life records are ready, and nonpositive fuse/life records await
the already-existing next-step detonation. The same player view/handle must
still resolve in the live entity registry. No request/tick or other mutating
service is called from this reader.

Only the slot9 local draw snapshot replaces its displayed count with ready
plus pending deployed charges. The existing inventory, gameplay counters and
slot8 carried-ammo display are untouched. The original-art path labels the
count DEPLOYED and shows RT DETONATE, DETONATING or NO CHARGES as appropriate.
The procedural fallback carries the same information. PC staging uses the
generic FIRE DETONATE wording; no PC runtime work was done. A stale generic
reload counter cannot add a reload strip to a mode that has no reload action.

The labels are first-playable explanatory UI, not a claim of retail wording.
The original detonation controls, cadence, blast effects, save/checkpoint
formats, lifetime, damage and ownership remain unchanged. No allocations,
textures, timers, controller bindings or simulation steps were added.

## Other essentials reviewed

Existing magazine/reserve numerals, red empty signal and live reload progress
already describe ordinary firearms. Flame and Fusion adapters publish their
own actual reload counters into the HUD's existing value; no replacement
reload owner was needed. Existing health numerals, health-frame progression
and damage flash give first-playable health feedback. No extra flashing or
low-health overlay was invented. Scope input and feedback were already
source-verified and are unchanged here.

Generic interaction prompts remain outside this patch. Trigger contact polling
owns dwell/action state and optional reach checks on actual Use input; it must
not be re-entered as a HUD reader. No guessed interactable target or duplicate
activation was introduced merely to add a prompt.

Existing remote pickup/ordinary continuation cases are in
tools/check_remote_pickup.py and tools/check_ordinary_remote_reload.py, with
native historical coverage recorded in docs/REMOTE-PICKUP-FIRST-PASS.md.
Those prior checks are not a pass for this new HUD branch. A useful parent
batch observation is a planted charge with no carried reserve, then the
normal request and next-tick retirement; no new helper fixture was created.
