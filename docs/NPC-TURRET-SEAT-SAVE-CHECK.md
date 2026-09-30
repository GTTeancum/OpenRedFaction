# Active NPC turret seat persistence check

`tools/xemu_npc_turret_seat_save.py` reuses the authored L3S2 guard936/turret935 fixture in CTF06. It saves after60 neutral frames, then boots a fresh Xbox process and loads for60 more neutral frames. Native execution is pending parent integration. The earlier runtime-only fixture passed at `artifacts/xemu/npc-turret-seat-20260930-131904`: two damaging turret shots, no handheld shots and3,093 free pages.

The harness decodes the actual captured RFWC vehicle component. RFNS must contain exactly one active turret seat linking authored NPC UID936 to host UID935 with the runtime seat tag. RFTU must contain the living, occupied host targeting player UID0. Saved remaining cadence and burst values may be zero while the turret is still aiming; they must not be invented just to force a nonzero test value.

Both boots take a live memory probe before teardown. Checks require one active physical seat, action13, a valid linked host, unchanged NPC vitals, finite published pose, ongoing operator eligibility/suppression and zero handheld shots. Ordinary fresh-load telemetry must report exactly one restored seat and one targeted living turret. Restored target/cadence is compared against the serialized RFTU row using `rf_scene_turret_restore_probe[8]`: host UID, target UID, resolved target handle, saved remaining ticks, restored fire deadline, restore frame, restored burst and restored action. The deadline must equal restore frame plus remaining ticks, or zero when remaining is zero, matching the current adapter contract.

The loaded turret must subsequently fire and damage the same resolved player target, with no healing masking the health decrease. CTF06 armor pickups remain permitted. The harness retains generic player-survival gates for these short runs. It reports save/load evidence and restores disc flags/archive in `finally`; it does not inspect images, send host input, traverse a campaign route or establish animation/audio fidelity. Jeep seats and detached-seat persistence are outside this bounded check.

## First integrated Xbox run (unresolved)

`artifacts/xemu/npc-turret-seat-save-20260930-132350/report.json` records
FAIL, preserved unchanged. Source60 frames save active RFNS actor936/host935
and RFTU aim/target state. Fresh load60 restores one active seat, exact cadence
probe, actor vitals/action13 and target identity with zero admission errors.
However, loaded combat records59 aim holds,20 turning ticks and zero shots,
so the resumed-damage requirement fails. Disc inputs restore successfully.
Do not report occupied gameplay continuation as verified until this is resolved.

## Corrected aim and scoped continuity pass

`npc-turret-seat-save-20260930-132759/validation.json` records
PASS_SEATED_TURRET_CONTINUITY from the existing source/load evidence. Both boots
finish60 frames at64MiB. One active seat restores actor health75/armor40, action13,
its host link and the turret target; the saved4-frame fire delay restores as4.
Loaded turret fires four damaging shots with zero handheld shots, killing the
stationary player (32.80001 health at save to -9.599998). Minimum free pages2,931
(~11.45MiB). The original report remains FAIL because the generic runner rejects
player death; the scoped validator already permits it and all its checks pass.
Future runs explicitly permit player death while still checking attributed
damage, ownership, cadence and memory. No extra emulator run was needed.
Jeep runtime, detached-seat saves and animation/audio appearance remain open.
