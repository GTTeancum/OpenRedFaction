# Campaign vehicle saves: first pass

Ordinary same-level saves carry one supported authored vehicle, parked or
occupied. The world checkpoint uses the existing Driller/APC/Jeep vehicle
record, binds the host by its authored UID, and validates the saved rigid pose
against the unchanged static world, staged movers and props, player, and
living NPCs. A seated player's overlap with that host is intentional; the
loader stages both poses and restores possession after player publication.

The L1S3 APC can be driven, fired, exited, saved, and reloaded. The PC reload
restored primary ammo at 996 and secondary ammo at 15. A no-image stock-64-MiB
XEMU save produced a 39,536-byte checkpoint whose every section matched the PC
checkpoint, including the vehicle (`artifacts/xemu/render-20260926-192156`).
The separate no-image XEMU reload restored that checkpoint, ran 120 frames,
and matched PC vehicle state; 4,816 physical pages (18.81 MiB) remained
available (`artifacts/xemu/render-20260926-192612`). A stationary L12S1 Jeep
save/reload also passed on PC. These are implementation checks, not claims of
original-game save format or complete vehicle behavior.

Vehicle idle audio now claims an ambient slot after the level's initial ambient
reset and schedule; the earlier order could lose the slot on a quick-load and
abort the first resumed frame. The Jeep's campaign seat, moving route cursor,
multiple vehicle owners, and broader continuation remain open.

The authored L1S2 Driller UID8122 now saves while seated and moving. Its
section's Remove_Object event has already removed trigger UID10072 from the
object registry; the ordinary trigger record distinguishes that absence from
a spent trigger that remains registered, then removes the fresh trigger on
reload. A stationary miner UID9845 turns in place by a 92 mm maximum collision
sphere-center shift. The unchanged-world restore accepts that bounded
static-surface contact only at its exact authored position, rechecks every
actual sphere center, and retains strict mover/prop overlap rules. The PC
save/load/resave succeeds. The image-free stock 64 MiB Xbox run writes a
54,124-byte checkpoint with all 16 sections byte-equal to PC
(`artifacts/xemu/render-20260926-202409/report.json`); a fresh load retains
the driver and matches PC vehicle state after 64 frames with 4,790 free
physical pages (`render-20260926-202555/report.json`). A PC continuation
drives after loading. Native post-load driving, other removed-trigger layouts
and wider seated save points remain unverified.

The first live L1S2 cut is also retained by ordinary saves. RFDS2 occupies the
world destruction section for this exact UID8123/room-8 terrain owner; the
fresh loader rebuilds edited room collision before placing the seated player,
NPCs and Driller. A no-image 330-frame PC/Xbox replay saved after the cut at
frame 270, and a separate 60-frame load matched the 171-face one-cut terrain
publication and vehicle state on both platforms. The Xbox runs finished with
3,752 and 3,720 free physical pages respectively. See
`artifacts/xemu/render-20260927-081157/report.json` and
`render-20260927-081415/report.json`. Continued drilling and other authored
terrain owners remain open.

The L12S1 Jeep now saves while the driver is seated and its authored
`Follow_Waypoints` event UID 9692 is active. The optional 32-byte `RFVR`
vehicle trailer stores the event UID, route mode, direction and waypoint
cursor. On load, the event UID rebinds to the current authored 34-node path
and live Jeep owner; no runtime handle or borrowed path pointer is persisted.
The old 128/160-byte vehicle records still load with no active route. A fresh
PC load keeps node 1 active and continues driving for 64 frames. Relative to
an uninterrupted 284-frame PC run, the resulting Jeep position differs by
about 0.11 m, so exact rigid-motion continuation remains open.

The no-image stock 64 MiB XEMU save wrote 35,756 bytes and all 16 components
matched PC byte-for-byte (`artifacts/xemu/render-20260926-204039/report.json`).
A separate XEMU load ran 64 frames with the driver seated and `VEHICLE_ROUTE`
matching PC at active node 1; 3,033 physical pages (11.85 MiB) were free
(`artifacts/xemu/render-20260926-204254/report.json`). Later route nodes,
and more campaign vehicle owners remain open. A separate
stock-64-MiB L12S1 Xbox run already verified ordinary boarding and driver exit
without a reload: `render-20260926-183240/report.json` records one entry,
one exit and no active seat after 180 frames.

A focused Xbox-only optical restore of that byte-equal 35,756-byte Jeep world
checkpoint then sent one process-local Use edge at frame 20. The 120-frame run
recorded one exit, no active vehicle seat, no runtime error and 3,024 free
physical pages (11.81 MiB) on stock 64 MiB
(`artifacts/xemu/native-world-20260929-225219/report.json`). The source RFSG
fixture's payload is byte-identical to the native `xbox-world.rfwc` emitted by
`render-20260926-204039`; the harness neither ran the PC executable nor
captured images. This verifies a driver exit after ordinary checkpoint restore,
not its visual presentation, gunner exit or exact route-motion continuation.
