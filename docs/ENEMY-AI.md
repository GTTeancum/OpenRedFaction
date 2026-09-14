# First-pass enemy awareness and combat

The shared scene now checks unalerted, visible, alive armed actors every30
simulation frames, staggered by actor index. Authored affiliation0 can acquire
the player within20 world units and a120-degree forward cone when the retained
static/moving geometry ray is clear. Acquisition waits30 frames before firing;
alerted actors then attack every60 frames within40 units with a fresh obstruction
check. Damaging an armed actor also provokes retaliation regardless of affiliation.

These distances, cone and timing are practical port policy, not reconstructed
original AI. The affiliation values are corroborated by Dash Faction's
[ObjFriendliness declaration](https://github.com/rafalh/dashfaction/blob/master/game_patch/rf/object.h):
0 unfriendly,1 neutral,2 friendly,3 outcast. No community implementation code
was copied. Original loader field evidence is in entity-loader-fields-verification;
Set_Friendliness original action evidence is in door-event-actions-verification.
Runtime scripted allegiance changes remain to be connected to this awareness owner.

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
