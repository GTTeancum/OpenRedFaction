# Flamethrower primary fuel-debit automatic selection

Integrated and independently source-reviewed against `ae64a383` on 2026-10-10.
Compilation and runtime remain unverified pending the scheduled Xbox batch.
No builds, tests, syntax checks, emulator runs, new fixtures, routes, original
input edits, ammo/timer changes or save-format changes were performed.

## Original evidence and bounded projection

The supplied read-only RF.exe 1.20 NA (SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`)
and installed tables support these separate boundaries:

- Local continuous-fire update `4aaf5d..4aafca` requires the flame descriptor
  bit, action 2 and its fuel timer; the actual `4257c0` debit is at `4aafa8`.
  This is continuous fuel accounting, independent of the damage pulse.
- Ordinary entity update's empty-active path `419bed..419c9b` reaches weapon
  reset `41ae70`; its local-player callback at `41b02c` invokes `4a6f10`.
  The original depletion-to-selection chain therefore includes active reset.
  It is not evidence that every primary damage pulse directly selects.
- The shared depleted-weapon decision uses actual loaded plus mapped reserve,
  descriptor no-switch flags, authored preference order and ownership. The
  established factory defaults remain Autoswitch on and explosive defer off.
  See WEAPON-EMPTY-SELECTION.md for the exact selector and default stores.

This adapter projects that active-empty behavior to the existing successful
outer combat boundary in the same port frame as an actual positive primary
fuel debit. It does not recreate the original timer/update scheduling, full
reset dispatcher or queued draw/blend timing. Idle ammo polling and all dry
attempts are outside this slice. Positive reserve retains the existing Flame
reload owner rather than requesting replacement.

## Live capture and shared gates

The same 16-byte call-local request receives a distinct
`SCENE_WEAPON_EMPTY_FUEL_DEBIT` operation. Its enum, typedef and exact note
prototype precede the earliest Flame producer include; no retained event,
allocation or diagnostic counter is introduced. The existing Fusion SHOT and
conventional/paired SHOT/DRY consumers remain separate.

`scene_flame_input_tick` passes the request only to an admitted primary tick.
Immediately after `rf_weapon_charge_step` succeeds with consumed>0,
`scene_flame_tick` qualifies its actual weapon as `campaign_flame_id` and
records the request before ammo publication or contact/damage callbacks.
A pulse without debit cannot request selection; a debit without a pulse can.
Reload discard/transfer, ignition delay, rejected/inhibited input, idle and dry
cannot request it. Existing direct helper callers may pass NULL to omit capture.

Both note and consumer pair FUEL_DEBIT exclusively with selected slot 10 and
its exact Flame ID. SHOT/DRY cannot admit Flame, and FUEL_DEBIT cannot admit a
conventional, paired or Fusion outgoing weapon. Selection runs only after
`campaign_combat_tick` returns RF_OK; all outgoing effects, sweep, visuals,
hearing, contacts and damage finish first. Any later combat error prevents the
consumer. It rechecks the captured full player handle, registries, body and
damage owner, current slot/weapon, finite positive health, life/hidden flags,
on-foot/form/cutscene/holster/reload gates and exact ownership/resource admission.

Loaded and mapped reserve must both be exactly zero. Negative malformed counts
are declined. Authored ranks, candidate resources/views, retained Machine Pistol
mode, no-switch flags and the shared held-input release gate are unchanged.
No ammunition, ownership, preference or candidate is fabricated. The replacement
cannot execute input or fire again during the completed combat call.

## Ordinary off-selection state and RFAP

Only after a real `campaign_select_primary` replacement, the consumer calls
`scene_flame_input_stop` at the actual completed combat position, including an
already-staged eye. Its named release Foley is independent of the newly selected
weapon descriptor, and repeated loop closure is safe. The consumer then clears
reload bookkeeping, pending throw and `scene_flame_active`. This immediately
applies the existing ordinary off-selection semantics; no second gameplay tick
or blanket `scene_flame_input_reset`/`scene_flame_canister_reset` runs.

Fractional fuel, damage cadence, alternate held/cooldown, live thrown tanks and
burn owners remain intact. No candidate, declined request or failed combat
means no adapter cleanup. Existing RFAP admission requires ignition, reload and
pending throw to agree with selected Flame, while it permits fractional/cadence,
alternate history and independent canisters off-weapon. Cleaning only the
selection-bound fields avoids an inconsistent same-frame save without weakening
admission or changing a byte of the save layout. Cosmetic particles already
submitted by the outgoing operation retain their ordinary lifetime.

## Explicit exclusions

Alternate depletion is not added. The original alternate admission requires a
full reserve tank, then delayed commit `42c362..42c386` replaces loaded fuel with
the full magazine and debits that amount from reserve before the existing empty
callback. The later source-written full-reserve admission correction removes
the former partial-refill/zero-reserve throw policy; see
FLAME-CANISTER-RESERVE.md. Successful releases retain a full loaded tank, so no
alternate ranked-replacement request is introduced. The existing
successful-spawn-only debit policy remains separate from original behavior.

Flame dry handling, original selection/reset timing, arbitrary saved Autoswitch
preferences and full original presentation remain deferred. Primary exhaustion,
held-input release, release audio, retained canister/burn behavior and same-frame
save/load remain uncompiled/runtime-unverified. The two existing direct-call
Flame test files receive only mechanical NULL arguments for the new optional
request parameter; no test case or fixture was added or executed.
