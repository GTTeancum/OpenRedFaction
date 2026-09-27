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
