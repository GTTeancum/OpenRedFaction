# Scripted Shoot_Once, first pass

Xbox is the runtime target for this implementation. The PC build remains a shared-code compile check; no PC gameplay run was used.

The verified PC executable (`SHA-256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`) constructs event type 9 at `0x4be550` with vtable `0x58997c`. Its ON action at `0x4bad80` visits linked actors, calls the AI-target setter `0x409050`, then calls primary fire at `0x425830` when the authored mode word at event offset `0x2b8` is zero or secondary fire at `0x426ca0` when it is one. Its OFF action at `0x4b9f80` has no type-9 effect. The exact setter argument and its effect in this event still need inspection; no motion-reset claim is made. This is binary-derived behavior, not a screenshot comparison.

The reconstructed event dispatcher now passes linked actor handles and the authored fire mode into the scene. For primary mode, the scene queues one shot along the actor's current facing and routes it through ordinary NPC weapon selection, finite ammunition, world cover, hit resolution and firing presentation. Pending requests have bounded lifetimes and now persist through RFNC10, as described below. The stock-64-MiB XEMU fixture stages L20S1's authored `UnHide` UID 12475, then `Shoot_Once` UID 12546, linked to mercenary UID 12544. It records one queued and one fired shot, no unsupported request and no pending shot after 110 frames, with 5,065 free physical pages (about 19.8 MiB). The fixture is `tools/xemu_shoot_once.py`; it restores the disc's prior flags after the run. The passing run is in `artifacts/xemu/shoot-once-20260929-171412` (untracked output).

The actor was initially hidden in L20S1, so dispatching Shoot_Once without its reveal correctly left the request waiting. This fixture verifies the authored reveal and shot sequence rather than weakening the hidden-actor fire guard.

L7S4 has 20 immediate mode-1 Shoot_Once events linked to Tankbot UID 10696. Its `Tankbot Missile` secondary declaration in `weapons.tbl` has speed 15, lifetime 6 seconds, collision radius 0.15, damage 25, damage radius 3, crater radius 5, and no magazine. The weapon parser now accepts secondary explosives identified by explosive damage type without a primary `$Weapon Type` field. Resource demand includes the actor's secondary missile without granting a player slot. The existing NPC missile pool now carries each shot's authored flight and blast definition, consumes the correct magazine or reserve source, sweeps world/actors/props, applies impact and radial damage, and requests terrain destruction. The Xbox-only `--tankbot` fixture fires authored event UID 11132; at 110 frames it records exactly one queued/fired request and one missile launch/impact, with 4,223 free pages (about 16.5 MiB) on stock 64 MiB. Passing run: `artifacts/xemu/shoot-once-tankbot-20260929-172759` (untracked output).

Open: the Tankbot currently launches from its eye and shares the Rocket Launcher impact recipe and sound; secondary-hand muzzle placement, `big_charge_explode` presentation, and visual inspection remain. L4S2's six-second delayed primary event, secondary/primary mixed queues and pending primary/secondary shot saves need live coverage. The AI-target setter call, exact animation timing and shot direction relative to the original remain unverified.


## Independent queued requests

The previous live adapter retained one aim point, mode, event UID and expiry
for all pending shots. A later request overwrote those fields for earlier
requests. Each actor now owns a bounded FIFO of 16 complete requests; firing
or expiration removes only the first request. Primary, Tankbot secondary and
no-animation requests share this path. Pending-request persistence is implemented by RFNC10, described below.
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
Tankbot secondary mixed-order path, queue saturation and wider pending-request
persistence remain unverified.

The run ended with 5,195 physical pages free (20.29 MiB).


## Pending-shot persistence

RFNC10 extends the NPC base row to 600 bytes. Offset588 stores the queue
count, offset592 the shared spread RNG, and offset596 the NPC shield-disable
state; up to16 compact24-byte shot
records follow movement/combat extensions and precede animation playback.
Each record retains the authored event UID, remaining frame lifetime, mode
and aim point. Older RFNC1-9 rows load with an empty queue. The codec checks
count, finite aim, mode and bounded, ordered lifetimes before publication.

Scene capture omits already-expired requests. Restore resolves each event
against the actor's authored links, requires its matching Shoot_Once or
Fire_Weapon_No_Anim mode, verifies secondary missile resources when needed,
and checks shared RNG consistency with saved active combat. The event codec
now admits types9 and79, whose ordinary scheduler and NPC queue state have
separate owners. Capek shield event76 also admits normal scheduling; its NPC
damage-owner bit now saves/restores explicitly instead of being lost. Older
NPC records preserve the constructed shield state. Shield-hit behavior and
player-targeted shield persistence remain separate follow-up work. Queue publication
rebases lifetimes to the load frame without dispatching the events again.
Synthetic requests without authored event identities are not saveable.

The compiled NXDK codec check exercises legacy basic-row migration, a mixed
pending-shot roundtrip and truncated-queue rejection. The stock-64-MiB Xbox run
`artifacts/xemu/single-fire-save-20260930-101602/report.json` passed using
`python tools/xemu_single_fire_save.py`. Authored Fire_Weapon_No_Anim8478
queued on hidden Parker6810 and remained unfired at the 50-frame save.
The30,332-byte ordinary world checkpoint retained its exact aim, mode2,
spread RNG1 and71 remaining frames. A fresh process restored that request
without firing its event again; authored UnHide6825 then allowed exactly one
ordinary shot with the saved aim hash and no firing-motion start. No duplicate
request was queued, and the pending counter returned to zero. Disc flags were
restored. The load ended with 5,002 physical pages free (19.54 MiB).

This validates one pending no-animation shot across ordinary save/reload.
Pending primary/Tankbot shots, mixed queues through live reload, shield toggles
followed by damage/save, and visual/audio presentation remain unverified.
