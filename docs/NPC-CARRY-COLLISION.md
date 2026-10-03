# NPC moving-platform collision and velocity

Integrated in the shared scene path and compiled with NXDK. The focused native result and its unverified branch are recorded below; no visual or broader campaign claim is made.

## Gameplay changes

When platform carry hits the actor's own upward-facing support, retry the full NPC body query while excluding only that support. The previous retry supplied an empty mover set and bypassed passive-vehicle and fragment queries, permitting unrelated moving obstacles to disappear from this collision decision.

`scene_npc_carry_collision.inc` uses the registered mover's existing collision-disabled pose flag0x40000, or the passive host's existing linked-owner exclusion. It restores the original flag/link immediately after the synchronous query, including errors. No allocation, event, callback dispatch or simulation commit is allowed within that temporary exclusion window. Geometry/collision source confirms that the pose flag reaches mover collision filtering without changing surface indices or registry ownership.

Ground-contact acceptance also now uses rotational point velocity. Collision contacts contain the mover-center velocity, which is zero for rotation about a fixed pivot. Copying that value erased the carried actor's tangential velocity after each successful ground query. `rf_scene_npc_support_interval_velocity` maps the accepted END-frame point back to the START frame and computes (current-previous)/interval duration. The lifecycle contact adapter uses it for a registered rotating support; translating/static/fragment/passive contacts retain their existing rules. Fall release preserves the final cached contribution, and static landing clears it.

## Bounded native check

`python tools/xemu_npc_rotating_support.py` uses DEV NPC8 and platform mode2, with read-only observations of ordinary carry, contact, falling and landing. It validates the first carry against an independent rational rotation and checks that ground acceptance retains point velocity. It requires the narrow own-support retry, natural support loss, inherited velocity on the next falling tick and a later static landing.

The explicit fixture places a mover at[9.449,5.55,2.5] and seeds the actor at[8.699,9,2.5]. The negative X offset is on the rising side of the prescribed clockwise quarter-turn. The dedicated `--npc-rotating-platform` builder option leaves other platform fixtures unchanged. Existing static face994 is at y2.375 under z2.5; nearby roof faces6497/6499 are at y16, so this fixture has room to fall. No host input, images, PC gameplay or campaign traversal is involved.

The initial lower-platform check (`artifacts/xemu/npc-rotating-support-20261003-120106/report.json`) failed its full acceptance criteria: first rotations matched the intended transform, but nearby static geometry caught the actor before falling, the carry retry was not exercised, and ground-contact velocity was zero. It is retained as failure evidence; it does not establish full support behavior.

Walking riders, rotating-support save/load, simultaneous unrelated-obstacle coverage and visual refinement remain outside this bounded check.

## Xbox evidence (2026-10-03)

`artifacts/xemu/npc-rotating-support-20261003-120851/report.json` remains **FAIL**, solely because the own-support retry counter stayed zero. The preceding gameplay assertions passed: first/second rotations matched the independent transform, accepted-contact velocity was[1.89108,1.46848,0], the actor naturally lost support at454 with inherited[1.31224,-.55687,0], retained it at455, landed at499, and stayed grounded through598 with inherited velocity cleared. The run completed600 frames on stock64MiB with1597 free pages and no reported owner/lifecycle errors; original disc controls were restored.

This verifies the rotation/contact-velocity/loss/landing slice. It does **not** verify execution of the newly narrowed collision retry or simultaneous unrelated-obstacle handling. That source-reviewed, compiled branch remains a targeted coverage item. The full harness acceptance gate is preserved, and no further fixture runs were made to chase this isolated branch.
