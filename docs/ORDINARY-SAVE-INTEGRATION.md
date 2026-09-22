# Ordinary-level saves: implementation workstream

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
