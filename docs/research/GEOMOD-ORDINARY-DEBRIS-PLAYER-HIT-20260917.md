# Ordinary flying debris strikes the player

The ctf06 quiet DEV replay now exercises player contact without any injected
fragment or damage fixture. tools/replay_debris_player_hit.py generates400
ordinary RFI6 input frames, runs the PC replay with all other replay flags
cleared, and verifies captured contacts against original48f678 actor-loop code.

Recipe: select Rocket Launcher with four next-weapon presses, aim at the upper
post (targetY2.5), fire on frame240, then walk forward from255 through284.
The actual rocket blast occurs on249. Fragment slot6 hits on287, slot3 on291;
both retain room index3 and enter with suppression flags0. No jumps, teleports,
health overrides, injected contacts, seeded particle fixtures or asset edits.
The generator's initial eye and westward contact-plane aim use the established
ctf06 DEV spawn values. Input SHA256:
beb120ef4e7aa4d0fef7ac87e0dc305e8d04ca9169694a9eeb73fffcc913ea2d.

Actual contact tracing records positions, velocities, radii, player body/radius,
requested damage and fragment identity. Both original overlap decisions and
scalar words match exactly. Original actor lookup is supplied; damage, blood
and player-direction services are recorded/supplied. This is not execution of
the original full world, health system or trajectories. Actual game health
uses the already reconstructed shared player damage owner.

PC result: one rocket self-blast, then two distinct fragment damage events
(.753861 and.718904 requested). Final health42.393387. Contact telemetry is
[239,2,2,1060637214,0,2,1110020820,2]. Two billboards and44 drops are emitted
with no pool exhaustion. The injected-fixture telemetry remains all zero.
The journal retains the later debris events separately from the earlier blast.
The shared flags suppress subsequent impacts by those fragments.

Generated inputs, detailed log, original comparison and endpoint are in
artifacts/debris-player-live/. The trace-only addition has no effect on the
ordinary simulation when tracing is off. The native report verifier requires
the exact input hash, positive hit/effect counts and disabled fixture state;
equal zero counters cannot pass.

## Native acceptance

artifacts/xemu/render-20260917-012340 completes400frames and passes all66
comparisons on stock64MiB. The additional ordinary-hit verifier passes against
its report: no fixture, two real fragment hits, two billboards/44drops and
exact matching final health and effect RNG/hash.4084pages remain free
(15.953125MiB). Disc restoration completed and the owned emulator exited.
Native endpoint inspection confirms the approached post/ceiling, weapon and
reduced health/armor HUD. Blood has expired by this endpoint; transient blood
rendering was verified separately in the prior effect integration capture.

Remaining: repeated-blast suppression/retained flags, debris-caused death,
ordinary contacts at more distances/angles, live particle pool exhaustion.
Estimate remains approximately50% overall /75% GeoMod.
