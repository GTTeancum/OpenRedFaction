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
dropped items, multiplayer respawn, corpse poses, destructible props and GeoMod
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


## Rendered revisit fixture

`tools/replay_pickup_return.py` runs240 PC input frames: collect L4S5 rifle3415,
select/fire it, dispatch exit861 to L4S4 at60, dispatch592 back at180, then walk
into the original pickup location. Final selected rifle has39 rounds and zero
reserve; the final scene reports zero new grants and the persistent key remains
`l4s5.rfl / 3415`. The frame boundaries are61 and181. The fixture explicitly
stages arrival for repeatable contact; it does not bypass the pickup grant logic.

The XEMU harness accepts `--return-exit-uid` with the existing exit/item fixture.
It sends no host input and compares every taken level/UID key directly from guest
RAM with the PC record, in addition to the existing inventory and scene checks.


Native verification: `artifacts/xemu/replay-20260914-053954/report.json` passes
240 input frames with two exits at61/181, three scene loads and the exact retained
key `l4s5.rfl / 3415`. Rifle state remains39 loaded/0 reserve and final pickup
grants remain zero. The PC final body is1.18 units from the item (within the2-unit
collection radius), and native state matches. Native framebuffer visually checked;
7231 free pages (28.25MiB) remain while rendering the returned scene. Both builds
and all36 CTests pass. This verifies pickup lifetime through controlled scene
transitions; that earlier run predates actor retirement.


## Defeated actors

A separate owned actor-key store now records defeated NPCs before successful
section teardown. Its 2,048 records and 128 level names occupy 32,776 bytes, and the
shared C inventory verifies all 1,610 authored actor keys across the campaign fit.
Pickup and actor namespaces stay independent even when their level/UID pairs
match. Both stores share the bounded registration implementation; the pickup
layout remains 20,488 bytes.

Actor keys register before startup events. At a successful level handoff, owners
with health<=0 mark their preallocated key, including owners already unregistered
by death cleanup. On revisit, the normal actor registry entry is removed, the
physics body is closed and render dispatch skips that explicitly retired actor.
Registry-backed combat/AI/weapon paths consequently cannot treat it as living.
This first pass removes defeated actors on return; it does not reconstruct their
corpse pose, move animation time forward or replay death side effects. Partial
health, positions, dropped items and resurrection scripts remain separate work.

The contained scene test covers alive/dead capture, capture after unregister,
replacement owner restoration, missing registry lookup, sphere allocation release,
repeat capture and a fresh campaign. The rendered L4S5 rifle round trip also
exercises this path naturally: its three shots defeat NPC 3305; the return restores
one defeated actor while retaining 39 rifle rounds and retiring pickup 3415. This
caught and fixed a renderer assumption that every retained model had a live actor
registration. The error remains for unexpected missing registrations; only
explicit campaign retirement is skipped.

Native actor-retirement verification passes in
`artifacts/xemu/replay-20260914-060602/report.json`: 240 frames, both handoffs,
retirement `[8,1,0,0]`, defeated key `l4s5.rfl / 3305`, and pickup/inventory
state exactly match PC. The returned scene has 7 active collision/eye actors
from 8 created owners; the harness now distinguishes live counts from startup
counts while retaining exact PC comparisons. Native framebuffer inspected;
7,223 free pages (28.21 MiB) remain. Both builds and all 36 CTests pass.
