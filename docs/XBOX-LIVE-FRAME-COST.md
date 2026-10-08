# Xbox live-frame cost reduction

Current priority,2026-10-08: input response, frame pacing and the playable core
loop. Campaign additions are paused. These changes are written, not timed yet.
The parent owns the15:00 consolidated build and existing campaign timing run.

## Concrete work removed

1. `renderer.c:preview` previously copied the CPU mesh into PAGE_WRITECOMBINE
   GPU storage, then read that uncached storage twice to classify each vertex
   and alter its colors. Both exact color rules now run on a cacheable local
   vertex before its single complete GPU write. Source mesh, material binding,
   ordering, flush fence and buffer lifetime are unchanged.
2. Shared large diagnostic hashes can be disabled separately from simulation,
   counters, RNG and save integrity; see LIVE-FRAME-CHECKSUMS.md.
3. Xbox `scene_present` previously invoked `group_storage_check` after every
   presentation AND every skipped presentation. That rehashed immutable
   trigger/event/entity/group payloads, initial runtime copies and membership
   diagnostic owners. Live play now skips those per-frame diagnostic scans;
   setup/teardown validation still runs, as do normal gameplay owner checks.

## Mode and timing evidence

Normal unlimited live controls opt out of payload checksums. Existing replay
and bounded defaults keep them enabled. The optional disc file
`diagnostic-checksums-off.flag` selects the identical fast path for an existing
bounded timing run, without adding a gameplay fixture. The one-time console
line `SCENE_DIAGNOSTIC_CHECKSUMS` and exported mode report the actual setting.

Keep the same level, source input, frame count and viewport for a comparison.
Existing `rf_renderer_profile[2]` covers dynamic vertex upload/normalization;
`rf_scene_profile`, `rf_scene_world_profile`, `rf_scene_presentation_profile`
and `rf_scene_npc_playback_profile` retain their timing counters. Diagnostic
hash values are deliberately unavailable in the fast mode and must not be
reported as parity matches. No measured speedup, FPS or gameplay-readiness
claim is made before the parent timing pass. No helper tests, new harness,
images or input automation were used.
