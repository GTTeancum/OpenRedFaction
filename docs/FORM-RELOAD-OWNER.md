# Accepted player-form replacement retires the outgoing ordinary reload

Baseline: 65490a235229cafdff24c94c9a40ddf2bfefffa5. Date: 2026-10-10. Staged outside the frozen active repository; parent integration is after the 19:00 batch. No tests, builds, syntax checks, game execution, routes, forced events or inventory fixtures were performed.

## Finding and bounded fix

The already-known L6S3 trigger6937 → Invert6939 → Go_Undercover6938 OFF and L8S1 trigger6446 → Go_Undercover6447 ON can reach real in-level form changes. Their actual trigger volumes are away from the starts; this does not establish that any runtime route exercised a reload there. The copied authored records and independent qualification are in evidence/.

Current `campaign_set_player_form` changes selection without clearing the outgoing generic reload countdown. Later completion uses `campaign_selected_weapon()` and can therefore apply the old countdown to the destination weapon's finite magazine/reserve. Neither ordinary request refusal nor background completion correctly represents this forced actor-replacement boundary.

The one-file candidate adds only two guarded assignments. Reuse `campaign_reload_blocks_ordinary_selection()` to identify the existing positive ordinary-gun countdown under its actual outgoing selected slot, and clear only `rf_scene_combat[6]`:

- Entry: after every existing early return and successful `rf_weapon_acquire_sp`, before form mutation or destination selection.
- Exit: after the existing inactive-form early return, before destination selection.

This deliberately also cancels same-weapon form replacement. There is no old-versus-destination ID inequality requirement: the original replacement is a new actor even when the weapon ID happens to match.

## Original construction and cancellation evidence

Supplied RF.exe SHA256: b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836. Only the bounded already-selected form/create/constructor/reload chain was disassembled; no binary execution occurred.

- Entry4b018d and exit4b0402 call `4a4130`, which invokes actor factory422360 at4a41d3 and publishes the resulting full actor handle at4a4343..4a4349.
- Factory42272f calls486da0 with type0. That factory calls487100 at486e1f. Type0 table entry487330 is48718f; it allocates1494 bytes and calls40e380 at4871b2.
- Constructor40e441..40e45d invokes4fa340 for reload fields+139c, +13a0 and+13a4.4fa342 writes-1. Thus the replacement has no retained old finish, event or drain deadline.
- Factory4227bb zeroesEDX and4227fd stores it into actor+810, clearing the reload-active0x100 bit. This is explicit initialization, not an inference from calloc or uninitialized heap memory.
- Form entry/exit selectively copy pose/vitals and other named fields; the bounded functions do not copy the old reload owner. They select either directly or with force1 at4b02c3/4b04ea. The ordinary force0 request veto must not block this form change.
- Helper42cd60 is not an inventory-copy routine. It resolves the attached object at actor+75c, marks it for deletion, clears75c and sets timer514. Its shown body does not transfer reload reservations or ammunition.
- Original42550f captures the real reload weapon at actor+13b0. Service419d27 compares it with the current primary; mismatch cancels ownership/timers before completion. Finish419da6..419dcc writes the previously staged loaded/reserve values only for a matching owner. No inferred transfer from an old reload to a new form weapon is authorized.

## Finite ammunition, failure and save ownership

The cancellation itself never writes loaded ammo, reserve, ownership or a grant. It does not complete or refund the old reload. It preserves the port's existing real Undercover acquisition and finite reload-transfer policy, without trying to reproduce the original full replacement-inventory lifecycle. The current SP acquire wrapper performs its existing ordinary inventory operation and no selecting callback; the guarded cancellation observes the still-selected outgoing weapon after success.

Invalid context/time, duplicate entry, unavailable Undercover resources, invalid weapon metadata, failed acquisition and inactive exit all retain their existing early returns before cancellation. A rejected/no-op form operation therefore does not lose the reload. Accepted forms retain the current health/armor/flags and return-slot behavior. The patch does not turn a forced form change into a refused ordinary request.

There is no new retained state or save schema. Existing `scene_checkpoint_player_state_scope` refuses an active ordinary reload; its independent Flame/Fusion exceptions and all other input/campaign guards are unchanged. No saved ordinary reload needs migration or fabricated weapon identity. Successful form cancellation removes the actual pending gameplay obligation, rather than merely relaxing a save guard. The patch does not touch candidate loading, final-close publication, failed-load behavior, projectiles, delayed contacts, bursts, shared FP/f90 or renderer playback.

The ordinary-only predicate covers slots0,1,3,4,6,7,13,14,15,16. Special Riot/Flame/Fusion reload controllers and full original player actor/model replacement remain outside this slice. The current body and generation handle are retained by the existing port form implementation; clearing the outgoing reload at this verified boundary is an explicit compact port projection of original actor replacement, not a claim that all original form construction is implemented.

## Delivery

`reload-form-owner.patch` and full modified/base scene.c are included. The independent review supplies the exact source approval. Parent owns integration, milestone updates and scheduled executable validation.
