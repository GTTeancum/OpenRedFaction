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
