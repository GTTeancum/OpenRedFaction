# Immutable dry-sweep validation reuse

Post-16:00 source candidate, based on frozen a653795. No builds or tests run.
The earlier controller/player-physics phase was only about1.2ms; integration
priority should follow the parent's new profile, not an assumed saving.

The scene now retains the existing bounded `rf_collision_sweep_batch` between
dry body queries over its currently published static collision world. It
reuses room/list/node validation and immutable face-block bounds. All actual
queries, traversals, contacts, material resolution, response, and current mover
transforms still run. Alpha-aware and liquid queries retain their old path;
explicit caller-scoped batches and synthetic/staged worlds also retain theirs.
The original public geometry APIs are unchanged.

The cache occupies2,788bytes on32-bit Xbox, including its world binding, and
allocates nothing. The existing bounded overflow behavior remains: trees not
retained in the16-entry table validate normally. Every query still checks its
own floating-point inputs, moving solids, traversed faces and contacts. The
core body-query context now avoids clearing its unused local batch when an
external batch is supplied; its contact accumulator is always initialized.

## Invalidation ownership audit

The supported live static world has these publication paths:

- Scene entry and mover/collision teardown: discard the binding and batch.
- `scene_terrain_bind`: reset before the DEV terrain overlay bind.
- `scene_terrain_publication_finish`: reset before the first authored overlay
  bind, including the L1S2 base-room copy, optional detail bind, rollback and
  ordinary/same-generation authored checkpoint publication.
- `scene_l1s2_detail_publish`: reset before its standalone detail overlay bind.
- `scene_terrain_legacy_edit`: reset before copying staged descriptors into the
  existing live overlay after the checked DEV edit commits.
- `scene_campaign_geomod`: reset before its scripted-room bind.
- `scene_campaign_wall_refresh_script_room`: reset before copying that room
  into both retained wall overlays.
- Campaign wall publication: reset before exposing the replacement world.
- `scene_world_mission_assign`: reset on successful whole-world assignment,
  even if a restore reuses the same addresses or generation.

World identity changes also lazily reset the batch; its existing implementation
checks room/list pointer identities and counts. These safeguards supplement,
not replace, the explicit mutation resets. Initial construction happens before
first admission. The immutable authored world has no runtime node/face/room
mutation outside these scene publication owners; moving solids use separate
live poses and are never cached. A future static geometry writer must reset
before publishing, just like the listed owners. No topology hash or unreliable
pointer-only inference substitutes for that contract.

The candidate has no saved state and does not cache collision outcomes. Source
checkpoint and packaging are deferred until the parent completes the active
XEMU timing window.
