# Independent review: no-dwell Use-trigger edge

Source-reviewed against frozen53dd1a928ca0168e854a8e5b5cf870910098b040 on 2026-10-10 at15:53 UTC. No active-repository edits, builds, syntax checks, tests, runtime, fixtures, routes or broad campaign scan. The review covered `use-trigger-edge.patch` and its staged `src/diagnostic/scene.c`.

## Verdict

No source blocker found in the bounded staged patch. Keep outside the active checkout until the parent's already queued16:00 batch completes. Compilation and all action behavior remain unverified.

This is substantive interaction behavior: the current held-input fallback can submit another usable door/lid activation once a cooldown or mover completes. The earlier NPC kind9 lead remains deferred persona-response work; do not combine a talk handler with this patch.

## Actual qualified owners

`actual-zero-dwell-door-owners.json` records exact original selected L3S1 records:

- Trigger12: Use flag1, enabled tail_flag0, unlimited activations, cooldown0, values[0,0], link32 to `1st_aid01_door`.
- Trigger20: Use flag1, enabled tail_flag0, unlimited activations, cooldown0.25s, values[0,0], link7004 to `big_crate_lid`.

The current runtime binds values[1] to contact_timer.seconds at core/event.c:1802. Known L11S3 trigger6298 likewise has Use1, enabled tail_flag0, unlimited,6s cooldown and zero dwell, with Load_Level6290 and real door controllers; its paired2658 begins disabled. Those existing airlock facts are in docs/AIRLOCK-EQUALIZATION-SOURCE.md:37-62. No route was run.

## Existing source ownership and consequence

- Original action2/type0 press query43d0a9/43d4f0 reaches4a6210 ->4a6493 ->4a4970 ->4a29f0. Consumed actor Use returns before fallback; unconsumed fallback4a1970 calls ordinary4c0100 and linked4c04e0 Use-trigger paths. This is documented with disassembly in known-gameplay-next/MEDIC-USE-SOURCE-CONTRACT.md and MEDIC-DISPATCH-SPEECH-ADDENDUM.md.
- scene_use_edge.inc:27-31 currently passes filtered held input to unclaimed fallback. scene.c:20857 passes this to the sole ordinary player contact pass.
- core/event.c:193-221 accepts Use-bit triggers whenever the supplied input is nonzero, subject to existing state/count/cooldown gates. fire_sp at2077-2093 dispatches links, increments count and arms cooldown on each accepted visit.
- scene.c:3452-3474 forwards linked group activation to the actual mover owner. core/level.c:1262-1280 admits another activation after next_key returns to-1. Thus repetition reaches physical door/lid behavior, not merely telemetry. Exact runtime outcomes are not claimed.

## Patch review

The local trigger_use starts from the existing consumed/filtered value. It additionally requires scene_use_edge.edge only when this is the player pass, preparation occurred, state.flags has Use1, and contact_timer.seconds<=0. The same local input feeds the optional reach probe, rf_runtime_trigger_contact_cached, and rf_runtime_trigger_contact_authored_cached.

This preserves:

- Raw press sampling before modal/dead/turret filtering and current owner/claim checks.
- Initial frame and successful-load suppression already owned by scene_use_prepare and scene_medic_load_committed.
- Rejected presses remaining rejected while held; later reach, enable, cooldown expiry or ownership changes cannot synthesize a new no-dwell press.
- Every automatic/NPC contact visit and current per-trigger geometry/timer/filter processing.
- Pending airlock service: scene.c:3619-3623 calls scene_airlock_tick independently of input. Once accepted, scene_airlock.inc:349-393 completes its retained actor/time transaction without rechecking Use.
- Existing positive-dwell approximation and nonprepared legacy path; global scene_use_trigger_input is unchanged.
- Existing finite timer checks and save formats; there is no new latch, mutable state, allocation or serialization.

The <=0 predicate matches the existing immediate/no-dwell branch. Positive dwell requires consecutive accepted polls (core/event.c:138-161); blindly edge-gating the global helper would clear its deadline on the next frame. That broader behavior is intentionally not reconstructed here. NaN and infinities still reach the existing finite guard; the patch does not normalize them.

## Handoff limits

This is source review only. The patch is not integrated, compiled or runtime-qualified, and it does not prove current route reachability. No new fixture or test was created. Parent owns integration, serial Xbox batch and cleanup.
