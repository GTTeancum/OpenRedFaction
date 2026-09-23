# Enable_Navpoint first pass

The campaign event `Enable_Navpoint` (type 69) now binds its authored link UIDs to
the level's navigation nodes after both owners load. An OFF action writes zero to
each linked node's live radius; ON restores the node's original radius bits. The
shared event dispatcher handles immediate and pending actions, and the scene's
navigation consumers read these mutable nodes. Missing UIDs remain inert.
When a node's radius changes, scripted NPCs whose cached remaining route uses
that node discard the route and search again on their next movement tick.
Reopening a node also releases an active scripted actor's failed-route retry
delay. This immediate replan is a first-pass port policy; original timing has
not been recovered.

The behavior comes from original binary ON handler `0x4bcfa0`, OFF handler
`0x4bd020`, post-load binder `0x4612cb..0x4613c5`, and navigation predicate
`0x40c570`. The bounded reconstruction and its limits are recorded in
`docs/research/secondary-re/campaign-navpoint-enable-20260916.md`. The port keeps
inert holes where the original binder removes missing links; surviving writes
retain their authored order.

The installed L6S3 test fires Invert 7124 into Enable_Navpoint 7123 OFF, checks
nodes 21, 58 and 56 reject a positive-radius query, then fires 7123 ON and
checks exact baseline bits and query admission. The full PC scene also accepts
process-local setup events: a 60-frame L6S3 replay of 7124 reports three OFF
writes ending at live radius zero; a 120-frame replay of 7124 then 7123 reports
three OFF and three ON writes ending at the authored radius bits. Both replays
exit successfully. The PC test, PC play build and NXDK build pass. One live
actor route is checked below; broader travel and Xbox runtime remain open.

The replay records are 60 or 120 legacy 24-byte zero-input frames in the ignored
`artifacts/navpoint-live/` directory. Run `rf_pc_play.exe --spawn-replay` with
`RF_REPLAY_LEVEL=L6S3.rfl`, `RF_REPLAY_ARCHIVE=levels1.vpp` and
`RF_REPLAY_SETUP_UID=7124` or `7124,7123`; the `SCRIPT_NAVPOINT` line reports
`3 0 0 3 ... 0` for OFF and `3 0 3 3 ... 1058642330` after ON. The focused
AI scheduler test confirms cached-route invalidation for an affected node,
preservation for an unrelated node, and immediate retry after reopening. These
checks do not by themselves establish live NPC travel through a changed route.

A separate process-contained L6S3 replay fires authored Goto event 6758 for
actor 6755 after the navigation action. With Invert 7124 leaving the three nodes
OFF, 120 movement ticks give `SCRIPT_ROUTES 2 0 2` (two failed searches) and
`SCRIPT_MOVE 1 119 0 0`; the actor reaches `(2.3126,-4.1386,-19.3807)` while
approaching directly. With 7124 followed by 7123 ON, the same 120 movement
ticks give `SCRIPT_ROUTES 1 1 0 2 4 3` (one four-node route and two waypoint
advances) and the actor reaches `(3.0508,-4.1386,-19.6469)`. No obstacle is
reported in either run. The ignored replay logs are `l6s3-goto-off.log` and
`l6s3-goto-on.log` under `artifacts/navpoint-live/`. This establishes one live
actor's route selection and movement response; it does not cover every NPC
class.

A separate 180-frame mid-route fixture dispatches Goto 6758 at frame 30 and
Invert 7124 at frame 60 while actor 6755 is moving. PC records one successful
route followed by two failed searches (`SCRIPT_ROUTES 3 1 2 1 0 1 0 0`),
compared with one successful search and no misses (`1 1 0 2 4 3 0 0`) when the
second event leaves the nodes ON. Stock 64 MiB XEMU matches PC exactly for
`SCRIPT_NAVPOINT`, `SCRIPT_ROUTES`, `SCRIPT_MOVE` and `SCRIPT_ACTOR` in
`artifacts/xemu/replay-20260923-020818/`; the full replay checker passes. The
process-contained replay timing override is `--goto-frame 30` in
`tools/xemu_replay_check.py`. Reopening a node mid-route and reload after an
active route remain unverified.

The ON/Goto case also completes 480 frames in stock 64 MiB XEMU. Guest memory
matches PC exactly for `SCRIPT_NAVPOINT`, `SCRIPT_ROUTES`, `SCRIPT_MOVE`, and
`SCRIPT_ACTOR`, including the actor's final position bits. The old broad replay
checker rejected `ACTOR_FOLLOW_SUMMARY` because it hashed the CPU mesh: Xbox
keeps static world and model vertices in GPU-resident buffers, while PC emits
them into that mesh. The final camera pose and scene-visibility summary match
exactly. In `artifacts/xemu/replay-20260923-014026/`, Xbox reports 9,354
visible retained world vertices in 1,002 draws and 10,215 retained model
vertices; its native 640x480 framebuffer shows the same room, door, lights,
weapon and HUD as PC. Only 631 of 307,200 pixels differ by more than two
color levels; mean absolute channel difference is 0.147. This is a comparison
of reconstructed PC/Xbox output, not PS2 parity. The revised broad checker
passes the full 480-frame stock-64-MiB run in
`artifacts/xemu/replay-20260923-015334/`, including retained-clutter
accounting and the native framebuffer capture. Mid-route OFF now passes the
separate fixture above; mid-route ON and native save/reload remain unverified.

## Ordinary save format

The environment component now writes RFEN2. It appends only nodes whose live
radius bits differ from their authored baseline as `(navigation UID, radius
bits)` rows, eight bytes each. Restore matches the first current node with
that UID, validates the saved radius as zero or baseline, and stages all writes
before publishing any gameplay state. RFEN1 loads with pristine navigation.
This UID-based representation is a port policy; the original save format for
live navigation radius has not been recovered.

The focused environment checkpoint check covers UID reorder, an unchanged
scene before publication, stale-state rejection, RFEN1 decode and restoration
of a zero radius. The event checkpoint codec now accepts type 69. In a full
L18S1 PC scene, Invert 10505 closes node UID 8637 and ordinary save writes
RFEN2 with one row `(8637, 0)`; the saved file is in the ignored
`artifacts/navpoint-live/` directory. A fresh L18S1 PC load now passes NPC
staging and player placement, restores the closed node `(8637, 0)`, and reports
`WORLD_SNAPSHOT_LOADED`. Hidden actors are excluded from physical NPC pair
clearance. The authored player start lies inside `BustedEscapePod` UID 10469's
coarse collision sphere; the port admits only the intact, unchanged pod and a
saved player within 0.125 horizontal units and 3 vertical units below that
start. Static standing, movers, and all other props remain checked. This is a
bounded port policy, not a recovered original-game save rule. Native runtime
reload and live NPC travel after this reload remain unverified.
