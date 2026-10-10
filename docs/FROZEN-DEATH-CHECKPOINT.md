# Terminal frozen-NPC ordinary saves

Source-written 2026-10-09. Compilation and runtime remain unverified until the
parent's consolidated Xbox batch. No build, test, emulator, fixture, original
asset mutation or campaign traversal was performed for this slice.

## Original ownership

The source reconstruction distinguishes the dead NPC's existing physics body
from its separately allocated corpse. Original death entry at `41fe27` through
`41fe59` conditionally clears linear velocity, clears angular velocity, sets
entity810 bit1 and clears body1a8 bit `8000`. It preserves source-body freeze
bits `18000000` and wake bit `80000000`. The separate corpse construction at
`416a35` through `416aaa` creates a type7 body with flags `33`/`73`, without
inheriting the source freeze. The reconstructed death entry is
`rf_entity_death_entry_sp` in `src/core/entity.c`; that file's corpse
construction and `scene.c`'s live corpse adapter preserve the separate owner.
The earlier death-prefix evidence is recorded in
[DEATH-LIFECYCLE.md](DEATH-LIFECYCLE.md).

Consequently, restoring a terminal source actor must preserve its accepted
physics state rather than thaw it or apply that freeze to the corpse. This
change does not alter damage, death entry, dying update, corpse construction,
retirement, animation timing, event delivery or audio. The existing source
retirement clears its two script-physics markers only after closing the body;
that ownership boundary is unchanged.

The earlier freeze-save owner is documented in
[NPC-PHYSICS-CHECKPOINT.md](NPC-PHYSICS-CHECKPOINT.md). Its historical exclusion
of frozen deaths is superseded only by the bounded terminal profile below.

## RFNC17 semantic extension

RFNC17 has exactly the RFNC16 byte layout: the same 600-byte base, optional
movement/look, combat, queued-shot and animation payloads, followed by the
12-byte movement, 76-byte physics, 24-byte pain and 4-byte holster tails on every
row. The maximum full row remains 1608 bytes. No field, pointer, handle, pool,
allocation or resident telemetry is added.

The writer selects17 only when at least one row combines `physics.present`
with `dead_pose`. Other components retain the existing10 through16 selection.
Version selection is monotonic across sorted rows, including a later living
holster row after a terminal physics row. RFNC1 through16 keep their existing
layouts and admission: decoding or preflighting a pre17 row with both present
physics and `dead_pose` explicitly fails. RFNC17 includes the unchanged RFNC16
tails, including zero pain and holster values for the terminal row.

The existing physics validator now admits present physics on a nonretired
terminal `dead_pose`; the main row validator still requires finite fatal
health, the death flag, a valid death-action range, one retained animation clip
and no active script animation. It still excludes live-only movement, pain,
holster, queued shots, combat orders and moving support from dead rows. Scene
capture and restoration retain their real class/motion, death-timer and
settled-or-admitted-in-progress death-clip checks. Merely setting `dead_pose`
does not bypass these checks.

All previous physics checks remain:

- Presence and script marker are boolean; absent payloads are entirely zero.
- Body mask `99400001` and object mask `06000000` have no additional bits.
- Every vector component is finite. Linear velocity remains quiet, with an
  absolute per-component maximum of `.001`; angular, momentum, force and
  torque retain their previous finite-only admission.
- A marked suspended body, `scripted && !(body_bits & 80000000)`, requires both
  original `18000000` sleep bits. Marked-but-awake and seen-only post-wake
  states remain distinct and are retained without normalization.
- Saved support UID must agree with body support bit `00400000`; supported
  owners cannot also have the falling bit. The main validator rejects every
  dead moving-support owner, so this slice introduces no dead-vehicle support.
- Retired actors and affected seated owners remain excluded.

## Capture and candidate restoration

`scene_npc_physics_checkpoint_capture` runs after the existing terminal death
admission. It now copies the real source body's masked bits and all five
vectors for both retained script markers and seen-only affected owners. A dead
actor never touched by the physics event still has an absent tail. Retired
owners keep the previous exclusion, and no missing physics owner is invented.
Current registry identity, body allocation, linked/seat/attachment, pending
contact override and script-physics immunity checks are unchanged.

The existing prepare and assignment functions are unchanged. Saved bits and
vectors are installed on the private candidate after position publication;
class, descriptor and sphere state stays reconstructed from the real owner.
Final assignment restores only the admitted source state and its two physics
markers. No ON/OFF, wake, support, damage, death or corpse callback is replayed.
Legacy/absent restoration retains its prior event-state baseline behavior.

Candidate placement continues to use saved body flags. Only the already
existing truly script-suspended/no-moving-support case can use its no-floor
admission; death alone adds no floor exception. Complete room, volume, mover,
prop and actor-pair clearance checks remain. Non-suspended bodies retain the
existing support rules. The separate owned corpse and its RFCL lifetime state
continue through their existing owners.

## Verification boundary

Owned changes are limited to `src/core/npc_checkpoint.c`,
`include/rf/npc_checkpoint.h`, `src/diagnostic/scene_npc_physics_checkpoint.inc`
and this document. Read-only source review covered terminal admission, codec
version selection, unchanged row-span/tail handling, candidate preparation,
placement and assignment. The parent owns shared scene wiring, integration,
compilation and proportional Xbox validation.

No new per-slice fixture or broad test run is supplied. A future scheduled
check should distinguish preserved source freeze from independent corpse
state, and verify version17 persistence and exact saved masked state before
claiming runtime support. Mixed living-holster/terminal-physics ordering and
pre17 rejection are compatibility boundaries for that consolidated validation.
Existing analysis scripts that cap RFNC at16 need a version17 semantic update
before inspecting a newly affected save; its row parser itself is unchanged.
