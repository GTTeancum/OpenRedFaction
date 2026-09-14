# L2S3 gameplay blockers

The L2S3 spawn replay exposed three independent blockers rather than a missing
level asset. The level had already loaded and drawn its first PC frame.

1. The first simulation tick reached a particle with flags0xd119b, including
   collide bit0x10. The strict free-flight implementation returned
   RF_NOT_FOUND(-3). Earlier notes incorrectly called this RF_RANGE; that code
   is-4. The new world-aware simulation path keeps the existing list/lifetime
   behavior and supplies a solid-world point sweep for colliding particles.
2. After that fix, the second tick reached the PONR Block mover, whose authored
   key8207 has zero travel time. Translation integration divided by zero before
   the existing instant-arrival branch could run. Zero-time integration now
   supplies a finite completed distance and zero speed, allowing the ordinary
   position/arrival/dwell logic to complete the key.
3. Xbox rejected the combined image payload before GPU allocation:18,677,764
   bytes were already loaded, above the old16MiB renderer check. The combined
   owner contains world plus separately loaded actor, clutter, weapon and pickup
   images. Its cap is now20MiB, enforced by both PC play and Xbox rendering.
   This changes an accounting check; it does not duplicate images, change their
   resolution, increase physical Xbox RAM or prove every section will fit.

## Particle behavior and limits

The existing strict simulation entry remains available without a collision
world. The gameplay scene uses rf_level_particles_simulate_world, borrowing the
retained collision world only during the call. No per-particle or frame heap
allocation is added. Lookup callbacks, list ordering, expiration and bounds
updates retain their existing behavior.

The practical collision response sweeps from the old to proposed position,
stops just outside the first solid hit, reflects the inward normal velocity and
damps tangent velocity. Packed restitution and stickiness nibbles are normalized
over15 as first-pass tuning. This normalization and single-hit response are not
claimed as original numerical parity. Invalid contact data or a failed query
preserves the particle and its bounds. Collide-and-die sets age to life for
recycling on the following step, except the original flag0x80 exception.

Original evidence: RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`;
495120 calls495880 for bit0x10 instead of free movement.495880 performs collision
queries, sets contact flag0x8000 and handles the0x800/0x80 death condition.
497afe..497ba2 packs restitution/stickiness into bits16..23. Bounded Ghidra
exports are retained locally under artifacts/analysis. Multi-contact sliding,
moving-object hits, liquid effects, impact callbacks, swirl, wind and damage
remain separate completion work. Unsupported non-collision modes still fail
explicitly; this change does not silently drop their effects.

## Checks

`gameplay_effect_collision` covers sweep misses, impact, restitution, full
stickiness, delayed recycling, callback/invalid-normal failure preservation,
zero-time arrival and ordinary positive-duration movement. This is meaningful
behavior coverage, not a full original-executable oracle.

`python tools/replay_l2s3.py` checks natural startup and an explicit Alarm2102
setup, each120 frames at the authored spawn. Both complete119 simulation ticks,
165 particle creations,41 expirations,124 live particles and two mover arrivals.
The alarm case starts once after its0.25-second delay. This is not a walked
mission or complete encounter; its non-NPC link is skipped by the current alarm
wake service.

Native failures remain recorded in render-20260914-174209,174426 and174743;
the last identifies the old image cap with renderer diagnostics
[1,18677764,3145728,0]. The final stock64MiB run render-20260914-175145 passes120 frames and all
selected PC/native gameplay comparisons, including particle totals/RNG and
mover statistics, with4756 free pages (18.578MiB). The harness now compares particle simulation totals/RNG and live
mover statistics alongside existing gameplay fields.

Reproduce the native fixture:

```text
python tools/xemu_render_check.py --input artifacts/l2s3-replay/neutral.bin --spawn --level L2S3.rfl --setup-uid 2102 --seconds 240
```

Failure diagnostics: scene stage201..206 identifies NPC, clutter, world weapon,
pickup, first-person weapon or platform presentation failure. Xbox renderer
stage1 identifies pre-allocation validation;2 device initialization;3 vertex
allocation;4 descriptor allocation;1000+N texture uploadN;5 subsequent drawing.
Its other three words report referenced image bytes, GPU vertex capacity and
free pages before GPU vertex allocation. They are diagnostic evidence, not a
replacement for final in-level memory measurement.
