# Exhausted Detonator ranked replacement

Source-written against clean `90c04e1f5e78d43518c1d3f10592412083466b35`
on 2026-10-10. Compilation and runtime remain unverified pending the parent's
scheduled stock-64-MiB Xbox batch. No helper build, syntax check, test, new
fixture, XEMU session, image, route or original-input change was performed.

## Original evidence and concrete gap

- RF.exe `4a539d` recognizes the Detonator descriptor through `4c9070`.
  `4a53ae` calls `429c70`, whose owner/class-qualified request writes fuse -1
  at `429ca3`.
- `4a53dc..4a5415` schedules the full Detonator fire-action duration and
  presentation. `4a5420` then calls depleted-weapon consumer `4a6f10` directly.
- Its Detonator-specific branch at `4a6f70..4a6f88` calls `4c9e30` for an
  exact Charge type/owner with positive fuse/life and no retired flag 2.
- `4a6f8a..4a7010` applies positive-capacity, loaded-plus-mapped-reserve and
  descriptor no-switch rules. `4a70b8..4a70cf` chooses an authored ranked
  replacement and requests selection.
- The installed Detonator definition has capacity 20, the shared remote-charge
  ammo type, no magazine and no no-switch flag. The actual descriptor is
  `campaign_detonator_id`; `campaign_selected_weapon()` deliberately aliases
  slot 9 to the Charge ID for shared inventory ownership.
- Factory Autoswitch Weapons on / explosive defer off was already established
  at `4afed2` / `4afed8`; this extension uses that existing policy and makes no
  new claim about loading a saved user profile.

Previously, successful Detonator input armed the existing target-1 follow-up,
but its expiry left Detonator selected whenever reserve was exhausted. The
ordinary ranked consumer excludes slot 9, so no replacement followed. Trigger
edges and the pending action already prevented held-fire repetition, and manual
cycling remained usable. This fills a missing action consumer, not an input lock.

## Bounded integration and admission

Only `scene_remote_selection_after_world`'s already-successful target-1 action
deadline, while slot 9 remains selected, can call the new helper. Positive
reserve keeps the existing Charge return and ready-charge remap. Target-2
throw/place completion is unchanged. Idle exhaustion, manual selection, failed
requests, empty trigger presses and diagnostic counters cannot request replacement.

The helper retains paired Remote views/resources, base Charge ownership,
living on-foot armed state and no player form. It additionally requires the
full current player handle to resolve to the actual object and entity in both
registries, match the damage owner and retain its body. Retired/hidden objects,
nonfinite health, death, cutscene, holster and reload exclusions decline. There
must be no pending throw. The actual named Detonator ID must be in the admitted
catalog, distinct from Charge and have positive capacity with the same valid
mapped ammo bank. Both its loaded count and the shared reserve must equal zero;
negative inventory is not treated as exhaustion. No magazine-positive gate is
added to this reserve-only weapon, and separate Detonator ownership is not
invented.

`rf_weapon_decide_empty` receives that actual Detonator as current/block ID and
the actual Charge exclusion ID. The helper passes the existing factory policy,
catalog descriptor flags and 32 authored ranks filtered by
`scene_weapon_empty_rank_slot`. Thus current no-switch flags, real candidate
ownership/ammunition, resources, Grenade admission and retained Machine Pistol
mode behavior remain under the established owners. It adds no rank, fallback
weapon, ammo grant, option or resource load. If no candidate qualifies,
selection stays unchanged.

## Explicit port policy: full retirement and action deadline

The new block predicate considers every active exact Charge-type record owned
by the actual player, regardless of attachment, fuse, life or object flags.
Flying charges and already-requested-but-not-retired charges therefore block
ranked replacement. This is deliberately stricter than original `4c9e30`'s
ready-charge predicate and is separate from the existing positive-reserve
selection remap.

An accepted request on frame N normally retires its charges in the next world
tick, N+1. The installed `fp_rmt_det_fire.rfa` action rounds to 467 ms, already
represented as 29 simulation frames. This extension waits until N+29 after
world processing before considering replacement. The original calls the empty
consumer immediately after scheduling presentation and uses its queued
selection dispatcher. Full-action N+29 replacement at the port's established
immediate selection/view boundary is integration policy, not exact retail
queued timing or subframe parity.

The due target/countdown is consumed before the new helper, including all
declined/error outcomes. It never leaves an unsaveable target-1/ticks-0 pair or
retries from idle. A real replacement uses the existing
`campaign_select_primary`, `scene_player_auto_select_inputs`, idle-view/ammo
publication and changed return, ending handheld input for that frame. Held
fire, alternate and reload require release before the new weapon can act.
Independent charges, host attachment, damage/terrain processing and cooldown
remain untouched.

## Persistence and scope

There is no new retained owner, timer, allocation, checkpoint field or format.
RFRM2's existing successful-action target/countdown is the only provenance;
selection cancellation, staged restoration after `campaign_select_primary`,
RFRM1/absent-component behavior and existing save guards are unchanged. Restore
does not run this helper: a restored valid pending action still completes at
its ordinary future combat deadline.

The helper is defined within the existing late Remote include; the shared
rank helper is already defined earlier in scene.c, so no scene wiring changes
are required. Parent owns integration, top-of-file TO-DO reconciliation and
the scheduled Xbox batch. Empty Detonator animation, generic post-draw
selection, missing-resource follow-up admission, retail queued presentation
and runtime/save-continuation validation remain outside this slice.
