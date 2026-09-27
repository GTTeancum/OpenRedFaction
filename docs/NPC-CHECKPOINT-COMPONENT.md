# Ordinary saves: NPC component preparation

Current status (2026-09-27): RFNC8's optional 40-byte combat continuation
supports authored `Shoot_At` and active `Attack` orders. Shoot_At retains the
authored event and fixed point; Attack retains its target as an authored actor
UID (or zero for the local player). Both retain remaining fire/reload frames,
burst count and spread RNG. Restore rebinds the event or target to the current
scene, rejecting missing or defeated actor targets. RFNC1-7 remain readable. A live L15S1
PC order saves 22,460 bytes, reloads in a fresh process, fires 10 shots over
60 frames and saves again. The prior RFNC7 L1S1 wall snapshot also loads and
resaves as RFNC8 (105,056 bytes). A stock-64-MiB XEMU save matches all 16 PC
sections byte for byte, and a fresh XEMU load fires the same 10 shots in 60
frames (`artifacts/xemu/render-20260927-045939` and
`artifacts/xemu/render-20260927-050218`). No images were captured.

The natural L1S1-to-L1S2 route quick-saves in L1S2 with two active
NPC-to-NPC scripted Attack orders targeting UID9709. Its63,616-byte
PC save reloads in a fresh process with both orders still bound to that
actor; a100-frame continuation retains pursuit and the player's vitals.
Stock64MiB XEMU writes the same-size quick-save after the level handoff
(`artifacts/xemu/render-20260927-140200`) and loads it in a fresh process
(`artifacts/xemu/render-20260927-140922`) with all reported PC/Xbox state
checks passing and at least2931 pages free. The native harness confirms
save/load and aggregate state, but does not read individual Attack targets;
that binding was directly inspected on PC. Other scripted combat modes and
unbound/dead Attack targets remain unsupported.

Current status (2026-09-27): RFNC7 uses a 568-byte base row and an optional
168-byte movement/look extension for only the actors that need it. It retains
authored Goto, Goto_Player, Follow_Waypoints and Look_At bindings, route
cursor/start/goal, and steering vectors using event UIDs and route ordinals
instead of process-local pointers. RFNC1-6 remain readable. The real L1S1
Remote Charge wall now saves all 78 NPCs in 54,736 component bytes; its
104,744-byte PC world snapshot loads in a fresh process and resaves after
1, 10 and 60 neutral frames. Focused codec, capture and restore checks pass,
and the stock-64-MiB NXDK build completes. A text-only stock-64-MiB XEMU
wall save matches all 16 PC components byte for byte; a fresh XEMU load
continues for 60 frames. No images were captured.

Earlier RFNC6 (2026-09-27) added a settled frozen death-pose record at
offsets 552/556/560, including retained death flags and action. Completed
dead actors remain registered; capture preserves their final clip and weapon
drop while discarding stale movement, look and combat state. RFNC1-5 remain
readable. Focused codec/capture/restore tests passed, and the real L1S1 PC wall
replay admitted its settled dead guards; living actors with active scripted
movement/look still required RFNC7.

Earlier RFNC5 added an ordinary combat-alert bit at row
offset 548 (552-byte base row), while RFNC1-4 remain readable. The scene
captures a settled guard alerted to the player, then rebinds the player target
and schedules a fresh 30-frame attack delay on load; burst/reload deadlines,
scripted targets, active routes and one-shot combat/death clips still reject.
The three focused codec/capture/restore tests pass; `python
tools/check_ordinary_alert_component.py` confirms a live PC developer-room
alert exports a 736-byte NPC component, and the NXDK build passes. A 60-frame
stock-64-MiB XEMU guard fixture passes without images, with 1695 physical
pages free (`artifacts/xemu/render-20260927-034803/report.json`). Its
developer-room whole-save attempt still rejects, and no Xbox save/reload of
an alerted NPC has been claimed. The natural L1S1 wall save still rejects
UID8625's nonlooping frozen motion before other active actor states.

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

The earlier RFNC1 component supported only basic AI modes-1/0/1/2/11 and did not contain active waypoint/Goto route cursors, combat awareness/reaction/reload/burst timing, damage/pain/death animation progress, projectiles, burning/shield state, attachments or carried objects. Later versions add the narrow capabilities described above; unrepresented states still reject rather than silently reset. Physics must be settled upright and class/pose reconstruction validated before publication. Dead actors must finish their death transition; restore retirement and persistent drop independently so reloading cannot respawn actors or duplicate pickups. A successful decode alone does not establish playable save fidelity.

Parent integration: add `src/core/npc_checkpoint.c` to PC/Xbox core sources and a focused `npc_checkpoint_tests` target linked to rf_core. Future RFCP framing and live actor staging must remain opt-in. Movers, event/trigger state, collected authored items, unsupported player systems and moving props are separate ordinary-save blockers. Tests were authored without running builds or gameplay in this task.

## Scene capture preparation

`scene_npc_checkpoint_capture(catalog, now, rows, capacity, count)` in the dedicated capture include reads actual registered owners and their campaign persistence keys. Include it near the checkpoint adapters after NPC, seed, pose and playback globals. Caller supplies row storage sized to the resident count and current wrapped simulation milliseconds. The function allocates nothing: a first pass validates identities, duplicate UIDs, individual RFNC records and all admission checks; a second assignment pass copies and sorts. Scene ownership must remain stable across both passes. Output rows/count remain unchanged on error.

Capture includes retired owners that have already unregistered; their retained `published` position replaces the freed physics body position. A missing registration for a nonretired persistent actor is an error. Nonexistent, never-registered placeholders contribute no row. Registered retired actors with an active death clip or deadline are rejected until settled. Basic standing-loop animation may restart its cosmetic phase; any other active clip, frozen/scripted animation, bound route, navigation, combat state, pending pain/unholster/death timer, burn or linked owner rejects. Motion commands, eye adjustments and body velocity must be settled. Complete active-route/combat/attachment save support remains separate.

The composed caller must still gate global projectiles, shield durability, dynamic support, all carried-object ownership and world systems outside this adapter. Capture is not restore: class/resource/body-placement checks, retired publication and transition-safe initialization remain mandatory. The dedicated actual-scene test exercises UID ordering, vitals/ammo/drop transfer, unsettled rejection, capacity/error preservation and safe capture after retired body release. No standalone test target was built by the helper task.
