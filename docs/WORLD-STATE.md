# Campaign world state

Pickup consumption now survives section handoffs in the shared scene runtime.
The owned key is the canonical level filename plus authored item UID, so identical
UIDs in different levels do not collide. The store contains no archive, scene or
renderer pointers. Its first-pass limit is128 levels and1024 items, occupying
20,488 bytes before the small per-scene slot array. All593 authored items across
68 installed campaign levels fit, including currently unsupported pickup classes.

Each supported item registers during scene loading, before inventory or health can
change. Taking a pickup sets its preallocated record only after a successful grant.
Full health/ammunition, blocked contact and dead-player attempts leave items in
place. On return, the scene restores its taken mask from those records; that mask
already controls both pickup rendering and collection. Section handoffs retain
the owner; fresh scene starts clear it alongside the mission-goal store. Levels
without an item section now initialize an empty list.

This is an explicit product-first persistence policy, not a reconstruction of the
original save-file format. Disk saves, dedicated restart/checkpoint policies,
dropped items, multiplayer respawn, killed NPCs, destructible props and GeoMod
state remain separate work.

Validation:

- `campaign_pickup_persistence` inventories68 campaign levels, registers593 items,
  replaces their authored list owners and verifies retained consumption. It also
  checks level isolation, case folding, capacity rollback and fresh-state reset.
- The scene's contained pickup test collects a handgun pickup through the actual
  contact/grant code, recreates the per-scene mask, restores it and confirms that
  the item cannot grant again. Existing obstruction, health/armor and ammo tests
  still pass.
- The L4S5 PC rifle replay still acquires42 rounds and fires to39. Both PC/NXDK
  builds pass;36 CTests passed before the final capacity reduction, and both
  affected tests pass with the final20,488-byte owner.

A complete rendered out-and-back pickup replay and native XEMU verification of
pickup retirement remain to complete. Mission-counter handoffs already have
separate native evidence in MISSION-GOALS.md.
