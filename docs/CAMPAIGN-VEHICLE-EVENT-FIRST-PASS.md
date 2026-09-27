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
has no drill contact in that short campaign replay, so actual L1S2 excavation
remains unverified. All runs used `--no-images`; reports are
`artifacts/xemu/render-20260926-184928/report.json`,
`artifacts/xemu/render-20260926-185103/report.json`,
`artifacts/xemu/render-20260926-185319/report.json`, and
`artifacts/xemu/render-20260926-185512/report.json`.

The campaign drilling gap is structural, not just an untested control input.
`scene_driller_excavate` requires `terrain`, `terrain_authored` and a template
before it can publish a cut, while ordinary L1S2 loading does not open the
editable authored terrain owner (currently opened for `ctf06.rfl`). The
L1S1 wall's dedicated room cut is a separate path. L1S2's vehicle contact
must be connected to an admitted campaign GeoMod source with collision,
render publication, checkpoint history and a stock-memory budget before
campaign drilling can be called functional. The existing 220-frame drive
replay recorded no drill contact; it proves boarding/driving/exit only.

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
Both attempts return `RF_NOT_FOUND` with zero accepted cuts because no
campaign terrain owner is open; this is a controlled failure, not successful
excavation. The harness asserts the first-contact face/source identity but
allows the cut status to change when the owner is implemented. It writes only
text and input data. PC and NXDK builds pass with the contact room/face
telemetry; the replay itself has only run on PC, so Xbox cut behavior remains
unverified. A first campaign owner should target UID 8123's actual contact
surface, preserve its 83 linked compiled room-8 faces, then prove collision,
render publication, save/reload and stock-memory behavior.

The shared C asset decoder now admits this exact L1S2 UID 8123/room-8
source. It validates the 389-brush section with bounded successor-header
recovery for opaque editor sidecars, then retains 48 authored faces and 83
compiled windows; a focused PC check reports 26,652 resident bytes and a
959,885-byte decode peak. The existing ctf06 decoder check also passes.
This is source ownership only. `rf_geomod_terrain_open` still returns
`RF_RANGE` because the shared cutter has a 32-source-face limit, including
fixed plane/support arrays. UID 8123 has 48 authored faces but only 14
geometrically distinct planes in an offline rounded-plane census. The next
core step is to qualify a lossless coplanar-source representation or expand
the source-face limit consistently through cutter provenance and publication;
simply raising the admission check would overrun fixed arrays. Scene identity
capture, live publication and native memory admission are also still gated.

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
`render-20260926-202555/report.json`. Campaign drill contact and native
post-load driving remain to be checked.

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
