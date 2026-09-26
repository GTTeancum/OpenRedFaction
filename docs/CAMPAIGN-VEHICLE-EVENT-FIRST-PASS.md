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

This is one authored Jeep owner, not general campaign vehicle creation. The
L12S1 event chain still needs a live scripted-route check, and other campaign
vehicle classes and multiple instances need ownership. The vehicle exit sweep
does not yet account for nearby NPC bodies. L7S3's authored
`Never_Leave_Vehicle` has no links, so no implicit current-vehicle lock is
invented. Pulse state and active exit locks are not yet represented in ordinary
world saves. No visual image capture was used for this work.
