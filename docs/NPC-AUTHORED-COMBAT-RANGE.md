# Bounded placed-NPC combat range override

Parent source-reviewed October 13, 2026. Compiled in the 07:00 UTC stock64 build on 044cbeabfa47b9300d1c6233c8f1128b0b48c9f0; minimal startup smoke passed with 13 simulation / 10 presentation frames and clean shutdown. No functional test or coverage claim.

## Source and authored demand

Original464563 reads the final placed-entity override byte and46457c reads the optional float. Original464c36/464c3c install actor7c8/7cc, equivalent to AI528/52c.401cc0 checks that byte exactly against1 before weapon selection. Both the primary firing gate401aca and pursuit limit4077a0 consume401cc0. Applying this input to pursuit alone would allow stopping beyond the port's unchanged firing admission range.

The v180 retained record already stores and validates the final17-byte flag block, its last override switch and optional4-byte float. The new rf_level_entity_combat_range_read accessor validates the full record, projects that existing immutable input, preserves output on error, and reports the original disabled default15. It supports finite positive enabled values up to1000000, matching the existing authored weapon-range bound. No spawn structure or serialized layout changes.

A read-only survey of94 supplied levels found185 enabled overrides, all finite and positive (2 through900).118 intersect the prior tactical candidate inventory across29 levels. These are static source counts, not live admission counts or functional coverage.

The weapon-table scalar is independently traced: row base85cd08, parser50c/510 stores the two "$AI Attack Range:" floats; default finalization4c2a20 copies the first into active514, addressed by401cc0 as85d21c+ID*550. All supplied pairs are equal. Original IDs<=0 retain the source.5 fallback; this consumer only applies to selected positive weapon IDs and does not remap the table.

## Bounded live scope

At the existing combat traversal, the override is read only for an alerted actor with either an existing matching RFNC40 delay receipt or the existing fully qualified authored Stand Ground consumer, a selected ordinary primary nonmelee weapon, positive weapon ID and ordinary movement mode. Fixed-point orders and vehicle targets are excluded. RFNC40's immutable class/style and full target relation qualification remain authoritative for Direct/Evasive owners. Stand Ground reuses its existing identity, model, source style and live target checks and still never starts a pursuit. Its qualification is deliberately reevaluated at the original stop site after tactical service, rather than caching a possibly stale result across intervening callbacks.

An enabled value replaces the local firing-admission and pursuit-decision ranges together. Disabled records retain their previous behavior. Current15-frame pursuit decisions,0.8 pursuit hysteresis, bounded sight sampling, delay clock, navigation and retry behavior remain explicit port adaptations. No full original1/2/11 pursuit state-machine or short-retreat claim is made.

The pre-override weapon range remains the shotgun ray-reach input. Point-order ray reach, projectile lifetimes, damage, spread, paid attack release, ammo/reload, secondary fire, tactical local-route range, stronger movement ownership and player weapons are unchanged. New primary admission may occur at the authored override distance; existing weapon-specific flight/contact rules still determine whether an admitted shot reaches anything.

## Lifetime, save and memory review

The accessor performs only bounded raw-record reads and scalar validation. It adds no resource demand, queries, callbacks, allocation, owner mutation or RNG. The live call reads the existing section's placed record by the current owner slot, checks record bounds and completes before subsequent callbacks. The result is local to this combat service; no borrowed record pointer escapes.

There is no new mutable actor state to capture, restore, seal, rebase or publish. Existing checkpoints retain their route/combat/RFNC40 target state and bind the same immutable level records after load. The override is recomputed on the next eligible service; capture and final-close code do not seed or mutate it. No checkpoint version or compatibility gate is changed.

Persistent/heap/save-payload delta is zero. The accessor adds an8-byte output and bounded scalar/read-only validation stack, conservatively below512 bytes for the additional nested source-level call path including caller locals. This is a source bound, not compiled stack usage. Existing actor, restore, shared seal, late-close, resource and byte budgets are unchanged.
