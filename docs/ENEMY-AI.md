# First-pass enemy awareness and combat

The shared scene now checks unalerted, visible, alive armed actors every30
simulation frames, staggered by actor index. Authored affiliation0 can acquire
the player within20 world units and a120-degree forward cone when the retained
static/moving geometry ray is clear. Acquisition waits30 frames before firing;
alerted actors then attack every60 frames within the supported weapon
range (pistol20, rifle30, Riot Stick2.6), with a fresh obstruction check.
Unsupported weapons retain the provisional40-unit attack limit. Damaging an armed actor also provokes retaliation regardless of affiliation.

These distances, cone and timing are practical port policy, not reconstructed
original AI. The affiliation values are corroborated by Dash Faction's
[ObjFriendliness declaration](https://github.com/rafalh/dashfaction/blob/master/game_patch/rf/object.h):
0 unfriendly,1 neutral,2 friendly,3 outcast. No community implementation code
was copied. Original loader field evidence is in entity-loader-fields-verification;
Set_Friendliness original action evidence is in door-event-actions-verification.
Runtime Set_Friendliness now updates this awareness owner as described below.

The existing two words per NPC retain alert/timing; global awareness counters
add32 bytes. There is no per-frame allocation for perception. No patrol, pursuit,
squad behavior, hearing, disguise, shooting animation or weapon-specific AI is
implemented here. Alerts persist until death/respawn; original alert decay is open.

`python tools/replay_enemy_awareness.py` checks240 frames without player fire:
staging near hostile8456 acquires two guards and receives8 hits (health61.6);
staging near neutral miner8431 acquires none and retains100 health. Nonhostile
and range rejections are observed. Facing/obstruction rejection branches are
not separately demonstrated by these routes. Neutral guard8326 has another
hostile nearby; its encounter must not be treated as an isolated neutrality test.

`tools/replay_enemy_combat.py` covers provocation, killing one of two attackers,
and fatal damage. `tools/replay_player_life.py` holds Use before death and checks
fresh-press recovery, restored vitals/ammo and real movement after respawn.

Stock64MiB XEMU240-frame PASS: awareness and combat state match PC, with
2 acquired guards,8 hits,health61.6 and matching final HUD samples.7115
available pages at completion (not peak memory):
artifacts/xemu/replay-20260914-010332/report.json. Both builds and26 CTests pass.

## Scripted allegiance

Set_Friendliness (type30, original action4bc280, off no-op4b9f80) now
dispatches resolved registered links through the scene service, including
delayed event ticks. The value comes from authored words[0]. NPC and player
damage affiliations are updated; the practical NPC combat alert and due time
are cleared so the new affiliation takes effect on subsequent awareness ticks.
Non-entity objects remain unsupported by the scene service. This does not
claim the original object+560 targeting side effects or all original AI state.

The npc_motion_residency test exercises the actual scene callback through
registered runtime events: absent backend, stale generation, duplicate links,
immediate friendly change clearing an active alert, and delayed hostile change.
Both PC/NXDK builds and26 CTests pass. Eight bytes of borrowed callback/context
are added to the32-bit trigger owner; no event-time allocation. End-to-end
authored mission sequences in XEMU remain to be verified.

## Authored attack range (2026-09-14)

The primary-weapon reader now accepts the optional `$AI attack range:` pair.
It retains the first value for this single-player implementation and validates
both finite positive values. Missing range stays zero so callers can retain
their explicit fallback. Duplicate, truncated and invalid pairs preserve output.
Installed pistol/rifle/Riot Stick pairs are respectively20/20,30/30 and2.6/2.6;
all supported pairs agree. The table comment describes this as the range the
AI attempts to stay within while attacking. No claim is made that the original
uses the same hard firing cutoff or pursuit hysteresis as this first pass.

Supported firearms pursue outside their range and stop at80 percent of range.
Riot Stick keeps its2.2-unit stopping threshold within the authored2.6 reach.
The same definition supplies damage kind. Unsupported classes retain the
existing20/16 pursuit thresholds and40-unit firing fallback.

The weapon-resource test verifies installed values, distinct paired values,
missing/default fields and malformed/duplicate preservation. PC full-spawn
3,000-frame rescue/cell-exit passes. PC and NXDK builds pass.

`tools/replay_cover_combat.py` extends the generated3,000-frame spawn replay
to3,600 frames: advance, aim, nine semiautomatic shots, retreat and reload.
Four shots hit and kill guard8490; the player returns to the doorway at
(24.540,-4.118,10.210), reloads to16 rounds and survives with damage taken.
A standing exploratory variant dies at frame3587. This is one fought encounter,
not a cleared corridor or completed section. Generate the prefix first with
`python tools/replay_area2_spawn.py`, then run the combat script.

Stock64MiB XEMU `render-20260914-205728` completes3,600 frames and all28
selected PC/native comparisons. COMBAT is `[9,4,1,42664586,...]`: nine shots,
four hits, one guard kill. Ammunition matches16 loaded,116 reserve and one
reload. Player health matches47.19998. Free memory is4,304 pages (16.8125MiB).
The harness closes its own process; no RF emulator remains. Reproduce with:

```
python tools/replay_area2_spawn.py
python tools/replay_cover_combat.py
python tools/xemu_render_check.py --spawn --level L2S2a.rfl --input artifacts/cover-combat-replay/input.bin --seconds 540
```

This proves the connected spawn/rescue/guard-kill/retreat/reload sequence. It
does not establish complete encounter balance or a cleared path to the exit.
