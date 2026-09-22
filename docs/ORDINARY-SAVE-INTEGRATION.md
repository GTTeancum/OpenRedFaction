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

The event component currently admits only settled death monitors, cyclic timers and health/armor threshold latches. Other event types and unresolved source/actor handles remain unsupported. This is an explicit remaining integration gap.

`rf_scene_npc_checkpoint_export` now captures actual registered NPC owners into RFNC1, with bounded temporary storage and error-atomic caller output. Its actual-scene check verifies sorted identity, health, ammunition and retired actors whose physics bodies are already released. Unsupported active routes/combat/animation/attachments still reject. Exporting this component does not create a usable whole-level save.

Focused PC checks for these components and the rotation fix pass. Full ordinary-world composition, physical mover restoration, NPC restore publication and broader event coverage remain next. Estimated save-system implementation is approximately55%; this is not a live ordinary-save acceptance claim.

The shared PC executable and NXDK Xbox image build with the capture/export and state components. Xbox compilation caught a signed UID comparison, corrected to the same unsigned authored UID representation used by the codec. No ordinary-level save/load run is claimed yet.
