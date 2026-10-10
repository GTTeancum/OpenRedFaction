# Authored Slay_Object clutter consumer

Source-written 2026-10-09 against `8ccd746a`. No compilation, test, emulator run,
fixture creation or campaign traversal was performed for this slice. The parent
owns the separate `scene.c` hook and consolidated Xbox validation.

## Concrete authored gap

`src/core/event.c::startup_event_action` already dispatches `Slay_Object` type1
in authored link order, preserving its normal delay and common continuation.
`campaign_slay_object` previously consumed vehicles, turrets and ordinary NPCs,
then returned `RF_NOT_FOUND` for registered clutter. No new generic event action,
scheduler, link propagation or save format is needed.

Read-only decoding of the installed original SP archives found these five
clutter-targeting Slay events:

| Level | Slay UID | Clutter UID | Authored class |
| --- | --- | --- | --- |
| L6S1 | 5425 | 4771 | DesktopPC_Monitor |
| L7S2 | 5331 | 4974 | Console Button01 |
| L8S1 | 10567 | 7458 | Duct Grate Cover |
| L8S1 | 10569 | 7459 | Duct Grate Cover |
| L8S2 | 9385 | 6792 | DesktopPC_Monitor |

The two zero-delay L8S1 actions are concrete obstruction-release requests.
Original `levels2.vpp/L8S1.rfl` trigger10566, named `Trigger Door`, links
`[7458,10567]`; trigger10568 links `[7459,10569]`. Both are authored use-trigger
boxes with 0.5-second contact timing. The props' `clutter.tbl` class has life30,
mesh `grateduct1.V3D`, material `metal`, `collide_object` and `grate break`.
This evidence establishes an omitted consumer, not a claim that the natural
trigger interaction or the resulting passage has been runtime-validated.

The protected L7S2 button is also material: its authored class has life-1 and
`is_switch`. A normal weapon-hit adapter without Slay's protection clear would
leave it active.

## Original action evidence

Verified installed `RF.exe` SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
This evidence was read by static disassembly, without executing the original.

- Slay constructor `4be4d0` selects vtable `58993c`; its ON entry is `4baa40`.
- `4baa5d..4baa74` resolves each linked object through `40a0e0` and skips absent
  targets.
- `4baa76..4baa81` clears object+7c bit4 before the effect.
- `4baa84..4baa99` routes object kinds0,1,4 to ordinary damage; other kinds use
  deletion helper `48ab40`.
- `4baa9b..4baab7` calls `4892c0` with exactly100000 damage (`47c35000`), untyped
  damage/type-1 and invalid attribution handles. The actor-only tail updates an
  entity death field; it is unrelated to clutter retirement.

The implementation retains the fixed100000 amount instead of inventing a
health-dependent amount. This is intentionally narrower than full Slay parity:
player, item, specialized held-shield and other object-family actions remain
outside this clutter adapter.

## Bounded implementation

`scene_clutter_damage_live.inc` retains the existing public-to-the-scene
`scene_clutter_damage_receive_live` signature. Its firearm/projectile callers
still take the identical ordinary path. A private mode and a dedicated
`scene_clutter_damage_slay_live` wrapper add the explicit scripted behavior:

- Resolve the same registered owner and initialized class binding.
- Admit an explicitly linked hidden prop while preserving its hidden bit.
- Clear protection in the staged damage state, then call the existing
  `rf_clutter_damage_receive` with100000/type-1.
- Publish health, flags, killing type, one retirement and `break_pending` through
  the shared existing damage path. Completed retirement is idempotent.
- Preserve retained registration, break effects/Foley, subsequent contact
  exclusion and existing clutter checkpoint ownership. No direct fake removal,
  new allocations, effects, fragments or save fields are added.

`scene_scripted_clutter_slay.inc::campaign_slay_clutter` matches the live registry
pointer and generation-qualified handle to the original clutter slot, then
calls that wrapper. It contributes to the existing Slay and clutter damage
counters and emits one `CLUTTER_SLAY` line per applied live request. A repeated
request after retirement returns successfully without another break request.
Existing vehicle, turret and NPC branches remain unchanged.

The parent hook includes this adapter immediately after `rf_scene_script_slays`
and calls it at the start of `campaign_slay_object`, falling through on
`RF_NOT_FOUND`. Only after that hook is integrated is the authored consumer live.

## Verification boundary

Source review only. The parent may include this in the planned Xbox build and
use the already-planned ordinary prop-destruction case to check the shared
retirement/break path. That does not validate authored Slay dispatch, protection
clear, hidden-target handling, original trigger access or save/reload.
No new per-slice runtime fixture is supplied.

The scan also rejected a tempting unrelated gap: the sole installed
`Spawn_Object23` record, L11S1 UID10418, has empty type/class strings and links
only Music_Start10402. Original loader `4625a0` calls `4b8250`, which returnsNULL
at `4b82e6` when the type string is not one of its three supported class families.
It is not evidence for a missing authored gameplay spawn. Existing common link
dispatch should not be duplicated to manufacture one.
