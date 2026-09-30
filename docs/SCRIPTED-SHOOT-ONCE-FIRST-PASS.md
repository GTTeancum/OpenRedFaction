# Scripted Shoot_Once, first pass

Xbox is the runtime target for this implementation. The PC build remains a shared-code compile check; no PC gameplay run was used.

The verified PC executable (`SHA-256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`) constructs event type 9 at `0x4be550` with vtable `0x58997c`. Its ON action at `0x4bad80` visits linked actors, calls the AI-target setter `0x409050`, then calls primary fire at `0x425830` when the authored mode word at event offset `0x2b8` is zero or secondary fire at `0x426ca0` when it is one. Its OFF action at `0x4b9f80` has no type-9 effect. The exact setter argument and its effect in this event still need inspection; no motion-reset claim is made. This is binary-derived behavior, not a screenshot comparison.

The reconstructed event dispatcher now passes linked actor handles and the authored fire mode into the scene. For primary mode, the scene queues one shot along the actor's current facing and routes it through ordinary NPC weapon selection, finite ammunition, world cover, hit resolution and firing presentation. The pending request has a bounded lifetime and prevents an ordinary NPC checkpoint save until it has fired or expired. The stock-64-MiB XEMU fixture stages L20S1's authored `UnHide` UID 12475, then `Shoot_Once` UID 12546, linked to mercenary UID 12544. It records one queued and one fired shot, no unsupported request and no pending shot after 110 frames, with 5,065 free physical pages (about 19.8 MiB). The fixture is `tools/xemu_shoot_once.py`; it restores the disc's prior flags after the run. The passing run is in `artifacts/xemu/shoot-once-20260929-171412` (untracked output).

The actor was initially hidden in L20S1, so dispatching Shoot_Once without its reveal correctly left the request waiting. This fixture verifies the authored reveal and shot sequence rather than weakening the hidden-actor fire guard.

L7S4 has 20 immediate mode-1 Shoot_Once events linked to Tankbot UID 10696. Its `Tankbot Missile` secondary declaration in `weapons.tbl` has speed 15, lifetime 6 seconds, collision radius 0.15, damage 25, damage radius 3, crater radius 5, and no magazine. The weapon parser now accepts secondary explosives identified by explosive damage type without a primary `$Weapon Type` field. Resource demand includes the actor's secondary missile without granting a player slot. The existing NPC missile pool now carries each shot's authored flight and blast definition, consumes the correct magazine or reserve source, sweeps world/actors/props, applies impact and radial damage, and requests terrain destruction. The Xbox-only `--tankbot` fixture fires authored event UID 11132; at 110 frames it records exactly one queued/fired request and one missile launch/impact, with 4,223 free pages (about 16.5 MiB) on stock 64 MiB. Passing run: `artifacts/xemu/shoot-once-tankbot-20260929-172759` (untracked output).

Open: the Tankbot currently launches from its eye and shares the Rocket Launcher impact recipe and sound; secondary-hand muzzle placement, `big_charge_explode` presentation, and visual inspection remain. L4S2's six-second delayed primary event, secondary/primary mixed queues and save/reload while a request is pending need coverage or persistence work. The AI-target setter call, exact animation timing and shot direction relative to the original remain unverified.


## Independent queued requests

The previous live adapter retained one aim point, mode, event UID and expiry
for all pending shots. A later request overwrote those fields for earlier
requests. Each actor now owns a bounded FIFO of 16 complete requests; firing
or expiration removes only the first request. Primary, Tankbot secondary and
no-animation requests share this path. Pending-request saves remain rejected.
The fixed capacity adds 360 bytes per NPC, with no allocation during firing.

The stock-64-MiB Xbox run
`artifacts/xemu/single-fire-queue-20260930-100243/report.json` passed at
110 frames using `python tools/xemu_shoot_once.py --queue`. After authored
UnHide6825, the process-local fixture submitted three synthetic requests to
Parker6810 in one frame: no-animation, animated primary, no-animation. The
middle request faced the opposite direction. The ordinary combat path fired
all three in order with their original aim hashes, consumed three handgun
rounds and started exactly one firing motion. Both pending counters reached
zero. Disc flags were restored after the run.

This is a contained gameplay-state check, not visual/audio inspection or a
claim that the authored campaign places this exact mixed sequence. The
Tankbot secondary mixed-order path, queue saturation and active-request
persistence remain unverified.

The run ended with 5,195 physical pages free (20.29 MiB).
