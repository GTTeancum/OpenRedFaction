# Live physics/event CPU work

Written 2026-10-08 for the parent-coordinated 16:00 Xbox batch. No build,
fixture, emulator run, or performance test was performed for this delta.

The existing 15:00 stock-64-MiB L1S1 profile reports 273/223 ms (1.224 ms)
for controller/player physics, 670/223 ms (3.004 ms) for events, and 219/224 ms
(0.978 ms) for the trailing collision diagnostic stage. These are historical
phase timings, not measurements of this change. NPC playback is separate.

## Diagnostic collision views

`campaign_collision_views_check` rebuilt the player and every registered NPC's
collision view, collision response and extra velocity solely to populate audit
arrays, even after live-play checksums were disabled. It now skips that audit
loop when diagnostics and the explicit actor-pair fixture are both disabled.
Frame-zero diagnostic initialization and all explicit fixture calls remain.
Real sweeps/contact consumers independently construct their current owner
views where needed and are unchanged. No actor simulation is skipped.

## Exact trigger contact memoization

The event pass visits player then live NPCs in the existing order. Eligible
stationary actor/trigger pairs repeatedly evaluate the same sphere or oriented
box contact, including x87 coordinate transforms. The scene now supplies a
bounded 256-entry geometry cache (29,696 bytes on Xbox, no heap or per-frame
allocation) through an optional shared event API.

A successful geometry result is keyed by all bytes of the current trigger
volume, all three actor pose points and box flag0x20. Handle hashing chooses
only a cache slot; it is not evidence that a result is reusable. Collisions
replace entries and only affect speed. Changed position, orientation, size,
shape, radius, directional flag or pose forces recomputation. Identical values
may safely reuse a result across owner/level/restore transitions because the
geometry function is pure; scene entry also clears the whole cache. No world
geometry or topology participates in these trigger-volume tests.

Registry lookup, original4c06d0 eligibility, activation limits, flags, input,
actor filtering and original4bfc60 contact-delay processing still execute on
every visit. In particular an ineligible or non-contacting actor still clears
a positive dwell deadline, and delayed eligibility is never memoized. Flag4
still bypasses geometry; callbacks and linked effects retain their order.
Failed geometry calls are never cached. Cache contents are neither persistent
nor authoritative gameplay state and require no save-format change.

The existing ordinary `rf_trigger_contact_poll` and
`rf_runtime_trigger_contact` APIs remain uncached. Cached geometry uses the
same reconstructed helpers: original4bf620 inclusive squared-radius sphere
contact and original4c0a80 box/forward-face contact. It changes no thresholds,
movement rates, collision algorithms or mission event semantics.

Integration touches `event.h`, `event.c`, and three isolated `scene.c` seams:
trigger contact loop/cache declaration, audit loop gate, and scene-entry reset.
