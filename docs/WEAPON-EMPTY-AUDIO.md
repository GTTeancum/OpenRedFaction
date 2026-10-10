# Authored handheld empty-fire audio

2026-10-09: the ordinary firearm consumer compiled in the parent-coordinated
19:00 Xbox batch. The subsequent Riot Stick consumer is source-written only,
awaiting the next parent batch. Natural empty-fire playback and audible output
remain unverified for both branches.

## Concrete missing consumer

The ordinary handheld firing path already distinguishes trigger event2 from an
accepted event1. With no loaded ammunition and no reserve, it increments the
denied-shot counters and returns without requesting any audio. Reload starts,
successful firing and NPC pain/death already have live sound consumers.

`scene_weapon_empty_audio.inc` supplies only that missing denied-shot consumer.
For ordinary firearms it reads the current weapon's authored `$Launch Fail`
Foley group, selects via the existing random Foley service, and uses the
existing player sound routing. Riot Stick's empty alternate fire uses its
explicit original flat sample2 branch instead. Both use the existing bounded
sample bank, idle eviction and generation-qualified voice allocation. No PCM
or voice pool is duplicated. Missing firearm declarations remain silent, and
a missing sample or exhausted audio budget cannot fail combat or spend ammunition.

## Original evidence

Read-only disassembly and installed archive inspection, not executable testing:

- RF.exe SHA256:
  `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
- `4c3d16` requests `$Launch Fail:` at `5a2ec8`; `4c3d40` calls Foley lookup
  `434cb0`; `4c3d48` stores the group at weapon descriptor `+170`. The absent
  branch at `4c3d50` stores `-1`.
- The empty-ammunition branch in primary firing begins at `425c73`. For a
  player-associated owner, `425c8c..425c97` arms its retry deadline from the
  selected fire interval. The ordinary branch at `425cbe` reads descriptor
  `+170`, calls random Foley selector `434da0` at `425ccc`, then player sound
  route `48a9c0` at `425ce4`, with gain1 and centered pan0.
- `4c8710` chooses primary `+d0` or alternate `+d4` fire-wait seconds, multiplies
  by1000 and adds0.5 before integer conversion (`589410` is1000.0,
  `5893c0` is0.5). The adapter uses that rounded millisecond interval only for
  its presentation retry deadline.
- `48a9c0` routes an ordinary local first-person owner through flat `505560`;
  the existing `rf_player_sound_route` and playback adapter already implement
  that route and authored sample gain.
- Installed `weapons.tbl` binds handgun, shotgun, assault rifle, sniper rifle,
  both Machine Pistol definitions, heavy machine gun and scoped assault rifle
  to `Glock Fail`; `rail_gun` binds `Rail Fail`. `foley.tbl` maps `Glock Fail`
  to `handgun_click01.wav`. The implementation resolves table data rather than
  hardcoding either group or waveform.

## Riot Stick continuation

The dry branch at `425c9c` calls `4c90d0`, which compares the weapon ID with
global `872468`. That global is initialized at `4c664f` from the lookup of
`"Riot Stick"` at `5a3350`. On a match, `425ca9..425cb4` calls
`505560(2,0,0,1.0)`, bypassing the Foley-group branch. It uses the same selected
fire-interval deadline installed at `425c97`; there is no separate500ms constant
in this branch.

Installed `weapons.tbl` supplies Riot Stick's `$Alt Fire Wait: 0.5` and no
`$Launch Fail`. Installed `sounds.tbl` row2 is `negative_Beep.wav`, with
authored volume0.80. Existing audio startup requires each global declaration's
registered ID to equal its table row, so sample2 is stable rather than a
position guessed within the later Foley bank.

The live port already routes a denied Riot Stick alternate request through
the same consumer as ordinary firearm event2; it previously found group`-1`
and stayed silent. The bounded continuation recognizes the resolved
`campaign_riot_id`, requests sample2 through `campaign_sound_start` with
volume1, flat routing and centered pan0, and retains the existing audio-only
deadline. The common sample start validates the ID, lazily reloads through
the budget-safe bank, applies the authored sample gain once, and allocates a
generation-qualified voice. There is no extra Foley selection or RNG draw,
no new retained state, and no additional scene hook. The group telemetry stays
`-1` because this branch does not select a Foley group; sample/voice/start/error
telemetry reports the actual direct request.

The existing caller excludes uncharged primary baton strikes from its empty
branch. It also handles reserve-backed reload first, so this change does not
beep during reload or manufacture a denied request. Missing metadata for other
weapons still does not receive a generic beep.

Secondary-request helper `426f40` separately writes owner`+4bc` with500ms and
calls sample2 only for a local player. Its caller `426ca0` reads owner`+2a8`,
the distinct secondary weapon ID used by vehicle requests; it is not evidence
for changing ordinary handheld alternate firing. The port's only current
`rf_weapon_secondary_request_admit` caller is vehicle AI with local-player
false. That separate request path and its gameplay backoff remain out of scope.

## Ownership and integration

`rf_weapon_launch_fail_groups_read/load` reuse the established table lexer,
stable weapon-name catalog and Foley name resolver. They build a separate
64-entry group array transactionally, reject duplicate fail tags and mismatched
weapon order, and leave the existing weapon-reset catalog ABI unchanged.
The load uses at most128KiB temporary table scratch, freed immediately. Missing,
empty and unknown group names produce`-1`, matching existing Foley lookup.

The parent owns these shared hooks:

1. Admit groups after the existing weapon-reset catalog during audio startup.
2. Include the new adapter after `combat_sound`, where existing Foley/player
   audio services and the combat audio RNG are available.
3. Call playback only for existing trigger event2 in the no-loaded/no-reserve
   return branch. Supply the current selected weapon ID, including the Machine
   Pistol's actual mode, and primary/alternate authored fire-wait seconds.
4. Clear its transient deadline on accepted weapon selection, ordinary player
   relocation/restore/teleport/vehicle placement, and the dead-player return.
5. Reset at scene startup and close before the existing audio owners teardown.

The current port emits automatic empty event2 repeatedly without advancing its
gameplay firing cooldown. An independent wrapped-game-clock AUDIO deadline
prevents one sound request per simulation frame. This does not change existing
input edges, burst cancellation, firing timing, reload initiation/completion,
inventory, damage, or animation. The audio deadline is installed before Foley
selection/playback and remains installed when audio is silent or fails. No
retail-equivalence claim is made for the port's separate gameplay retry policy.

The deadline is transient presentation state and is not serialized. Accepted
load/relocation clears it without replaying a dry shot; the next ordinary input
attempt owns any new feedback. Existing successful shots and reload audio are
untouched. No new source-level fixture, ammo grant, direct event invocation,
build, test, emulator run or cleanup was performed by the implementation helper.

`rf_scene_weapon_empty_audio[16]` exposes requests, admitted retries, starts,
silent cases, suppressed requests, failures, last weapon/group/sample/voice,
status, retry interval/deadline, audio RNG, ready state and retained owner bytes.
This is diagnostic observation only and does not drive gameplay.

## Remaining scope

The next parent batch should compile the Riot Stick continuation. A bounded
normal dry-fire check can then observe authored sample selection and retry
admission without inventing inventory. Broader weapon-specific empty handling,
the separate secondary-owner negative beep, exact gameplay retry semantics and
retail audio mixing remain separate work. No campaign-route traversal is required.

## Proportional runtime boundary

A source-only review on2026-10-09 found no short ordinary pickup/depletion case in the already bounded L3S1/L4S5 scenes. Earlier item-staged L4S5 checks replace spawn and are not eligible original-spawn evidence. L3S1 first-pass startup supplies16 loaded plus125 reserve, requiring141 real shots and8 reloads; a simple30-tick press cadence needs about5100 frames, with survival unestablished. Do not alter inventory or manufacture a route merely for coverage. Current evidence remains compilation/source review until a natural opportunity is available.
