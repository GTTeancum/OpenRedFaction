# Sight-triggered authored Alarm runners

Status: source-written against 428ab7500b1bd278a85bd57b5b6fe9feb1468df6,
externally staged for independent source review and parent integration.
Uncompiled and runtime-unverified. No worker builds, tests, fixtures, runtime,
campaign routes, images, or protected saved-HDD access were performed.

## Concrete missing consumer

The existing Alarm46 runner already owns travel, event-facing, finite action44
completion, linked event activation, cancellation and ordinary saved continuation.
Its sole onset consumer was surviving player damage. Ordinary sight acquisition
instead entered combat directly, and unarmed actors were skipped at weapon
selection. An undamaged hostile actor could not use its real authored alarm
merely because it saw the player.

The new consumer runs after existing mode, disguise, target-liveness and opposed
acquisition handling, before primary weapon selection. It admits only an
unclaimed, living, registered Waiting/default actor from the runner's existing
humanoid/no-seat/no-no-flee authored profile. The current runtime affiliation
must be hostile0. A prior scripted or opposed target, alert, patrol, movement,
look, one-shot or animation owner remains authoritative.

Sight uses the existing staggered one-in-30-frame sample,20-unit range and
120-degree cone through campaign_enemy_opposed_cone, plus the same world/mover
cover query as ordinary player acquisition. The current on-foot player/body,
stream, registration and generation are authenticated before the query and
sealed before runner entry. This adds no general perception, peer-target search,
new range/FOV policy, explicit enemy-list handling or target-history bank.

On accepted sight, the shared runner entry takes an explicit SIGHT reason.
The actual damage caller retains its separate DAMAGE wrapper; no damage event,
health request, hit reaction, sound or RNG draw is fabricated. Damage still
rejects every active player disguise. Sight follows existing fresh acquisition:
an active uncompromised disguise blocks it; a compromised disguise can be seen.
No disguise state is changed by merely starting an alarm run.

## Original executable evidence

Read-only Installed_Game/RF.exe SHA256:
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.
All addresses below are original image virtual addresses.

- 0x403a80 services the ordinary acquisition timer; 0x403acb calls target
  selection0x401de0. When an actual target is returned,0x403b90 calls0x409050
  to assign it and0x403b96 calls0x408ac0 for action selection.
- 0x401de0 checks actual candidates, opposition, visibility and FOV before
  returning a target. This reconstruction preserves the port's already-used
  practical sight policy rather than replacing it with the full original query.
- 0x408ae2 rejects Goto suppression;0x408af3 checks the actor;0x408b26 excludes
  action17. Crucially,0x408b34 calls0x4087a0 and exits when it succeeds, before
  0x408b4b checks weapon presence through0x408dc0. Alarm onset is therefore
  not a damage-only action and is not restricted to armed actors.
- 0x4087a0..0x4088ef verifies positive movement speed, global alarm state,
  actual authored Alarm46 UID, competing runner, route and stance before
  selecting action16/state1 and clearing the action44 latch. The already
  implemented shared runner supplies this bounded behavior.

Original broader action and no-flee behavior is not silently added here. The
existing authored Waiting/no-seat/no-no-flee boundary is deliberately retained.

## Actual authored demand

Read-only entity/event records from all94 installed RFL files contain54 Alarm
records and69 actors with an authored Alarm UID. Thirty-three match the current
static runner family;12 also have hostile0 affiliation:10 guards and2 unarmed
civilians across six levels. These are source candidates, not runtime admission
or route-coverage claims. Current script ownership, selected resources, body
state, visibility and navigation may narrow them.

- L6S1: guard2 UID4553 and guard1 UID4554 -> Alarm5021; guard2 UID4560 ->5123;
  guard1 UID5131 ->5143
- L6S2: guard2 UID6510 ->6509
- L6S3: unarmed admin_male2 UID3828 ->7146
- L8S1: unarmed nurse1 UID5956 ->6449; guard1 UIDs6270/6516 ->6463
- L8S2: guard2 UID6519 ->6520
- L8S3: guard1 UID6602 ->6597 and UID10383 ->10381

Their authored class hit_alarm clips resolve to installed compiled files:
ult2_hit_alarm.rfa5972 bytes, ult3_hit_alarm.rfa6156 bytes,
tech01_hit_alarm.rfa10772 bytes and adfm_hit_alarm.rfa7996 bytes. Actual selected
mapping admission remains the existing runtime authority; no file is guessed
or loaded outside that owner. This inventory is not an extra resident bank.

## Ownership, resources and saves

Only the onset path changes. Shared entry retains the existing duplicate-alarm
check, actual route acceptance, phase publication and exclusive command owner.
A refused route leaves its prior route/target intact and ordinary behavior may
continue. Resource and navigation errors propagate. Once accepted, the enemy
iteration stops before primary/secondary combat can claim that actor.

Accepted command, death, hide, seat and action replacement continue through the
existing cancellation chain. Natural action44 cursor completion alone permits
alarm activation; the actor receipt retires before linked callbacks. Existing
fault, active-global-alarm and interrupted-animation policies are unchanged.

No actor fields, save words, versions, target bank, timer, actor pool, animation
slots or resource caps are added. RFNC31's existing16-byte present alarm receipt
(event UID, phase, started) describes the entire post-onset state, independently
of whether sight or damage initiated it. RFNC32/RFCH10 coexistence is unchanged.
Same-level composed restore keeps exact authored/event/route/body/playback joins,
duplicate refusal, current/candidate on-foot checks, resource sealing and final
native-storage-close publication. Active runner section handoff and standalone
component restore remain unsupported as before.

The four future motions still use the runner's existing demand admission,
1280-KiB motion cache,16 active slots, per-owner request allowance and56-request
publication seal. Per-call sight locals are bounded; no new heap allocation or
resident resource bank is introduced. Existing resource loads can still be
refused at their unchanged caps. Exact target stack use and stock64 behavior
remain unmeasured until parent compilation and tester use.

## Boundaries

Only fresh local-player sight is added. NPC-origin threats, alert/hearing-origin
alarm takeover, neutral/friendly actors, patrol-mode alarm takeover, no-flee
expansion, original perception fidelity and optional action44 audio remain
outside the slice. Existing ordinary armed combat and opposed targeting retain
their earlier behavior if the new alarm request is unsupported or refused.
