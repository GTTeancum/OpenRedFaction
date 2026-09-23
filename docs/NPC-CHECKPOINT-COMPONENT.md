# Ordinary saves: NPC component preparation

Current status (2026-09-23): RFNC4 adds a 548-byte base row with the mover
controller's authored UID at offset 544; zero means no backlink. RFNC1/2/3
remain readable. The scene adapter rejects unknown backlinks and restores the
saved UID to a fresh controller handle before publication. This admits L17S1
UID 20026: PC save/load and stock 64 MiB Xbox save/load pass. The sections
below describe the earlier component-staging work and historical limitations.


Earlier RFNC3: base rows544 bytes (RFNC2 eye angles plus optional trailer length at540). Optional animation trailer is108+12*active_slot_count bytes; maximum row844. It preserves script ownership and generic playback/controller state, including ambient mapped loops without script ownership. RFNC1/2 remain readable. Scene rejects unsupported non-script action clips and unsaved gameplay owners. See ORDINARY-SAVE-INTEGRATION.md for current evidence and blockers.

The first current admission blocker is `scene_checkpoint_player_scope_mode` in `scene_player_checkpoint.inc`: it requires DEV, rejects all NPCs and movers, then restricts the level to ctf06.rfl or glass_house.rfl. RFCP5 contains no NPC payload. Removing that gate would reset NPC progress, not implement ordinary saves. Static-clutter identity additionally rejects moved/nonordinary props. These restrictions remain intact.

Existing `rf_campaign_actors` stores per-level UID retirement, vitals, allegiance/hidden/invulnerable bits and persistent weapon drops for in-memory revisits. It does not retain position or inventory. RFNC1 adds a standalone component containing those retained fields plus current position/yaw, complete NPC inventory, selected primary/secondary, class ordinal and basic AI mode. No claim of a complete ordinary save or RFCP integration is made.

The64-byte little-endian header contains RFNC/version/bytes/FNV checksum/count/reserved, source identity32, weapon catalog hash and reserved. Checksum treats its own four bytes as zero. Each528-byte row stores UID0, class4, retirement8, mission flags12, affiliation16, health20, armor24, position28, yaw40, primary44, secondary48, drop state52/weapon56/quantity60/position64, ownership bytes76, reserve140, loaded268 and AI mode524. Rows are strictly UID-sorted. Caller identity must bind authored level/UID/classes and immutable definitions; catalog hash binds weapon ordinals and supported policy.

The codec allocates nothing, stages one row on the stack during validation, and validates all rows before copying decoded output. Caller capacity is the actual byte limit; RF_CAMPAIGN_ACTOR_SLOTS is only the structural maximum. Encode only resident/captured rows, not the entire campaign actor store. The composed transport's110524-byte cap remains unchanged and must reserve the exact encoded bytes before terrain serialization. No max-count stack array is needed.

Dormant loaded rounds without ownership are retained because authored `None` removes category ownership while leaving ammunition. Selected weapons must be owned. Ammo must be nonnegative and loaded rounds cannot exceed the catalog magazine; historical reserve surplus is preserved. Retired actors and empty available/collected drops remain distinguishable. Class health/armor limits, valid affiliation, UID/class matching, world fit/support and actual resource availability belong to live preflight.

This first component explicitly supports only basic AI modes-1/0/1/2/11. It does not contain active waypoint/Goto route cursors, explicit combat targets, awareness/reaction/reload/burst timing, damage/pain/death animation progress, projectiles, burning/shield state, attachments or carried objects. A scene adapter must reject those states until represented; it must not reset them silently. Physics must be settled upright and class/pose reconstruction must be validated before publication. Dead actors must have finished their death transition; restore retirement and persistent drop independently so reloading cannot respawn actors or duplicate pickups. A successful decode alone does not establish playable save fidelity.

Parent integration: add `src/core/npc_checkpoint.c` to PC/Xbox core sources and a focused `npc_checkpoint_tests` target linked to rf_core. Future RFCP framing and live actor staging must remain opt-in. Movers, event/trigger state, collected authored items, unsupported player systems and moving props are separate ordinary-save blockers. Tests were authored without running builds or gameplay in this task.

## Scene capture preparation

`scene_npc_checkpoint_capture(catalog, now, rows, capacity, count)` in the dedicated capture include reads actual registered owners and their campaign persistence keys. Include it near the checkpoint adapters after NPC, seed, pose and playback globals. Caller supplies row storage sized to the resident count and current wrapped simulation milliseconds. The function allocates nothing: a first pass validates identities, duplicate UIDs, individual RFNC records and all admission checks; a second assignment pass copies and sorts. Scene ownership must remain stable across both passes. Output rows/count remain unchanged on error.

Capture includes retired owners that have already unregistered; their retained `published` position replaces the freed physics body position. A missing registration for a nonretired persistent actor is an error. Nonexistent, never-registered placeholders contribute no row. Registered retired actors with an active death clip or deadline are rejected until settled. Basic standing-loop animation may restart its cosmetic phase; any other active clip, frozen/scripted animation, bound route, navigation, combat state, pending pain/unholster/death timer, burn or linked owner rejects. Motion commands, eye adjustments and body velocity must be settled. Complete active-route/combat/attachment save support remains separate.

The composed caller must still gate global projectiles, shield durability, dynamic support, all carried-object ownership and world systems outside this adapter. Capture is not restore: class/resource/body-placement checks, retired publication and transition-safe initialization remain mandatory. The dedicated actual-scene test exercises UID ordering, vitals/ammo/drop transfer, unsettled rejection, capacity/error preservation and safe capture after retired body release. No standalone test target was built by the helper task.
