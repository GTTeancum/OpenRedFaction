# Fusion successful-final-shot automatic selection

Integrated and independently source-reviewed on 2026-10-10 after the completed
05:00 failed build, against `a333c0f4`. Compilation and runtime remain unverified until the scheduled
parent Xbox batch. No builds, tests, syntax checks, emulator runs, fixtures,
routes, original-input edits or save-format changes were performed.

## Original evidence and narrow scope

The inspected read-only RF.exe and installed tables establish:

- `426c14` publishes the successful shot's firing presentation; `426c32`
  calls depleted-weapon consumer `4a6f10` during the same completed operation.
  This supports recording an actual successful shot, not polling idle ammo.
- `4a6f41..4a6f57` applies Autoswitch; `4a6f8a..4a6fab` checks positive
  capacity and the Remote Charge exclusion; `4a6fea..4a7010` requires depleted
  loaded plus mapped reserve and rejects descriptor `+264 & 0x20`.
- Installed `shoulder_cannon` has capacity 5, clip size 1, preference rank 0
  and no melee/no-switch flag. It is neither Remote nor paired Machine Pistol,
  so the existing ranked path through `4a70b8..4a70cf` applies when depleted.
- `4a6e84..4a6f01` scans authored preference entries with ownership and actual
  ammunition. Factory initialization at `4afed2/4afed8`, through the player
  controls subobject, gives Autoswitch on and explosive defer off. These
  existing port defaults are unchanged; saved user preferences are not loaded.
- The original accepted-dry path reaches feedback/backoff at `4a561d` before
  its final empty call at `4a564a`. Current Fusion `event.dry` has no original
  200-ms retry backoff. DRY is explicitly deferred rather than treating an
  every-frame empty-held result as an accepted original dry operation.

See WEAPON-EMPTY-SELECTION.md and WEAPON-PREFERENCE-ORDER.md for the shared
factory-policy and authored-rank reconstruction.

## Integration and invariant preservation

The existing 16-byte `scene_weapon_empty_request` remains call-local. Its enum,
typedef and exact static note declaration move ahead of the Fusion gameplay
include in scene.c; the actual note/selection implementation stays after its
existing dependencies. No duplicate type or persistent queue is introduced.

`scene_fusion_gameplay_tick` receives the same request as the combat invocation.
Only `event.shot`, emitted after the launch backend succeeded, reported a real
spawn and debited the loaded shell, records SHOT. Capture precedes optional
launch audio. Reload, rejected or zero-spawn launch, idle empty inventory and
DRY never record a Fusion request. The note and post-combat consumer both admit
slot 12 only for SHOT.

Selection remains at the existing outer boundary after `campaign_combat_tick`
returns RF_OK. A later combat error cannot execute the consumer. Full player
handle/object/entity/damage identity, live allocated body, hidden/retired flags,
finite positive health, current selection, ownership/resource admission,
on-foot/form/cutscene/reload/holster/death gates all remain unchanged. The live
Fusion resource and its view are checked by the shared candidate predicate.
Loaded and mapped reserve must both be exactly zero; positive reserve retains
the existing reload owner and malformed negative inventory is declined.

The common authored preference filter, descriptor flags, finite inventory and
retained Machine Pistol mode policy are unchanged. No rank is inserted or
borrowed, no ownership/ammunition is granted, and no special exception is
fabricated. Ordinary replacement uses `campaign_select_primary` and the existing
held fire/alternate/reload release gate, then publishes the new-view idle state.
The replacement cannot fire again during the completed combat invocation.

## Input-only deselection, independent flights and saves

Only after an actual successful replacement of outgoing slot 12 does the
adapter call `scene_fusion_input_reset(&scene_fusion_input)`. No candidate,
declined request or failed combat means no adapter reset. This immediately
applies the port's existing Fusion deselection semantics; it does not claim
source-exact original cross-selection cooldown behavior.

RFAP permits live Fusion flights while another weapon is selected, but a saved
Fusion cooldown/reload input owner must agree with selected Fusion. Waiting
until the next input tick to notice deselection would leave a same-frame
inconsistent save candidate. The narrow reset avoids that state without
weakening RFAP validation, adding a save field or delaying selection.

`scene_fusion_restore_reset` is never called here: it also resets projectiles.
All flights retain their existing source, damage, movement, contacts, lifetime
and checkpoint ownership, including the shell that caused depletion. No flight
pool or gameplay input implementation is changed.

## Remaining limits

Fusion dry retries/backoff, arbitrary saved Autoswitch preferences and the full
original queued draw/blend dispatcher remain deferred. The immediate first-pass
selection/view boundary and input-reset deselection policy are explicit port
choices. Final-shot replacement, held-input release, independent flight impact
and same-frame save/load behavior remain uncompiled/runtime-unverified here.
