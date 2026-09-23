# Ordinary-level saves: implementation workstream

## Current milestone: L17 native save/reload (2026-09-23)

RFNC4 encodes NPC mover-controller backlinks as authored first-key UIDs and
rebinds them to live handles before publication. This admits L17S1's UID 20026
without preserving a stale runtime handle. PC L17S1 live save/load completes,
and the final countdown bits match a continuous run. On stock 64 MiB XEMU,
the L17 save's 16 components exactly match PC and a separate native load
reaches 180 frames with 4522 pages available (about 17.7 MiB). Its native
framebuffer was inspected. The native post-load countdown bits, live native
quick-load, other levels and unsaved dynamic systems remain open.


## Current milestone: fresh ordinary PC reload (2026-09-22)

`python tools/check_ordinary_save_reload.py` now captures120 neutral frames, launches a fresh process to load and immediately resave, then launches another fresh process to load and move for30 frames followed by89 settling frames. All16 component payloads match byte-for-byte in the immediate resave. The continued run moves from[-119.1875763,0.3200837,59.4398079] to[-120.8788605,0.3425023,57.2130127] and saves successfully. Finite coordinates and squared movement distance are checked from the RFPL position at byte32. PC renderer output was inspected: mine geometry, lamps, crosshair and health/armor HUD render after continuation. No GitHub image was added.

`RF_REPLAY_WORLD_SNAPSHOT_IN` names the protected two-slot base path. The PC loader runs after frame-zero initialization, before frame-one input. It verifies current source identity, loads saved NPC motion resources, stages world/player/mission/history/environment/events, validates all targets, then publishes through assignment-only functions. The101708-byte save uses1136784 bytes of staged owners on PC, plus the input file buffer and bounded temporary motion-demand storage. Fresh NPC owner validation permits initialization clips while retaining rejection of unsaved routes/combat and checking full prepublication owner/pose snapshots. Normal save admission remains unchanged.

This is a working limited ordinary PC profile, not campaign-wide save completion. Current test starts unarmed with100 health/armor, so firing and finite-ammo continuation are not verified. Player weapon resources must already be available from ordinary scene demand; missing resources reject. Active remote charges, vehicles, destruction, queued level transitions and uncaptured AI/shield history remain outside this profile. Startup presentation can replay; audio/subtitle/particle continuity is deferred. Native storage dispatch remains unwired for this new profile, although the shared Xbox build passes. XEMU reload and stock64MiB runtime headroom remain next.

Validation: focused NPC capture/restore checks pass after the fresh-boot change; PC replay capture/load/resave/movement passes. Earlier RFWC2, environment, mission and carry checks remain applicable. Overall83%, Saves74%.



## Current milestone: ordinary restore staging (2026-09-22, solo integration)

The snapshot uses RFWC2 with a sixteenth ENVIRONMENT component. RFCH3 retains player import/export, Machine Pistol/Undercover carry records and campaign countdown state; RFEN1 retains gravity and UID-bound force active/strength state. The historical unmodified L1S1 capture was101708 bytes, within110524, and reported history omission mask0. Legacy RFWC1 decoding remains supported without an environment component; it cannot satisfy a required environment section.

The120-frame process-local replay now stages all78 NPCs (including actual bone pose evaluation), saved doors and static props,184 event records,61 trigger states,22 pickup bindings, mission goals, campaign histories/carry and the standing player with weapon modes. Gravity stages successfully; this level has zero force regions, so a focused actual-scene test covers nonempty force restoration, UID reorder, geometry mismatch, stale-owner validation and handoff rejection. No gameplay publication occurs during these probes.

NPC placement now preserves legitimate stationary authored placements: nonfalling actors may have no floor contact, represented as support handle0/material-1. Exact authored poses in proven unchanged static terrain may retain authored surface overlap and conservative sphere overlap with unchanged authored props. Changed poses, new/moved props, moving doors, solid/ambiguous centers and unproven geometry retain strict checks. Player placement remains strictly grounded and checks candidate NPC overlap.

NPC, mover and event commit helpers now expose separate validation and assignment phases, alongside existing prop and new mission/environment stages. This supports validating all components before any live assignment. Saved NPC motion-demand support is implemented and compiled, but not yet wired into a fresh-process loader. Separate probes succeeding is not proof of atomic composed publication.

PC and NXDK Xbox XBE/ISO builds pass. Focused component/placement/history/mission/environment/envelope checks pass. No new XEMU runtime or stock64MiB headroom measurement is claimed. Event flags385 after environment staging mean deferred message/particle presentation plus the player/mission composition dependency; independent player/mission probes now pass, but the complete loader still must bind them together. Queued level transitions are rejected. AI/shield histories remain separately unsupported when present.

Next: load the persisted file in a fresh process, demand saved resources before restore, connect one validated world publication with correct startup order, and verify walk/fire continuation; then stock64MiB Xbox. Restore of remote charges, vehicles and active destruction in the ordinary profile also requires composition. Overall82%, Saves70%. Helpers are paused by user instruction; continue solo.

Earlier milestones follow chronologically in their own sections.


## Current milestone: ordinary snapshot capture (2026-09-22)

`python tools/check_ordinary_save_readiness.py --snapshot --restore-probe` runs120 neutral process-local frames on unmodified Installed_Game/levels1.vpp:L1S1.rfl. It writes100676 bytes in a protected RFSG slot containing an RFWC envelope, under the unchanged110524-byte transport cap. The harness independently verifies persisted transport/envelope checksums and component directory lengths. No host input or original executable is used.

All78 NPCs now capture, including the two scripted animations and ambient fish controllers. RFNC3 preserves playback phase, active slots, weights, completion/freeze/events and controller transition state. NPC payload is51868 bytes. RFCH1 merges current Switch/event and actor histories into private copies, preserving prior-level state without mutating live catalogs. History rebinding uses level/UID, including case-insensitive canonical level keys.

This is a diagnostic capture bundle, **not a loadable game save**. Current history omission mask3 reports player campaign carry and weapon-mode campaign sidecars. AI/shield history also remains unrepresented when present. Current source identity binds the current RFL, tables and weapon catalog, not every asset or prior level.

Candidate restore now stages saved mover geometry and static prop state before NPC placement. The real probe stops at miner UID8322 (three spheres, authored position[-78.6500778,-4.55436325,48.7840118]) with RF_NOT_FOUND; no gameplay publication occurs. Real scripted bone evaluation has consequently not been reached/proven. Player/events/history/resource admission and atomic whole-world publication remain necessary, followed by fresh-process reload/continue.

Validation: shared PC executable and NXDK Xbox XBE/ISO build; focused NPC codec, actual capture, staged restore and campaign-history tests pass. Synthetic animation checks include script loop/one-shot/frozen and ambient state18 continuation. No new native run or64MiB runtime headroom claim for this snapshot path. Saves approximately67%; overall82%.

The sections below record earlier implementation milestones.

The current live checkpoint path supports bounded developer-room player/destruction profiles. It does not support an ordinary L1S1 save: both frontends require DEV player-checkpoint mode, the scene requires a terrain checkpoint owner, and player scope rejects NPCs and movers. Those gates remain intact until ordinary state can be reconstructed without resetting gameplay.

Implemented components toward that work:

- `npc_checkpoint.h`: RFNC1 retains settled NPC pose, health/armor, mission flags, full finite inventory, selected weapons, basic AI mode, retirement and weapon drops. Source and weapon-catalog identities bind class and weapon indices. Active routes, combat/reload/pain/death transitions, projectiles and attachments still need capture/restore or explicit admission handling.
- `campaign_goals_checkpoint.h`: RFGC1 preserves active mission counters and section-local goal history, including persistent flags and signed values. Scene declarations and source identity must match before publishing restored state.
- Both formats use explicit little-endian fields, checksum and complete validation before modifying outputs. They allocate no storage. Actual row counts consume caller capacity; the existing total checkpoint transport budget is unchanged.

Focused PC checks pass for both components. They are not yet connected to a new ordinary-world file profile. Player state, weapon modes, remote charges, props and vehicle component formats already exist, but their presence alone does not establish full composition.

Next integration: capture registered NPCs and restore them against authored identity/resources; retain mover/controller state; preserve event/trigger timers and one-shot history; include pickups and mission state; provide ordinary static-world source identity and placement checks independently of DEV terrain. Restore world/collision and actors before player placement, rebinding handles from stable UIDs. Keep the current profile unchanged for existing saves.

Acceptance is a fresh-process ordinary L1S1 sequence: walk/fire, settle, save, reload, then walk/fire again with unchanged health, ammunition, NPC state, pickups, doors and mission progress. Follow with a focused stock64MiB Xbox run and memory check. No original-game screenshot comparison is needed.

Build verification: shared PC executable and NXDK Xbox image build successfully with these changes. New component tests passed on PC; native save/load and moved-prop runtime behavior remain unverified.

## Additional state components and scene capture

The pickup-store codec preserves level/UID ownership so a collected item remains absent after reload/re-registration while the same UID in another level stays independent. The trigger-store codec retains the existing trigger flags/counts and remaining cooldown/contact timers; focused checks use actual runtime trigger save/restore.

Mover records now retain translation/rotation progress, key cursors, pose scalars and remaining dwell time. The rotating-door tick now uses the common wrap-aware pending check; a focused gameplay check catches premature movement before a wrapped deadline and verifies resumption at that deadline.

RFEC2 now admits settled common event state, Switch state, UnHide cooldown, monitors and cycles, with explicit UID reference remapping. Its supported-type inventory covers160 of184 L1S1 event records; that inventory does not establish scene-save admission. Pending work, unsupported types and uncaptured external effects still reject.

`rf_scene_npc_checkpoint_export` now captures actual registered NPC owners into RFNC1, with bounded temporary storage and error-atomic caller output. Its actual-scene check verifies sorted identity, health, ammunition and retired actors whose physics bodies are already released. Unsupported active routes/combat/animation/attachments still reject. Exporting this component does not create a usable whole-level save.

Focused PC checks for these components and the rotation fix pass. Full ordinary-world composition, physical mover restoration, NPC restore publication and broader event coverage remain next. Estimated save-system implementation is approximately55%; this is not a live ordinary-save acceptance claim.

The shared PC executable and NXDK Xbox image build with the capture/export and state components. Xbox compilation caught a signed UID comparison, corrected to the same unsigned authored UID representation used by the codec. No ordinary-level save/load run is claimed yet.

## Restore integration progress

The scene now includes staged NPC, mover and event restore helpers, but the ordinary loader does not call them yet. NPC preparation checks candidate world placement/support, restores bounds, material, room, inventory, vitals, basic AI and resident idle playback, and rejects overlaps between restored NPC sphere sets. Publication rechecks original owners before changing actors; retired bodies and registry entries are released after validation. Saved weapon/drop mesh demand must still be included in ordinary boot resource loading.

Mover preparation reconstructs door poses and collision views without changing the live world. Commit preserves existing allocation/registry bindings and rejects changed controller identities or membership. Event staging resolves UID references against fresh registry owners, retains Switch state and applies event retirement; its mandatory admission callback must account for each event's external effects. The composer must check cross-component references before retiring actors/events and perform all preflight before any component publication.

The ordinary world callback uses real static collision, ground sweep, material and room queries plus candidate mover and prop geometry. Prop sphere clearance is conservative. Candidate vehicles and detached destruction pieces remain separate composition checks. Player snapshot now shares existing inventory/vitals/pose capture while separating player-state restrictions from the legacy DEV-world gate; the legacy gate remains unchanged.

Focused PC checks cover actual NPC restore publication, player inventory/vitals roundtrip without DEV terrain, mover geometry publication, reordered event references and registry retirement, static standing support/material/room and mover/prop obstruction, and candidate NPC overlap. These are component integration checks, not a successful whole-level save/load run. Overall remains approximately82%; saves approximately60% on the implementation basis.

Next: assemble ordinary source identity, retained UID map, state sections and boot resource demand into one bounded transaction; provide explicit admission for remaining event/world state; connect PC/Xbox storage dispatch and run ordinary fresh-process acceptance. Do not enable the ordinary profile by bypassing missing state.

The shared PC executable and NXDK Xbox XBE/ISO build successfully with these restore helpers. No new XEMU run or whole ordinary-level save/load acceptance is claimed for this batch.

## Unmodified L1S1 readiness evidence

`tools/check_ordinary_save_readiness.py` runs120 neutral frames against read-only Installed_Game archives and produces a component report without writing a save. The original run exposed a real startup blocker: four NPC weapon textures require524288 image bytes plus material/scratch metadata, exceeding the old512KiB aggregate cap. A bounded1MiB cap now admits the actual526372-byte peak (525636 resident); it does not allocate the full ceiling. The inspected PC frame shows the authored mine entrance and startup message. Stock64MiB native headroom for this full ordinary scene remains unmeasured.

The current run completes and captures player544, mover504, prop4096 (168 real owners), event35328 (184 records), trigger2568, pickup392, startup140 and goal64 bytes; UID mapping produces502 entries. Two authored Pole Light1 prop records have no installed class and therefore no runtime owner. Their absence is explicitly source-bound rather than replaced with fabricated assets; known-class missing owners still reject. NPC canonical level-name comparison now matches the campaign store's ASCII case folding. Unsupported unused weapon definitions no longer poison the entire NPC catalog; owning/loading one still fails validation.

RFNC2 adds exact eye angles, including tiny smoothing residuals, and reads legacyRFNC1. Restore derives the look matrix through the existing eye-physics function. Legitimate zero-sphere Cutter/fish bodies retain point bounds and use real room lookup with no-contact support; no collision sphere or floor contact is invented. NPC capture currently stops on active script animations UID9204 (motion3, nonlooping) and UID9615 (motion151, looping). Those states need persistence, not a bypass or a longer neutral replay to hide the looping case.

RFEC3 represents every L1S1 event type's existing local fields, including remaining common dispatch delay and pending UnHide requests. The represented fields do not persist active Play_Sound voices or Black_Out_Player timers; missing music, gaze and explosion behaviors are tracked separately in TO-DO.MD.

RFWC1 supplies a bounded15-section envelope (308-byte metadata) for player, NPC, mover, event, trigger, goals, pickups, clutter, weapon modes, remote charges, vehicle, destruction, switches, startup and campaign history. It is structural framing only; source identity, component admission, resource preload and transactional scene publication remain mandatory. Source identity hashes the full level entry and validated tables archive plus catalog hash; external model/motion/texture/audio bytes are not yet included in a resource manifest. No complete ordinary file has been written or restored.

Focused PC envelope, event scheduling, NPC codec/capture/restore, point/sphere placement and sparse prop checks pass. Shared PC and NXDK XBE/ISO builds pass. No new native runtime save/load is claimed. Overall remains approximately82%; saves approximately64% for implemented late-alpha functionality. Next work is scripted NPC animation persistence, followed by actual whole-file capture/restore and boot resource composition.


## Ordinary HDD profile (2026-09-22)

The shared ordinary loader and capture path now compile for NXDK. Optical harness flags world-hdd-load.flag/world-hdd-save.flag opt into a separate R:\OpenRedFaction\ordinary two-slot profile; DEV slots retain their existing name. The native adapter flushes the selected new slot and releases its owned mount. An endpoint payload buffer (at most110524 bytes) remains available for process-local QMP inspection until the next capture.

A stock64MiB XEMU run of unmodified L1S1 completed120 frames and wrote101708 bytes at generation1. All16 component payloads match the corresponding PC capture exactly. Minimum sampled free memory was16.78MiB, not a continuously measured allocation peak. The native framebuffer was inspected: mine geometry, lamps, crosshair, health/armor and the authored shift message were present. Evidence: artifacts/xemu/render-20260922-205015. Native reload is the next acceptance step; ordinary active destruction/vehicles and finite-ammo continuation remain outside this result.

Focused platform checks pass20 cases, including ordinary-profile routing followed by unchanged DEV routing. Both PC and NXDK builds pass. This is an opt-in harness path, not an end-user save menu or campaign-wide persistence claim.

Fresh native reload attempt render-20260922-205215 returned RF_NOT_FOUND. The render harness launches XEMU with -snapshot, so the preceding successful HDD write was discarded on exit. This is not proof of a loader defect or a successful native reload. Next: use an isolated persistent test disk for the two-launch write/read pair; retain the default temporary disk behavior for ordinary render checks. The failed launch was closed and disc flags restored by the harness. Saves estimate75%, overall83%.


## Fresh native reload (2026-09-22)

The render harness now offers --world-hdd-persistent for the ordinary profile. It creates/reuses exactly one standalone private disk at artifacts/ordinary-save-hdd/save-test.qcow2, with an ownership record and no QCOW2 backing chain. The base remains untouched; ordinary render runs retain temporary-disk mode. This reusable test disk is approximately2.84GiB and the whole artifacts/ordinary-save-hdd folder may be removed when persistence evidence is no longer needed and XEMU is closed. Reports and captured save payloads are in separate render folders.

Write run render-20260922-205609 stored generation1/slot0. A fresh XEMU process in render-20260922-205753 loaded101708 bytes, staged1136784 bytes and resaved generation2/slot1. All16 sections match both the original native save and the fresh PC reference byte-for-byte. Native load/save statuses are zero, both attempted flags are set, and the retained payload hash is ba4f481af21ef8bfa0e5d49ff4b7f96931aaf744044faf5fb1a105caa9e5fead. The reload endpoint had16.96MiB sampled free memory; this does not measure every intermediate allocation. The framebuffer shows intact mine geometry, lamps, crosshair and health/armor. Startup message presentation is not restored by this first ordinary profile.


Movement continuation run render-20260922-205939 loaded the generation2 save in another fresh XEMU process, played120 process-local frames (one neutral,30 forward,89 neutral), and stored generation3/slot0. The player moved from (-119.1875763,0.3200837,59.4398079) to (-120.8788605,0.3425023,57.2130127). All16 resaved sections match the PC continuation; native payload101696 bytes. Minimum sampled free memory16.39MiB. The inspected native image shows the changed camera position with intact world/HUD; movement-result.json records the finite position check. This closes ordinary initial-level native load/resave/movement acceptance, not finite-ammo firing, active destruction/vehicles, broader resource admission or the end-user save/load menu. Overall83%; saves77% on the requested late-alpha implementation basis.


## Saved weapon demand and firing (2026-09-22)

Ordinary boot now reads the protected save and validates its source identity/player catalog before first-person resource planning. Saved ownership contributes a sparse resource mask through the existing expansion/loading path, without granting inventory or publishing state early. Gameplay still goes through the complete world transaction after initialization. Catalog mode ownership resolves weapon names directly rather than depending on IDs assigned later during inventory initialization.

The enemy-free CTF06 railgun grant fixture now captures an ordinary17092-byte save, starts a fresh PC process without redispatching the grant, restores slot7 and fires exactly one shot. Loaded ammunition changes1 to0. The resulting ordinary save is written successfully. tools/check_ordinary_weapon_reload.py records the reproducible case in artifacts/ordinary-weapon-save/report.json. The final framebuffer was inspected: railgun/hands, world geometry, water, crosshair and health/armor HUD are present. This fixture includes authored weapon demand as well, so it does not alone prove a saved-only optional resource absent from all pickups/events. Native armed continuation remains unverified.

This broader level exposed unsupported authored pickup classes: startup deliberately creates no gameplay owner/persistence entry for them, whereas mission restore previously demanded an entry for every authored item. Restore now preserves only that unchanged, untaken/unbound absence; missing supported pickup records still reject. Focused actual-scene checks cover both paths, alongside reordered UID binding and stale-owner rejection. Overall83%; saves78% on the working implementation basis.


## Armed native continuation (2026-09-22)

Ordinary saving now selects the previous generation by structural envelope validation, allowing a new level's save to replace a valid older level save while preserving the previous slot. The new write and all load paths still validate the expected source identity. This fixes an ordinary cross-level replacement failure; it does not implement automatic level selection on load.

Native write run render-20260922-211101 used the enemy-free authored CTF06 railgun grant fixture,180 input frames, and the same private persistent HDD from the L1S1 tests. It wrote17092 bytes as generation4/slot1 and all16 components match PC exactly. Minimum sampled free memory14.53MiB. This also proves the writer can retain generation continuity across level identities. The fresh armed load/fire run is the remaining check for this batch.


Fresh native continuation render-20260922-211519 loaded17092 bytes, staged763744 bytes, fired once and saved generation5/slot0. Loaded railgun ammunition1 became0; native COMBAT shot count1 and PLAYER_AMMO match PC. All16 saved components also match PC. Minimum sampled free memory13.90MiB. The native framebuffer was inspected and shows the equipped railgun/hands, test room/water, crosshair and health/armor HUD. artifacts/ordinary-weapon-save/native-result.json links the two runs and records these assertions. Ordinary armed load/fire/resave is now verified for this bounded fixture. No new screenshots were uploaded. Broader saved-only resource demand, active destruction/vehicles and the user-facing menu remain. Overall83%; saves79%.


## In-session quick-save (2026-09-22)

The public rf_scene_save_button edge latch now queues an ordinary save at the end of a complete simulation tick. F5 maps it on PC; Back+Y maps it on Xbox/XInput and suppresses other controller actions while held. A dedicated HUD message reports success, unavailable state or storage failure without overwriting story subtitles. Failures do not propagate out of the scene loop. The normal endpoint snapshot flags remain separate; PC live requests use redfaction-save.0/.1 in the working directory, while Xbox uses its ordinary HDD profile.

The process-local PC harness tools/check_live_quicksave.py requests at frame150 without an endpoint output flag, sees GAME SAVED in the inspected frame, loads that file in a fresh process and fires the restored railgun's final round. artifacts/live-quicksave holds logs/images/report. A busy-action request at frame120 returns RF_RANGE, continues to the endpoint and leaves both prior save files byte-identical. Compiled NXDK input checks pass13 cases, including neutral gameplay during the chord, no repeated request while held and rearming after release. The test's SDL axis doubles now also return neutral trigger axes instead of indexing beyond the four-stick fixture.

PC and NXDK builds pass. No host input was sent. Physical controller use, native mid-session save and user-facing quick-load/menu are not verified or complete. Overall83%; saves80%.


## Same-session quick-load (2026-09-22)

F9 on PC and Back+X on Xbox/XInput request a same-level load. Before leaving the current scene, the request reads the protected slot and verifies its identity against the current source. Missing/incompatible saves produce NO COMPATIBLE SAVE and leave play running. Accepted requests stop at the next frame boundary and use the normal frontend teardown/rebuild path, with a separate load marker: no departing player import, no doorway placement and no outgoing mission-history carry. The current archive remains open and is reparsed, preserving fixture identity and resetting authored spawn placement. After rebuild, saved inventory drives resource demand, all restore stages validate, and the ordinary transaction publishes before subsequent input. GAME LOADED then appears in the HUD. This is not a title/pause menu or cross-level save chooser. Full semantic admission still occurs after rebuilding; a corrupt or unsupported semantic/resource state that passes the preliminary envelope check can still fail there.

PC process-local sequence: save150, fire200, request load260, rebuild at261, fire330, end400. It restores the saved final railgun round and consumes it again. A missing-save variant keeps the original scene and completes without a transition. tools/check_live_quickload.py reproduces the cases. Compiled Xbox controller checks now pass17 cases including both chord edge latches and gameplay suppression. No host input is generated.

Stock64MiB XEMU render-20260922-213013 executes that same sequence through campaign-quick-actions.bin, a replay-only provider hook. It transitions once at261, loads17092 bytes with763744 staged bytes, and writes generation7/slot0 at completion. All16 final components, ammo and combat counters match PC. Minimum sampled free memory13.80MiB; teardown reports11517 free pages. The inspected native frame shows GAME LOADED, equipped railgun/hands, intact room/water and HUD. The saved round is spent again, leaving loaded ammo0 and one shot in the rebuilt scene. Both builds pass; no new GitHub images. Overall84%; saves83%.


## Cross-level quick-load resolution (2026-09-22)

The request now selects the newest structurally valid protected save, decodes its level, tries the current open archive, then searches levels1/2/3 beside the installed tables archive. The saved target's full level/table/catalog identity is checked before setting a transition or releasing current owners. Frontends use the same archive search after teardown while retaining same-archive fixture behavior. Departing inventory/history carry remains disabled for quick-load. Missing source or mismatched identity returns a normal load notice rather than discarding the active scene.

PC evidence artifacts/cross-level-quickload: start unmodified CTF06 from levelsm, request load60 using the prior unmodified L1S1 snapshot, transition to L1S1 at61, restore101708 bytes with78 NPCs/184 events, and complete180 total frames. The inspected final image shows the mine entrance, HUD and GAME LOADED. Same-session same-level save/fire/load/fire and missing-save continuation still pass tools/check_live_quickload.py. PC/NXDK builds pass; a new native cross-level runtime run is not claimed. Explicit menus/slot choice, cross-install asset manifests and broader gameplay-state persistence remain. Overall84%; saves84%.
