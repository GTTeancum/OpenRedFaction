# Authored NPC freeze/wake ordinary saves

Originally source-written 2026-10-09 for the parent 10:00 Xbox batch. The parent
subsequently compiled the integrated code and reached an ordinary native save;
the observer failure and bounded recovery are recorded below. No fixture,
original-asset mutation or campaign traversal was used.

## Concrete owner and blocker

Original L11S3 Turn_Off_Physics10651 targets Eos10636 and miner10637. The
existing event62 callback already freezes/wakes their real registered bodies;
its earlier bounded Xbox result is recorded in VEHICLE-PHYSICS-STATE.md.
Ordinary world capture nevertheless rejected the entire save whenever any
NPC retained `scripted_physics`, before the RFNC owner could be serialized.

Read-only original-archive confirmation: `levels2.vpp/L11S3.rfl` entry offset
17197056, size1905210 contains Trigger Auto12195 (flags[0,0,0,1,0], tail0,
empty script) -> Delay12214 (delay0) -> Turn_Off_Physics10651 (delay0).
The auto trigger's timing30 is its cooldown; the existing startup dispatcher
fires it immediately. This establishes a neutral-start path to the real owner,
without movement, a campaign route or injected events. An ordinary save still
has to satisfy all independent level-state admission and runtime constraints.

Original ON4b9380 calls417e00: unless object bit08000000 grants immunity, body
flags become `(old & 67ffffff) | 18000000`, and linear/angular/momentum vectors
are zeroed. OFF4ba180 calls40a420: body80000000 and object06000000 are set.
Freeze does not clear force/torque or stop animation/parent transforms. A later
impulse may wake the body while the scene's script marker remains set.

## Bounded representation

RFNC14 keeps the existing600-byte base row and optional payloads. Its fixed
76-byte tail follows the RFNC13 movement tail on every row:

- Presence, script marker, masked body bits99400001, object bits06000000.
- Five XYZ vectors: linear velocity, angular velocity, momentum, force, torque.

An explicit scene `scripted_physics_seen` bit is set after an accepted NPC
ON/OFF callback. This also distinguishes a standalone OFF or already-woken
owner from an untouched actor. Presence is not inferred from generic sleeping
body flags. An affected living actor selects version14 for the whole component;
mixed rows receive explicit zero/absent tails. Untouched ordinary actors retain
version13, and older10/11/12 writer selections remain when appropriate.
RFNC1–13 readers retain their exact layouts and decode the new fields as zero.
The maximum full row grows from1504 to1580 bytes. All existing total-budget,
identity, checksum, sorted-UID and error-atomic output rules remain.

The existing quiet linear-velocity limit, absolute component<=.001, remains.
Every retained vector must be finite. Marked, actually suspended bodies require
both original sleep bits; marker1 with body80000000 is a valid separately
represented state. Unknown mask bits, absent nonzero payloads and malformed
presence/marker values reject. No pending collision override, frozen death or
affected seated NPC is newly admitted. A completed death after wake continues
through the existing corpse profile. Other NPC animation/combat/route/attachment
admission checks are unchanged.

Body01000000 is retained because `rf_angular_velocity_step` checks that existing
prepare-contact gate before consuming angular velocity/torque. Moving-support
bit00400000 and a saved support UID must agree in both directions; suspension
cannot discard a support owner to obtain no-floor admission.

## Staging and publication

`scene_npc_physics_checkpoint.inc` captures the real owner and restores only
private candidate state, after `rf_physics_publish_position` so its dirty-bit
normalization cannot overwrite saved event bits. Class, sphere and unrelated
descriptor flags remain from the existing body reconstruction. Identity and
immunity checks precede publication. The candidate retains all five vectors;
neither ON nor OFF is replayed during restore.

Placement uses candidate saved falling/support state for affected rows. A truly
script-suspended actor without a moving support may lack a floor. It still
passes full volume, room, candidate mover, stationary-prop and actor-pair checks;
this does not add an authored-surface or overlap exception. Saved moving support
still requires its existing owner/contact admission. Non-suspended rows keep
their ordinary floor requirements.

Final assignment restores both marker and presence. Loading an absent/pre14
row over a later event-affected live owner resets only that event-bit baseline
and retained motion/force vectors, rather than inheriting the later freeze.
Unrelated actor state and the existing assignment-only transaction remain.
The older standalone RFCP guard remains unchanged; this slice targets ordinary
RFWC/RFNC saves. No new physics action, actor pool or generic checkpoint wrapper
is introduced.

`rf_scene_npc_physics_restored[8]` reports affected assignment count, suspended
assignment count, latest UID, handle, presence, marker, body bits and object bits.
It is observational and does not issue an action.

## Integration and verification boundary

Owned files are `include/rf/npc_checkpoint.h`, `src/core/npc_checkpoint.c`,
`src/diagnostic/scene_npc_physics_checkpoint.inc` and this document.
The parent hook patch covers the scene owner/include, successful event callback,
capture/restore, ordinary snapshot veto and candidate world-placement adapter.
The integrated code passed the parent 10:00 Xbox build and bounded native save.
Fresh-load publication and freeze-to-wake runtime remain unverified.

Existing Python RFNC readers need version14 tail handling before they can be
used to inspect an affected save. Examples include `xemu_corpse_save.py`,
`xemu_corpse_lifecycle.py`, `xemu_corpse_unsettled_save.py`,
`xemu_auto_turret_attack_save.py`, `xemu_auto_turret_removed_save.py`,
`xemu_single_fire_save.py`, `xemu_npc_opposed_save.py`,
`xemu_npc_jeep_detached_save.py`, `xemu_npc_jeep_gunner_save.py`,
`xemu_jeep_unpiloted_save.py`, `xemu_passive_jeep_driver.py`,
`xemu_vehicle_rotating_support.py` and `xemu_vehicle_released_driver.py`.
No such legacy reader was modified or run for this slice; update only a
reader actually selected for a future parent batch. Their existing historical
fixture policies are not authorization to create or run new fixtures.

## Prepared 10:00 parent batch

After one Xbox compile, tools/xemu_frozen_npc_save.py reuses the native runner for120 neutral originalL11S3 save frames, followed only on a successful actual save by32 fresh-load frames. It uses a private standalone copy of the owned HDD, direct ISO packing and exact selector/XBE/map/ISO restoration. Native RFNC14 bytes are read, never synthesized or repaired. Original automatic startup runs naturally in both fresh processes; explicit RFNC assignment telemetry distinguishes actual load publication from startup freeze. Stop on a failed save, with no second attempt. Wake/pre14 migration and vectors absent from existing live telemetry remain outside the runtime claim. This wrapper is prepared only and unrun until10:00.

## 10:00 result and strict observer correction

The parent build at `aad0bdd2888dcec2a25a0c9e13dedcd5e120516f` passed. The
original L11S3 run reached guest phase 5 after 120 neutral presentations in
64 MiB, with 4,089 pages free. Native storage published 40,844 bytes at
generation 98, slot 1. The wrapper then failed with `Stale owner sample` before
launching a fresh-load process. Its original `verification.json` remains
`CHECK_FAILED`, byte-for-byte unchanged, under
`/workspace/shared/rf-frozen-npc-20261009-1000`.

The failure was in the expected observer index, not the native save:

- `src/diagnostic/scene.c:19654` presents the current zero-based frame first.
  Line 19660 admits the next simulation step only when
  `frame + 1 < rf_scene_actor_frame_count`.
- The sole live call to `scene_script_physics_sample(frame)` is inside that
  guard at line 19853. The sampler writes that exact argument to word 12 of
  each row (`scene_script_physics_state.inc:21`). Sampling is once per admitted
  simulation step, with no terminal resample or independent time cadence.
- The final presentation is frame 119, but the final simulation sample is 118.
  Snapshot capture follows the completed animation stream at scene.c:21108;
  successful terminal phase 5 is published later at xbox/main.c:1304.
- The corrected observer requires exactly N-2, never an age range. It also
  requires phase 5, presented and configured counts both N, complete replay
  `[0,N,N,0]`, and exactly two full owner rows. For the planned 32-presentation
  load, the only accepted sample index is 30.

Read-only inspection of the existing captured native bytes confirmed both
RFWC/RFNC length and FNV checksums. The RFNC14 component is 15,416 bytes at RFWC
offset 864, with 19 rows. Exactly Eos 10636 and miner 10637 have present/scripted
tails; each has body bits `0x18000000`, object bits `0x02000000`, health 100,
no retired/dead/support owner, and five positive-zero XYZ vectors. Their saved
position, health, masked flags, linear velocity and angular velocity match
the actual frame-118 observations bit-for-bit. The other 17 physics tails are
explicitly absent and entirely zero. Momentum, force and torque are visible
in the save bytes, but have no independent live telemetry comparison. No
fresh-load or wake result is inferred from these observations.

RFWC SHA-256:
`34c15d8bbc0a1475bcfc8be4bc8bbca9cd1b41413ed29f4b398d69fc327b81e0`.
RFNC component SHA-256:
`72bd0e414773cc43bbb4db0bdd7f0c5345bdf99f9200138f39666bf7874db4cb`.
Actual saved standalone HDD SHA-256, pinned during this read-only diagnosis:
`129855eb9c0f311c52656c35f5443913a2a18b93e5ee1d348c5470e77139373b`.

## Prepared 11:00 fresh-load-only continuation

`--resume-saved` accepts only this exact hash-pinned prior report, result,
native byte capture, private HDD, tested XBE and map. It requires the current
disc XBE/map to match that producer and confirms the producer commit exists;
it stops on a mismatch without staging another binary. Original archives,
level/startup recipe, native save checks, RFNC14 rows and strict N-2 observations
are rechecked before the sole load process. The actual saved HDD is opened by
the existing native runner with `-snapshot`; no payload is generated, injected,
repaired, or written back. The new report records the original failure and
reused save evidence explicitly. The original report/evidence and saved HDD
are hash-checked again afterward, with ordinary disc/ISO restoration preserved.

Parent-only command for the next scheduled batch, not executed by the helper:

```sh
python tools/xemu_frozen_npc_save.py --parent-hourly-batch \
  --resume-saved /workspace/shared/rf-frozen-npc-20261009-1000 \
  --out /workspace/shared/rf-frozen-npc-20261009-1100
```

The observer/resume changes have only been written and source-reviewed. No
compile, test, emulator launch, fixture, save retry or Git publication was
performed for this correction. Fresh-load success and assignment of both
suspended owners remain the parent's next runtime check.
