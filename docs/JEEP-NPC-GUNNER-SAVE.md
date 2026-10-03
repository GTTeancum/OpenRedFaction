# Jeep NPC driver/player gunner ordinary saves

2026-10-03: integrated; NXDK build passes and the bounded ordinary save/fresh-boot check passes. This extends the shared-seat control in `JEEP-NPC-GUNNER.md`.

The existing RFVC3 Jeep record already stores player occupancy, gunner role1, independent aim reference/angles, ammunition and full host pose/motion. The ordinary world RFPL1 record stores seated player position and preboarding exit look. RFNC10 plus RFNS already stores the living NPC driver's identity, action13 and model-tag pose. Missing pieces are explicit simultaneous ownership admission and restore publication without trying to take the NPC's driver slot through the generic player-driver path.

## RFNS2 wire contract

Header remains16 bytes: `RFNS`, version, inner byte count, row count. Each row remains24 bytes: actor UID, host UID, tag, kind, active, control word. Version1 retains its exact old semantics. Version2 requires exactly one active Jeep row whose control word is1, meaning a simultaneous player gunner; other Jeep control words remain0. Turret control word retains its original `owns_host_flag` meaning. The companion normalizes those into separate in-memory `owns_host_flag` and `player_gunner` fields. No handles are serialized, no new wrapper is introduced, and the wire size/buffer budget does not grow.

RFNS2 is emitted only for a living authored NPC driver plus the player at Jeep `interface_2`. It requires the normal active gunner session and generation-checked physical pair, role1, live driver action13 and the selected authored host UID. Saved vehicle must be profile3, alive, player occupied and role1. A dead, retired, unseated, independently acting or wrong-host NPC cannot use the exception. RFNS1 with an occupied NPC host remains rejected. Version2 with zero/multiple marked rows, arbitrary control bits or a turret marker does not gain admission.

## Integrated transaction

The early companion declarations precede NPC/player staging, and the implementation follows vehicle checkpoint helpers. RFNS capture validates the live generation-checked shared pair before mutation. RFNS decoding preserves authored identity, tag, cardinality and budget checks for both versions. RFVC capture retains all fire/projectile/cooldown/burn guards while admitting the explicit shared pair.

Player/NPC overlap admission is limited to the living player and the same living, nonretired authored driver confirmed by the RFNS2 companion. Exact saved seat placement and world/mover/third-party collision checks remain intact.

World load prepares the budgeted gunner stage after all NPC/seat admission. At commit, it detaches old seat bindings, publishes the staged host, assigns NPC ownership, restores route/mode state, then assigns the player gunner session after ordinary player state. The host commit is shared with existing single-owner restore and has no fallible operations. The new stage is freed on every path. No live possess callback, fresh action13 or route event is replayed.

Current-session gunner loads remain rejected before mutation; this slice implements fresh-boot continuation. Supported legacy publication still clears the second occupant word. The immutable38-word `rf_scene_jeep_gunner_restore` probe records composed state before resumed simulation.

## Focused native continuation

Parent command: `python tools/xemu_npc_jeep_gunner_save.py`. Prepare-only and AST checks do not launch or build anything. The harness reuses the complete authored pair and the grounded gunner fixture. Source180: Use90, nondefault gunner aim100..104, delayed route120, quiet save180. Freshload90: no setup file, gunfire20..34, live probe40..59, safe Use exit60. It compares the immutable composed probe against actual RFNS2/RFVC/RFPL/RFNC saved words for ownership, host pose/velocity, seated player/NPC positions, nondefault aim, ammunition and exit look. It then requires real mounted rounds and ammo debit, NPC driver health/link preservation, continued route commands without event replay, and a safe exit that leaves the NPC driving.

The real save bytes are read from the ordinary save service and loaded from the harness HDD; none are fabricated. Mounted-flight/cooldown saves, driver-death saves while the player remains gunner, loading while already a gunner, visual fidelity and campaign traversal remain outside this bounded slice.

The first source run (`npc-jeep-gunner-save-20261003-131034`) wrote a real RFNS2/RFVC3 ordinary save. Its initial harness check wrongly expected standalone RFPL3; the world snapshot path intentionally uses RFPL1 and validates seating with RFVC. That assertion was corrected, and `--resume-saved-run` continues the retained source on the same harness HDD without resaving or fabricating bytes.

## Xbox result (2026-10-03)

Source `npc-jeep-gunner-save-20261003-131034` and resumed load `npc-jeep-gunner-save-20261003-131318/report.json`: ordinary180-frame source save, fresh90-frame continuation, both stock64MiB. Source velocity[-.00057453,-.2724463,4.497043] survives bit-for-bit along with host/player/NPC positions, gun aim, ammunition and exit look in the immutable composed restore probe. RFNS2 restores the real driver and distinct player gunner exactly once. Two real mounted rounds fire after loading; ordinary Use exits without removing the driver or replaying setup/route events. Minimum free physical pages2733 (10.68MiB).

Current-session gunner quickload, saves during active mounted projectiles/cooldowns, driver-death gunner saves, visual/audio output and broad campaign coverage remain open or unverified. Existing safe rejections remain for those unsupported states.
