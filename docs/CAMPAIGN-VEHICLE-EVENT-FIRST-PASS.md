# Campaign vehicle event first pass

Original `RF.exe` SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:
the `When_Enter_Vehicle` and `When_Try_Exit_Vehicle` polls at `0x4B8F30`
and `0x4B8FD0` consume local-player bits `0x400` and `0x800` in event-list
order, before checking links and without consulting the monitor's disabled
bit. `Never_Leave_Vehicle` ON/OFF at `0x4B9BD0`/`0x4BA3F0` changes actor
flag `0x80` at offset `0x814`; ordinary player-use exit checks that flag.
The independent original-code probe and authored inventory are in
`docs/research/FUTURE-CAMPAIGN-VEHICLE-EVENTS-20260915.md`.

The shared event runtime now consumes both pulses with original event-first,
mover-second link filtering and sentinel source/actor handles. The existing
DEV vehicle-use path emits a boarding pulse after successful entry and an
attempted-exit pulse on a living driver's Use edge, including an exit that is
blocked by the lock or collision. Type 80 sets or clears the linked entity
view's exit-lock bit; the ordinary Use-key exit honors it while scripted
forced detach remains separate. Focused checks cover absent player, a
disabled empty first monitor stealing a pulse, simultaneous pulse bits,
ordered duplicate monitors, linked event/mover effects, and a restored seated
driver’s locked then unlocked exit. Types 77/78/80 can be encoded by the event
checkpoint; a queued type-80 effect still blocks ordinary saves because its
live entity lock bit has no persistence record.

The PC build and NXDK build pass. The image-free 460-frame Jeep DEV replay
matches PC/Xbox state on stock 64 MiB XEMU and leaves 3,018 physical pages
(11.79 MiB) free; see `artifacts/xemu/render-20260926-181226/report.json`.
That replay exercises vehicle ownership and control, but does not contain
authored campaign vehicle events.

The first campaign vehicle is L12S1's authored `Jeep01`, UID 7629. Its
position and orientation come from the level entity record, and its live owner
is registered before event links resolve so scripts can target that UID. The
campaign keeps its normal world-image reserve and player/NPC weapon demand
when this vehicle is present. The old ctf06 ground fixture is confined to the
DEV room. A 120-frame process-local Use replay boards the Jeep on PC and
stock 64 MiB XEMU with identical `VEHICLE` and `JEEP_SEATS` state; the native
run has 3,243 pages (12.67 MiB) free. An ordinary 90-frame L12S1 spawn also
passes. Both runs used `--no-images`; see
`artifacts/xemu/render-20260926-182639/report.json` and
`artifacts/xemu/render-20260926-182349/report.json`.

The boarding pulse now activates L12S1 `When_Enter_Vehicle` UID 9694, which
starts `Follow_Waypoints` UID 9692 on the Jeep's live handle. The shared rigid
owner follows the 34-node `jeep_path` with bounded steering and throttle; the
first two node arrivals and vehicle pose match exactly across a 480-frame PC
and stock Xbox replay, with 3,210 pages (12.54 MiB) free. A separate 180-frame
PC/Xbox replay boards and exits the Jeep with identical vehicle and seat
state and 3,243 pages free. Both runs used process-local input and
`--no-images`; see `artifacts/xemu/render-20260926-184256/report.json` and
`artifacts/xemu/render-20260926-183240/report.json`.

The vehicle collision sweep previously saw a resting floor at fraction zero
on every nearly horizontal motion, preventing the Jeep from leaving its
spawn. A 5 mm elevated recheck admits floor-tangent travel only after
checking the raised path for another obstruction; the committed chassis
height is unchanged. Focused vehicle collision checks pass. This correction
is shared with other ground vehicles and still needs broader slope/step
coverage.

One authored static vehicle owner now loads in each of three campaign
sections: L1S2 `Driller01` UID 8122, L1S3 `APC` UID 9627, and L12S1
`Jeep01` UID 7629. Each uses its level record's position and orientation and
registers its real UID before event links resolve. Staged 120-frame boarding
replays match PC/Xbox vehicle state on stock XEMU. The L1S2 Driller has
4,983 pages (19.46 MiB) free; the L1S3 APC has 4,993 pages (19.50 MiB) free.
In separate 220-frame drive/fire/exit replays, both vehicles move and exit on
PC/Xbox; the APC launches three primary shots with matching state. The Driller
has no drill contact in that short campaign replay; the later contact test is
described below. All runs used `--no-images`; reports are
`artifacts/xemu/render-20260926-184928/report.json`,
`artifacts/xemu/render-20260926-185103/report.json`,
`artifacts/xemu/render-20260926-185319/report.json`, and
`artifacts/xemu/render-20260926-185512/report.json`.

Campaign drilling required an editable authored terrain owner, collision and
render publication, checkpoint history and a stock-memory budget. UID8123 now
supplies that first L1S2 room-8 owner. The L1S1 wall's dedicated room cut is a
separate path. The earlier 220-frame drive replay recorded no drill contact;
it proves boarding, driving and exit only.

The installed L1S2 editor source narrows the excavation candidates. A
read-only decode of all 389 declared brush records places the authored
Driller UID 8122 at `(109.0512,-3.57238,-12.2843)` inside the bounds of
brush UID 8117, `(101.5,-4.88,-17.5)..(116,1.12,4.5)`. UID 8117 has
52 source faces and 70 compiled faces, all in room 8, linked by the exact
face-source words. `python tools/probe_campaign_geomod_brush.py L1S2.rfl 8117`
confirms a closed convex cavity but not a convex solid; its operation word is
2, so treating it as one small solid post would be unjustified. Nearby UIDs
8120/8121 are operation-0 brushes, each with 26 source faces and 25 compiled
room-8 faces. This identifies
specific source owners and a room for a campaign-world cutter, but does not
establish the drill bit's first hit, cut admissibility, or a valid Boolean
publication strategy. `python tools/probe_campaign_geomod_brush.py L1S2.rfl
--inventory` now consumes all 839,569 editor-section bytes across 389 brushes;
38 records have opaque sidecars, the largest 17,568 bytes at UID 245. These
sidecars are bounded by validated successor headers, not decoded as Boolean
operations. A complete owner must preserve the
neighboring compiled face identities and room collision, then be exercised
by actual Driller contact, cut, save/reload and stock-memory checks.

The first real contact is **not** in spawn brush 8117. A deterministic
330-frame PC drive/turn/fire replay (`python
tools/check_campaign_driller_contact.py`) reaches the south wall twice:
frames 249 and 296, both room 8, compiled face 768, source word 283, owned by
operation-2 cavity brush UID 8123. The contact centers are
`(121.208061,-2.12633848,-17)` and `(124.314514,-2.1283164,-17)`.
The first contact now accepts one cut. The ordinary scene binds UID 8123 as
the terrain owner, replaces its compiled room faces with a 171-face
publication, and reports one committed cut and one draw generation. The
second contact still returns `RF_NOT_FOUND`, so repeated-cut behavior remains
open. The harness uses process-local input and text output. The PC replay and
a 330-frame stock 64 MiB XEMU replay match on the source cut counters,
publication face/vertex/cut/generation counters and terrain cut generation;
XEMU finishes with 3,801 free physical pages (about 14.85 MiB). The native
report is `artifacts/xemu/replay-20260927-073902/report.json`. No images were
captured. A focused composed-world ray now hits face 768 at z=-17 before the
cut and the recessed surface at z=-17.613625 after it, while an adjacent ray
still hits z=-17. Live actor traversal remains unverified; post-cut ordinary
saves are currently rejected as described below.

The second transaction reaches publication but its cutter bounds
`(121.107674,-2.405448,-19.962753)..(126.905739,0.702065,-13.505847)`
overlap editor cavity UID 8219. That brush has 14 source faces and 32
compiled faces spread across rooms 8, 13 and 123. A read-only geometric
probe (`python -B tools/probe_driller_second_cut.py`) uses the recorded live
frame-296 basis, the original-derived single-bit template, the real 0.4-unit
downward shallow limit and installed editor brush geometry. Intersecting the
26 cutter tetrahedra with UID 8219's convex cavity proves positive penetration:
the largest interior margin is about 0.119 units at
`(126.744,-1.903,-17.181)`. Clipping the compiled brush polygons against the
same tetrahedra finds positive-area contact on room-8 faces 5780 and 5784;
none of UID 8219's room-13 or room-123 polygons is touched by this particular
cut. A wider sweep also finds four intersected flags-8 detail faces belonging
to UID 9996 in room 121. Thus the broad-box rejection is conservative in
general, but **this cut reaches two additional owners**, not a false positive.
The C core now decodes UID 8219 and produces a combined UID 8123/8219 room-8
candidate; a focused collision ray verifies that it removes face 5780 and
exposes a recessed crater. The shared room importer and star cutter also trim
room-121 detail from 30 compiled faces to 74 retained fragments; a focused
collision-composition ray confirms that face 4972 no longer blocks at the cut.
The exact UID 8123/8219 pair now commits a smaller first cut through the
scene's grouped transaction in a PC harness. Replaying the recorded second
contact then selects both sources but rejects at UID 8123 obstacle admission,
leaving the first cut and publication serial unchanged. That guard remains
necessary until UID 9996 room-121 detail participates in the same edit.
This is not live admission: the two rooms still need one scene-level bind,
checkpoint history and renderer staging. See
`docs/research/L1S2-DRILLER-SECOND-CUT-20260927.md` for the measurements.

The first cut now survives an ordinary campaign quick-save and a fresh load on
PC and stock-64-MiB XEMU. This exact L1S2 UID 8123/room-8 profile writes RFDS2
in the world's destruction section; load rebuilds and publishes the authored
collision before admitting NPCs, the seated player and the Driller. A live
330-frame save at frame 270 wrote 59,902 world bytes, and the 60-frame fresh
load restored the 171-face, 707-vertex, one-cut publication. PC/Xbox terrain
and vehicle state counters match. Native save ended with 3,752 free physical
pages; fresh load ended with 3,720. Both runs were text-only with no image
capture (`artifacts/xemu/render-20260927-081157/report.json` and
`render-20260927-081415/report.json`). The focused RFDS2 PC stage test still
covers the exact 48-face source without detached pieces. This does not cover
the next Driller cut, other terrain owners, resumed drill motion after load,
or visual content.

The shared C asset decoder now admits this exact L1S2 UID 8123/room-8
source. It validates the 389-brush section with bounded successor-header
recovery for opaque editor sidecars, then retains 48 authored faces and 83
compiled windows; a focused PC check reports 26,652 resident bytes and a
959,885-byte decode peak. The existing ctf06 decoder check also passes.
The shared cutter now supports 64 source faces, with matching plane storage,
support-ID ranges, source filter storage and publication validation; convex
cutters remain bounded to 32 faces. A focused PC check opens UID 8123's 48-face
cavity within a 1,152 KiB core budget, applies the single-bit Driller template
at the recorded first-contact point using an identity test basis and unit scale,
and publishes 83 authored windows with the resulting cut. The core has 113
faces after the cut; the publication has 169 faces, including 16 crater faces
and 153 retained faces. Core resident memory is 889,672 bytes and cut peak is
1,014,012 bytes;
reset restores the original 48 faces. The focused geometry check and NXDK
build pass. Scene identity capture and actual vehicle-basis contact are now
integrated in the PC replay. This focused core result uses an identity test
basis, so its 169 faces differ from the 171 published by the live vehicle
basis. The native runtime and stock-memory checks now pass; live traversal and
save/reload behavior remain open.

An ordinary L1S3 save now retains the seated APC driver. The campaign player
class eye offset is refreshed before boarding, so the live seat pose and
checkpoint seat pose agree. The ordinary loader stages the occupied host and
player together, permits their intentional overlap, and restores possession
after publishing the saved player state. A 220-frame drive/fire replay saves at
frame 130; the stock 64 MiB Xbox run passes with no image capture
(`artifacts/xemu/render-20260926-195348/report.json`). A fresh 64-frame load
and a separate drive/fire continuation both match PC/Xbox vehicle state; the
continuation fires three APC primary shots and retains 4,816 free physical
pages (18.81 MiB). See `render-20260926-195605` and
`render-20260926-195825` in that same artifact directory. Other vehicle
classes and broader seated save points remain to be exercised.

The L1S2 Driller now also saves while seated and moving. The removed trigger
and stationary turned miner in that section required ordinary-world restore
admission; both are retained on PC/Xbox fresh load. All 16 native save sections
match PC, and the loaded driver/vehicle state matches after 64 frames with
4,790 free physical pages. No images were captured. Reports are
`artifacts/xemu/render-20260926-202409/report.json` and
`render-20260926-202555/report.json`. Later runs establish first drill contact
and post-cut save/load below; native post-load driving remains to be checked.

The L12S1 Jeep now also saves with its 34-node authored waypoint route active
and the driver seated. A fresh stock 64 MiB Xbox load retains route node 1
and matches PC route and vehicle state after 64 frames, with 3,033 pages free.
The save's 16 components are byte-equal to PC. No images were captured;
reports are `artifacts/xemu/render-20260926-204039/report.json` and
`render-20260926-204254/report.json`. Exact uninterrupted travel after reload
and the remainder of the route still need coverage.

This is one selected owner per section, not general campaign vehicle
creation. L1S3 contains other APC instances, and other sections and vehicle
classes still need ownership. The remaining Jeep route nodes, scripted
interactions and NPC traffic need live coverage. The first-pass waypoint steering is a playable
port policy, not an exact original AI driver reconstruction. The vehicle exit
sweep does not yet account for nearby NPC bodies. L7S3's authored
`Never_Leave_Vehicle` has no links, so no implicit current-vehicle lock is
invented. Pulse state and active exit locks are not yet represented in ordinary
world saves. The active vehicle route cursor is now saved. No visual image capture was
used for this work.
