# Scripted Shoot_Once, first pass

Xbox is the runtime target for this implementation. The PC build remains a shared-code compile check; no PC gameplay run was used.

The verified PC executable (`SHA-256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`) constructs event type 9 at `0x4be550` with vtable `0x58997c`. Its ON action at `0x4bad80` visits linked actors, calls the AI-target setter `0x409050`, then calls primary fire at `0x425830` when the authored mode word at event offset `0x2b8` is zero or secondary fire at `0x426ca0` when it is one. Its OFF action at `0x4b9f80` has no type-9 effect. The setter argument is verified as `-1`; its eligibility and the bounded ordinary-primary adapter are described below. It does not reset motion or the AI action. This is binary-derived behavior, not a screenshot comparison.

The reconstructed event dispatcher now passes linked actor handles and the authored fire mode into the scene. For primary mode, the scene queues one shot along the actor's current facing and routes it through ordinary NPC weapon selection, finite ammunition, world cover, hit resolution and firing presentation. Pending requests have bounded lifetimes and now persist through RFNC10, as described below. The stock-64-MiB XEMU fixture stages L20S1's authored `UnHide` UID 12475, then `Shoot_Once` UID 12546, linked to mercenary UID 12544. It records one queued and one fired shot, no unsupported request and no pending shot after 110 frames, with 5,065 free physical pages (about 19.8 MiB). The fixture is `tools/xemu_shoot_once.py`; it restores the disc's prior flags after the run. The passing run is in `artifacts/xemu/shoot-once-20260929-171412` (untracked output).

The actor was initially hidden in L20S1, so dispatching Shoot_Once without its reveal correctly left the request waiting. This fixture verifies the authored reveal and shot sequence rather than weakening the hidden-actor fire guard.

L7S4 has 20 immediate mode-1 Shoot_Once events linked to Tankbot UID 10696. Its `Tankbot Missile` secondary declaration in `weapons.tbl` has speed 15, lifetime 6 seconds, collision radius 0.15, damage 25, damage radius 3, crater radius 5, and no magazine. The weapon parser now accepts secondary explosives identified by explosive damage type without a primary `$Weapon Type` field. Resource demand includes the actor's secondary missile without granting a player slot. The existing NPC missile pool now carries each shot's authored flight and blast definition, consumes the correct magazine or reserve source, sweeps world/actors/props, applies impact and radial damage, and requests terrain destruction. The Xbox-only `--tankbot` fixture fires authored event UID 11132; at 110 frames it records exactly one queued/fired request and one missile launch/impact, with 4,223 free pages (about 16.5 MiB) on stock 64 MiB. Passing run: `artifacts/xemu/shoot-once-tankbot-20260929-172759` (untracked output).

Open: the Tankbot currently launches from its eye and shares the Rocket Launcher impact recipe and sound; secondary-hand muzzle placement, `big_charge_explode` presentation, and visual inspection remain. L4S2's six-second delayed primary event, secondary/primary mixed queues and pending primary/secondary shot saves need live coverage. The AI-target setter argument and eligibility are source-verified below; its bounded ordinary-primary adapter, exact animation timing and shot direction relative to the original remain runtime-unverified.


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


## Ordinary Shoot_Once actor-target release

Source-written on 2026-10-10 against
`4ec81b0b482d8fb684c3d10b00ac9de097948dba`, independently reviewed and parent-integrated. Compilation awaits the 12:00 Xbox batch; action/save runtime remains unverified. This replaces neither the blocked broader candidate nor any
existing request, weapon, reload or checkpoint format.

### Original target setter evidence

This reuses the bounded original-executable inspection retained with the
blocked `scripted-target-release-next` source notes, for the SHA-256 above;
no new binary or campaign scan was performed for this slice.

- `4badbc..4badbf` calls `409050(actor+2a0,-1)` before the original
  Shoot_Once fire dispatch. Setter refusal does not veto subsequent firing.
- `409050` returns immediately when the retained target already equals the
  supplied handle. An already-null target does not refresh its target timer.
- Otherwise, `4270a0(actor)` must succeed OR AI action `+280` must equal 15.
  Action 15 overrides that eligibility check. `4270a0` rejects missing actors
  and `48aaf0` player ownership, then accepts `42a130(actor,-1)` or class
  `+724` bit `0x10`.
- `42a1b6..42a1c5` reads actor `+810` bit `0x10000` for that `-1` query.
  `48aaf0` recognizes object `+7c` bit 8 or a registered player's actor whose
  linked handle `+200` equals the full owner handle. The port checks the
  existing single-player registration, registry and entity lookup exactly.
- AI `+530` bit `0x80` refuses the null-target assignment even under action
  15. This is `ai_mode.flags_530`, not either actor flags word.
- A successful original clear writes target `-1` and sets target timer
  `+294` with zero delay. Cached target position and last-seen state survive;
  motion, AI action and authored destinations are not reset.

### Narrow live adapter

Only an accepted mode-0 Shoot_Once requests release. The call is after all
existing registration, weapon/resource, living-owner, aim and FIFO-capacity
checks, after the complete request is prepared and before count publication.
The helper cannot fail the shot: every qualification refusal simply returns,
and the already-admitted request is still published exactly once.

The actual currently selected weapon ID must resolve through the existing
normal or extra selector to a nonmelee, non-reserve-fed finite magazine with
matching primary/supply definitions. This admits only `12mm handgun`,
`Assault Rifle`, `Shotgun`, `Sniper Rifle`, `rail_gun`, `Machine Pistol`,
`heavy_machine_gun`, `scope_assault_rifle` and `Undercover 12mm handgun`.
It does not use a class weapon default or approximate player slot. Mode 1
secondary and mode 2 Fire_Weapon_No_Anim never call the helper. Melee, Rocket,
Grenade, Machine Pistol Special, Vauss, Tankbot and all other special/creature
primaries keep their previous target behavior. There is no new shot admission
condition, inventory mutation or fallback policy.

The owner must be the exact full registered actor: its retained registration
points to its view, and the registry/entity lookups resolve the same full
handle to those exact objects. The helper then retains the original
`4270a0 OR action15` eligibility and independent flags530 bit80 refusal.
Player object-bit and linked-player exclusions belong to the 4270a0 branch;
action15 overrides them as in the inspected setter.

Port actor ownership includes explicit Attack mode1, acquired/retaliatory
mode2, or implicit player `combat_alert`, including an alert with target zero.
No retained target/order/alert is an unchanged-null no-op. Independent mode3
Shoot_At is always a no-op, preserving its point order and cadence.
For eligible actor ownership, the helper clears target, scripted-combat mode,
alert, burst remainder, fire deadline and navigation deadline. Existing
`campaign_pursuit_stop` retires only follow2 pursuit and its route. Authored
Goto, Goto_Player, Follow_Waypoints, Look_At, animation, AI action and stored
pose/physics remain under their existing owners. The ordinary follow2 stop
consumer may subsequently stop locomotion; no general motion reset is added.

Alert/cadence/pursuit clearing is an explicit adaptation to this port's
coupled combat ownership, not original byte parity. The port does not model
the separate original null-target timer, cached target-position/last-seen
lifecycle or exact reacquisition timing here. Existing sight, hearing,
retaliation and later Attack can establish fresh ownership without an
invented suppression interval. The helper is never called by shot pop,
expiry, firing completion or checkpoint restore, so an older request cannot
release a later Attack when it drains or expires.

### Reload, presentation and save boundaries

There is no release veto for an existing reload deadline, loaded-round/FIFO
budget, pain, visibility, model advancement or current checkpoint saveability.
The compiled target-independent completion prerequisite services an already
started same-weapon finite reload before owner combat admission and FIFO
expiry. It retains its existing living/visibility/selection eligibility and
outer simulation gates; release does not start, cancel, erase, refund,
complete or restart a reload. Ammo, reserve and `combat_reload_due/weapon`
are untouched. A late request, including later mode2, may still start a reload
which remains independently owned after its FIFO expires.

All existing save-only guards remain strict. A targetless pending reload
still rejects checkpoint capture until its existing completion service
consumes the obligation. This finite transient is not a release predicate.
Once there is no ammunition continuation, the integrated exact per-slot
residual action2/action39 profile for these nine guns can preserve retained
fire/reload clips, composing with independently saved pain/burn clips under
their existing checks. It does not admit arbitrary extra nonloops or delayed
special attacks; all special/secondary/flight/callback save vetoes survive.
No clip is stopped, force-advanced, restarted or granted an invented callback.

Source inspection and text-diff preparation only: no build, syntax check,
test, fixture, gameplay, route, campaign scan, worktree, image, active-repo
edit, original-input mutation or cleanup was performed for this candidate.
The earlier checks documented above do not cover this new release behavior.
