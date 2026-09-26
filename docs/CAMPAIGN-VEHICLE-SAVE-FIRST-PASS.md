# Campaign parked-vehicle saves: first pass

Ordinary same-level saves now carry one supported authored vehicle while the
player is on foot. The world checkpoint uses the existing Driller/APC/Jeep
vehicle record, binds the host by its authored UID, and validates the saved
rigid pose against the unchanged static world, staged movers and props, player,
and living NPCs before publishing it. The player and world still restore from
their ordinary sections. A seated save is refused until possession and seat
state can be staged with the player as one transaction.

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
abort the first resumed frame. Seated campaign saves, Driller reload, the Jeep's
moving route cursor, multiple vehicle owners, and broader continuation remain
open.
