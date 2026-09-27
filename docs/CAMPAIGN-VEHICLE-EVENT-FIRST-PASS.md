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

This is one selected owner per section, not general campaign vehicle
creation. L1S3 contains other APC instances, and other sections and vehicle
classes still need ownership. The remaining Jeep route nodes, scripted
interactions and NPC traffic need live coverage. The first-pass waypoint steering is a playable
port policy, not an exact original AI driver reconstruction. The vehicle exit
sweep does not yet account for nearby NPC bodies. L7S3's authored
`Never_Leave_Vehicle` has no links, so no implicit current-vehicle lock is
invented. Pulse state and active exit locks are not yet represented in ordinary
world saves, nor is an active vehicle route cursor. No visual image capture was
used for this work.
