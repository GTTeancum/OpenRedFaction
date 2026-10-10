# Original grate: native contact and prepared destruction/release

The parent completed the 06:00 contact-only check on `ae2eec37`: `artifacts/xemu/20261009-0600-solid-prop/verification.json` reports `PASS_OWNER_QUALIFIED_CONTACT`, eight owner-qualified observations, real selected/owned pistol with 16 rounds, and successful disc restoration. The original preparation below is retained as the contact-mode contract.

The 07:00 `--destroy` extension at the end of this note is source-written in a separate worktree based on `ae2eec37`, manually inspected and completely unrun. It changes only this note and the existing harness. No active-checkout changes, compilation, syntax/test execution, emulation, cleanup, PC runtime, images or publication were performed.

## One bounded check

After the parent explicitly builds the integrated Xbox source in its single 06:00 UTC batch, run once from the active project root:

    python tools/xemu_player_solid_prop.py --parent-hourly-batch --out artifacts/xemu/20261009-0600-solid-prop --seconds 600

The harness never invokes a build or `make`. It packages the parent's existing XBE directly with the existing `extract-xiso` binary. The existing native runner owns one stock-64-MiB XEMU process and snapshot-only private HDD. The existing Fighter runner's `BorrowedSessionLock` keeps the parent lock held across disc staging, guest execution and restoration. No additional emulator is launched if a project session or lock is already present.

The original XBE/map are hashed and copied to `tested-default.xbe` and `tested-main.map` before launch. Affected flags are recorded in `disc-restore.json`. The original ISO is moved aside, then restored without a build; its complete SHA-256 and the flags/XBE/map are checked afterward. Original `levels1.vpp`, `tables.vpp` and `meshes.vpp` must match their staged copies. Original inputs are never edited. Generated ISO/run payloads are left to the parent's established cleanup, after inspecting evidence.

## Grounded replay, deliberately contact-only

The original L3S1 version180 clutter records locate UID6913 by UID and derive its runtime placement-array slot; the script does not assume a fixed slot. The same original level supplies the unmodified player start and default forward/right vectors.

- Frames0–59: neutral at original spawn.
- Frames60–179: fixed quarter-stick horizontal motion toward the authored grate center, projected into the original body basis. No look, fire, crouch, jump, Use, reload or weapon-cycle input.
- Frames180–239: release input and settle. This releases the control only; it does not remove the obstruction.
- Stop/report if a sampled player pose leaves a five-unit horizontal neighborhood of the original spawn. There is no campaign route or automatic second attempt.

The original miner1 table declares maximum velocity6 and acceleration20. With quarter-stick input, the source's grounded desired speed is1.5 units/second; two seconds targets roughly the2.7-unit neighborhood after acceleration, subject to actual collision and traction. This is source/math grounding, not evidence that the path is open or that the actor reaches the prop.

The grate is a thin vertical X plane at approximatelyX=-53.00034, with Y[-3.87206,-2.37794] and Z[-19.98157,-18.48745]. It is about93.65 degrees left of the default forward. The player begins close to its plane, so this short approach is toward its edge. A reliable normal-facing shot and post-destruction crossing were not established proportionally. The case therefore does not turn, shoot, fabricate a firing origin, grant inventory, dispatch an event or teleport. It verifies that the ordinary startup pistol is really owned, selected and still has its16-round clip, without using it.

## Observation and verdict

A minimal optional `live_sample` callback is added to the existing native runner. All existing callers remain unchanged. It stops only the owned guest briefly for read-only coherent owner snapshots, and is rejected when FPS measurement is requested. The existing response ring does not retain a contact object handle; the existing current body-hit does. Thus live sampling is needed to associate a real contact with UID6913 rather than an unrelated wall. No new guest telemetry or fixture is introduced.

The reader checks the32-bit Xbox owner layout against its token, UID, generation-bearing registry slot, class/material and authored pose. It records the existing current body contact, actual player pose/velocity, retained position ring, sweep and response counters, and inventory/life/error state. The fixed ABI offsets are documented in the script and fail closed on identity mismatch.

`PASS_OWNER_QUALIFIED_CONTACT` requires:

- Clean240-frame ordinary replay on64MiB, available memory, no death or level transition, and live neutral/push observations.
- The original retained UID6913 owner remains living, stationary at its authored position, physical, greater than0.5 extent, `collide_object` without `collide_weapon`, and Metal2.
- The real owned pistol remains selected, loaded16 and unfired.
- After a neutral sample whose current contact was not the grate, a later existing body hit names that exact registered handle, Metal2 and a real player sphere, static solid/velocity, strict fraction<1 and a point within the original mesh box. Sweep-hit and ordinary response counters must have advanced.
- No collision query errors; original flags, ISO and exact XBE/map must be restored unchanged.

If these environmental checks pass but no qualifying contact is sampled, the verdict is `INCONCLUSIVE_NO_SAMPLED_GRATE_CONTACT`, not a gameplay regression or a pass. The loop can miss a short-lived contact. The retained last body-hit may also survive a later miss, so its observation frame is not presented as the exact impact frame. Each live sample is saved as it arrives; raw native terminal evidence remains in the existing runner output.

This cannot certify sustained passage blocking, released obstruction after destruction, weapon-only exclusion, prop-top standing/save support, grate-break VFX, performance, visuals or retail parity. Do not expand an inconclusive result into a campaign route or elaborate automated shooting script during this batch. Report actual accepted owner/material contacts and the boundary above, then choose the next small step from the native evidence.

## One 07:00 destruction/released-edge extension, unrun

After the parent's single scheduled Xbox build, including the gameplay source and optional parent audio telemetry from `8ccd746a`, run the same harness once:

    python tools/xemu_player_solid_prop.py --parent-hourly-batch --destroy --out artifacts/xemu/20261009-0700-prop-release --seconds 600

This uses the unchanged original L3S1, archive hashes, owner-identity checks, session lock, native runner, existing test HDD and exact disc restoration. It does not add a level fixture, force a damage event, grant equipment, replace a camera, teleport, build, capture images or run a second attempt. Missing required evidence is a failed/inconclusive check, never an assumed hit.

- Frames 0–239 reproduce the successful contact recipe unchanged.
- Frames 240–299 apply only ordinary look: target body yaw -0.05 radians, eye pitch -0.35 radians. The yaw input is derived from the original spawn forward; the 60-frame turn uses the actual authored player rate of 2 radians/second at 1/60 seconds. See `PLAYER-AUTHORED-LOOK-RATE.md` and `actor_listener_pose`.
- Frames 300–329 settle and provide a pre-shot pose/health/ammo observation window.
- Frame 330 presses ordinary primary fire once. All other button fields stay zero. At this historical preparation, slot-0 Pistol used eye-forward directly; that premise is superseded by the source-written [PLAYER-PRIMARY-SPREAD.md](PLAYER-PRIMARY-SPREAD.md) correction. Its sampled ray can differ from the retained eye-forward ring, so the old straight-ray geometric expectation is not evidence for the corrected shot. This harness is preserved, not rerun or adjusted to force a hit. A miss remains a miss.
- Frames 331–359 permit native death/break processing before renewed movement.
- Frames 360–479 repeat the original quarter-stick world-space push, reprojected into the new body basis. Frames 480–539 release input; the check then ends. The five-unit original-spawn neighborhood guard remains active.

### Camera and narrow-edge grounding

The 06:00 samples settle at approximately `(-52.976448,-3.118479,-20.584421)`. Original-model standing eye metadata is approximately `(0,0.785403,0)`. The reconstructed `look_basis_seed` uses forward proportional to `((1-|sin(pitch)|)*sin(yaw), sin(pitch), (1-|sin(pitch)|)*cos(yaw))`; substituting a conventional cosine-pitch camera here would be wrong.

The selected near-+Z direction meets the grate's original near-Z edge around `(-53.0066,-2.65,-19.98157)`, inside the thin X slab `[-53.01591,-52.98478]`. Center aiming from this near-coplanar stop has less thickness margin. These numbers justify a single bounded attempt; they do not prove a mesh hit or unobstructed shot.

The existing 46-word `rf_scene_actor_eye_frames` ring supplies the actual frame-330 eye position/forward, and existing `actor_look` supplies achieved angles. The harness requires the captured ray to cross the original near-edge slab with more than 3 mm X/Y interior margin. That ring is recorded before optional camera effects; actual same-owner damage/retirement is required independently, so this geometric check cannot manufacture a hit. There is no adaptive aiming or guest-memory write.

### Required result

`PASS_OWNER_DESTRUCTION_RELEASE` requires the contact-mode checks in the original pre-turn window, plus:

- At least 24 retained frames during the first held push with under 0.02-unit movement on every axis, alongside the exact-owner grate contacts.
- Sampled achieved aim within 0.001 radians, still-living grate before fire, and actual shot-frame eye-ray geometry within the slab.
- Exactly one real pistol shot, loaded ammo 16 to 15, unchanged reserve, zero reloads and no ammo/combat error.
- Exactly one applied damage/retirement naming UID6913, health at or below zero, bit-2 retirement observed before the second push, and the same retained pointer/registry handle/UID/authored position throughout.
- Ordinary movement at the original ground height, within 0.25 units of the grate's X plane, advancing beyond its old near-Z edge by over 0.25 units while retired. This proves release at the previously obstructing edge, not traversal of an entire passage.
- Clean collision queries, completed 540-frame stock-64-MiB replay, no death/transition, available memory and verified restoration.

Destruction mode stages the existing `audio-output.flag` and restores its prior state; deterministic replay otherwise disables the normal `player_pacing` audio-open path. The existing native runner keeps its host audio sink muted. When the parent-built map exposes `rf_scene_clutter_break_audio[10]`, require one request and one successful native start, zero audio errors, UID6913, a pre-second-push frame, authored vclip 62 and the lowercase FNV-1a filename hash of `Metal_hit_Metal3.wav`. Otherwise audio evidence is explicitly unavailable. This is native voice-request/start accounting, with no PCM capture or human-listening claim.

The extension requests no prop-top save, NPC/dynamic-prop check, VFX/debris assertion, performance result, campaign traversal or broader fixture framework. Parent owns the one hourly run, evidence review, cleanup and task-list update.
