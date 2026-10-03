# NPC Attack against a generated Auto Turret head

Integrated and verified with a bounded stock-64-MiB Xbox save/fresh-load check.

## Identity and admission

RFNC11 adds combat.active3: an ordinary NPC Attack whose target is a generated AutoHead, identified by its authored base UID and implicit head role1. Runtime handles and invented child UIDs are never serialized. Active2 keeps the authored UID/player namespace. The writer emits version10 unless an active3 row requires11; older formats remain readable and reject the new discriminator.

The 600-byte base row and 40-byte combat extension keep their existing layout. Capture resolves the head to its base UID. Fresh load resolves the newly created dependent head and restores the fire/reload deadlines, burst and spread RNG. Composed world admission cross-checks the RFTU2/RFTU3 companion and saved RFNC base before publishing. Dead, hidden, removed, retired or mismatched generated targets reject. The standalone RFNC component is not a composed save.

Implementation: `scene_turret_generated_attack_checkpoint.inc`, shared RFNC codec, NPC capture/restore adapters and ordinary world admission. Immutable restore telemetry records the assigned identity/deadlines before simulation.

## Focused Xbox check

Run `python tools/xemu_auto_turret_attack_save.py`. The harness copies authored L3S2 guard941 and Auto Turret base1994 into the isolated CTF06 testbed, changing their transforms. At frame1, its opt-in process-local fixture uses the existing Attack callback to target the real generated head, with first fire scheduled for106. Source capture occurs after60 presented frames.

The source uses the normal Set_AI_Mode backend to make the head Catatonic; that action persists in RFTU, keeping a live damageable target without killing the player before capture. Fresh-load observe mode does not reissue Attack, change AI mode, or reset deadlines. Normal NPC aiming, cover, collision and weapon damage must land the shot. No forced damage, fake target, desktop input, PC runtime, screenshots or campaign traversal is used. The harness restores original disc controls in finally.

## Xbox result (2026-10-03)

`artifacts/xemu/auto-turret-attack-save-20261003-115423/report.json` passes. The source save contains RFNC11 active3/base1994, a47-frame pending deadline, RNG1 and a live RFTU2 head. Fresh load restores those fields exactly, with zero Attack reissues. One ordinary pistol shot reduces head health from60 to40; base health remains80. Both runs complete60 frames with at least3063 free pages on stock64MiB. Original disc controls are restored.

Earlier frame60 source failures were valid dead-player rejections: current health was -9.6 while separate initial-vitals telemetry still showed100. Catatonic setup corrects the fixture; no save eligibility rule was relaxed. Temporary player-scope probes were removed. This establishes the focused gameplay continuation, not broader campaign, visual or audio fidelity.
