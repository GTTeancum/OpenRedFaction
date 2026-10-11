# Independent form reload-owner review

Date: 2026-10-10. Baseline: `65490a235229cafdff24c94c9a40ddf2bfefffa5`.

## Verdict and exact artifacts

Approved for parent integration, source-only, of the exact one-file candidate:

- `reload-form-owner.patch`: SHA256 `8ef15fb727d12a41ae56eaeb9d733f1545ba41ace1f1a826d03df5c20677ab36`
- staged `src/diagnostic/scene.c`: SHA256 `c3a1b895daf811380d4c44c39cc00181a556074b3352e71636f6f8fc9dac7931`

The patch adds two guarded assignments to the existing generic ordinary reload countdown, plus comments. No other executable behavior is changed. No active repository edits, builds, syntax checks, tests, game/emulator runs, routes or fixtures were performed by this review.

## Original actor ownership

The new evidence closes the reset prerequisite left open by the earlier separate form finding:

- Form entry 4b018d and exit 4b0402 call 4a4130 to replace the actor. 4a4130 calls actor factory 422360; 42272d pushes type 0 into 486da0, which calls 487100. The type-0 jump-table entry at 487330 is 48718f, and 4871b2 calls constructor 40e380.
- Constructor 40e441..40e45d initializes actor deadlines +139c, +13a0 and +13a4 through 4fa340. Independently reread 4fa340 writes -1. The actor-create tail zeros EDX at 4227bb and writes it to actor+810 at 4227fd, clearing the actor reload-active bit as part of construction.
- The bounded form routines copy selected vitals, pose and identity values but do not carry these reload deadlines/active bit into the replacement. 42cd60, independently reread, resolves and delete-marks actor+75c's attached object, clears that handle and arms +514; it is not a reload/inventory-copy owner.
- Form selection is direct or force 1. Ordinary force-0 request refusal therefore does not belong here. The replacement actor resets reload even when the eventual selected weapon ID is unchanged. Independently established ordinary mismatch service also cancels rather than completing a reload against a different primary.

Evidence is in the adjacent `original-*.txt` excerpts and the imported form entry/exit material. This qualification concerns the ordinary reload's retirement only; it is not a claim of whole form-replacement parity.

## Success-only placement and finite behavior

Entry clears only after all existing context/time, duplicate-active, player-view, resource, definition and `rf_weapon_acquire_sp` success checks. The acquisition wrapper's SP notice is a no-op and does not change selection or dispatch another callback. Exit clears only after the existing inactive-form early return. Both placements still observe the outgoing selected slot and precede destination selection.

The existing `campaign_reload_blocks_ordinary_selection()` predicate is reused unchanged. It identifies a positive combat[6] countdown only in ordinary slots 0, 1, 3, 4, 6, 7, 13, 14, 15 and 16. Thus failed or already-satisfied form requests retain the old countdown; accepted ordinary form replacement retires it. An equal old/new weapon ID is deliberately not exempted.

Only combat[6] is cleared. No load, reserve, reload-transfer, refund, grant, combat burst, raw button latch, renderer action or shared-charge state is added or changed. The pre-existing authored Undercover acquisition remains governed by its own real finite inventory operation. Cancellation cannot complete the old reload against either weapon. A later genuinely admitted reload from current input remains governed by the existing admission path.

## Persistence and remaining boundaries

`scene_checkpoint_player_state_scope` already refuses an active ordinary combat[6] countdown. Its explicit world-profile exceptions are selected Flame/Fusion reload owners, which this predicate excludes. The patch changes no capture, restore, wire version or legacy decoding; it introduces no persistent identity/timer lane. Once an accepted replacement consumes the ordinary countdown, there is no pending old reload to serialize.

The patch is separate from the ordinary manual/pickup request veto and does not place a guard or cancellation in the generic selector. Riot, Flame, Fusion and other special reload semantics remain outside this ordinary-only correction. Full original actor/model replacement and residual FP presentation remain existing separate work. The known L6S3/L8S1 authored chains establish a real in-level form boundary, but no runtime overlap with a positive reload was fabricated or observed in this review.
