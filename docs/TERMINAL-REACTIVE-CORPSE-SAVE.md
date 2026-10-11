# Accepted terminal corpse reactive marker capture

Actual04:00 normal save failed at UID910 with reason FNV1a1084725576 (`combat_scripted`), value2 and RF_RANGE(-4). The mode2 live branch is entered whenever `!terminal_dead`; reaching its zero fallback with2 proves this actor passed `scene_corpse_checkpoint_death_capture` and set terminal_dead. No cause of death or killer is inferred.

The original L3S1 record identifies miner1 UID910,75 health,0 armor, handgun, raw AI1, friendliness2, empty path and no direct event links. This qualifies the actual owner, not a synthetic case.

The correction admits only `terminal_dead && combat_scripted==2` at this otherwise live-zero fallback. It still requires script_shoot.active=0; markers1/3/unknown and all living reactive target/owner checks remain strict. All earlier death/persistence/item/flight/route checks and later pain/playback/physics/velocity/support/deadline checks remain intact. It does not change health, current AI, live scheduling or source state.

Existing capture zero-initializes the row, sets combat_alert=0 and dead_pose=1 for this accepted corpse, and omits live combat continuation. Existing restore clears combat_scripted, stale target, burst and deadlines. No stale target is serialized, no combat restarts, no new format or save bytes are introduced. This follows the already-defined corpse representation rather than clearing live state to force acceptance.

Exact reviewed patch SHA256:53cfd15e0b2af3ef79ba0129c6e95779dff221599ba66fb5c3907185d4272f23. Implementation and independent source reviews completed after the actual04 failure. Integrated source is uncompiled and runtime-unverified. Later admission can still reject; successful save/load is not claimed. The completed04 batch will never repeat; the next hourly batch needs a new exact source pin and distinct evidence paths.
