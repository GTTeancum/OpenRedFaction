# Liquid-aware flight preparation (2026-09-15)

Shared `rf_weapon_flight_step_liquid` separates one liquid entry from a possible later solid impact in the same tick. It uses the recovered weapon-contact timing, clears liquid query state, handles exact endpoint contacts without an entry effect, and reports effect metadata separately. Solid-only behavior retains the existing straight-flight policy; gravity, homing and general collision response are outside this API.

Tests cover water then wall, water-only, zero-fraction entry, endpoint entry, torpedo expiry, second-query failure, repeated-liquid rejection, inactive/zero-dt behavior, lifetime clamping and unchanged dry behavior. `weapon_explosive_fields` passes. PC playable and NXDK builds pass.

The extended world sweep returns authored surface flags through an optional output without changing existing guest diagnostic record layouts. All44082 sweeps across94 installed levels pass ownership, flags and unloaded-source replay checks.

Attempted live hookup found `sweep_rooms_prepared` explicitly rejects query1000. That hookup was removed; scene gameplay remains on the existing dry path. The dry900-frame two-blast run reproduces the8934-byte destruction checkpoint exactly; its actual output was inspected and still shows the known dark crater. No working live water collision, ripple rendering or water audio is claimed.

Next: recover and implement the original room-liquid face pass, preserving its eligibility and hit discriminator, then connect the new flight API. Do not merely remove the unsupported-query guard. Original-game screenshot comparison is not part of this work.

## Native regression after room-pass integration

The later room-pass implementation is committed and live rocket dispatch uses it. XEMU run `artifacts/xemu/render-20260915-215217` completed260frames and47 comparisons on67108864-byte base memory with no expansion, retaining8575 free pages (33.50MiB). The native framebuffer was inspected: room, rocket weapon, HUD and one crater are present; the known dark crater appearance persists. This is a dry destruction regression, not acceptance of wet-scene ripple visuals or audible splash playback. The owned process exited and all saved disc entries were restored.
