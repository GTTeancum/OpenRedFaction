# Ordinary-level saves: implementation workstream


## Current milestone: ordinary restore staging (2026-09-22, solo integration)

The snapshot now uses RFWC2 with a sixteenth ENVIRONMENT component. RFCH2 retains player import/export and Machine Pistol/Undercover carry records; RFEN1 retains gravity and UID-bound force active/strength state. Current unmodified L1S1 captures101708 bytes, within110524, and reports history omission mask0. Legacy RFWC1 decoding remains supported without an environment component; it cannot satisfy a required environment section.

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

RFEC3 now represents every L1S1 event type's existing local fields, including remaining common dispatch delay and pending UnHide requests. The represented fields do not implement missing sound/music, gaze, explosion or blackout behaviors. Those gameplay gaps are tracked separately in TO-DO.MD.

RFWC1 supplies a bounded15-section envelope (308-byte metadata) for player, NPC, mover, event, trigger, goals, pickups, clutter, weapon modes, remote charges, vehicle, destruction, switches, startup and campaign history. It is structural framing only; source identity, component admission, resource preload and transactional scene publication remain mandatory. Source identity hashes the full level entry and validated tables archive plus catalog hash; external model/motion/texture/audio bytes are not yet included in a resource manifest. No complete ordinary file has been written or restored.

Focused PC envelope, event scheduling, NPC codec/capture/restore, point/sphere placement and sparse prop checks pass. Shared PC and NXDK XBE/ISO builds pass. No new native runtime save/load is claimed. Overall remains approximately82%; saves approximately64% for implemented late-alpha functionality. Next work is scripted NPC animation persistence, followed by actual whole-file capture/restore and boot resource composition.
