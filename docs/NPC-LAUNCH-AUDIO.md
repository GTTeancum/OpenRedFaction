# Ordinary NPC primary Launch audio

Status (2026-10-09): source-written, with read-only source/installed-asset review.
Parent owns `scene.c` integration and the scheduled Xbox validation batch.
No build, tests, emulator run, gameplay fixtures, PC validation, captures,
cleanup or commits were performed by this worker. Playback remains unverified.

## Gap and original evidence

`campaign_enemy_fire_presentation` consumed only entity `action_sounds[2]`
outside the separate Assault Rifle burst adapter, with a hardcoded Glock
fallback when the action label was empty. Most human firearm actions have
empty animation Foley. This omitted actual weapon Launch sounds for accepted
shotgun, sniper, rail, rocket and scoped-assault shots, including supported
Undercover handgun shots.

Read-only disassembly of installed `Installed_Game/RF.exe` establishes:

- `42686e..426881`: load weapon descriptor `+0x160`, resolve through `434da0`,
  and retain the selected sample independently of animation action sounds.
- `426a6e..426a8b`: the noncontinuous path submits that sample through `48a9c0`.
- `48aa06..48aa21`: ordinary nonplayer actors take positional `5056a0`.
  The separate local-player branch uses flat `505560` at `48a9eb..48a9fc`.

Installed `tables.vpp/weapons.tbl` primary bindings, resolved by the existing
NPC weapon catalog names:

| Supported weapon | Launch group | Weapons line | Foley sample / line | Near / gain |
|---|---|---:|---|---|
| 12mm handgun | Glock Launch | 465 | handgun_fire01.wav / 1753 | 10 / 0.8 |
| Undercover 12mm handgun | Glock Launch | 556 | handgun_fire01.wav / 1753 | 10 / 0.8 |
| Shotgun | Shotgun Fire | 652 | Shotgun_01.wav / 1957 | 10 / 0.8 |
| Sniper Rifle | Sniper Launch | 737 | sniper_fire_01.wav / 1804 | 10 / 0.8 |
| Rocket Launcher | Rocket Launcher | 832 | rocket_launch_01.wav / 1830 | 12 / 0.8 |
| rail_gun | Rail Fire 1 | 1405 | Rail_Fire_01.wav / 2035 | 8 / 0.8 |
| scope_assault_rifle | Sniper 2 Launch | 1586 | sniper_fire_02.wav / 1807 | 10 / 0.8 |
| Riot Stick | Empty | 386 | No Launch | |
| Grenade | Empty | 1202 | No Launch | |

`Rocket Fire` is a different Foley group (`fp_rocket_fire.wav`, Foley1835–1836).
It must not replace the actual rocket Launch group. The player consumer's
existing choice does not establish the correct NPC binding.

Installed `bluebeard.bty` declares the six distinct samples above without
loop flags at lines5498–5503,15696–15701,15848–15853,15355–15360,
14864–14869 and15855–15860 respectively. Each relevant Foley group has one
sample; its ordinary chooser does not advance RNG for that single choice.
The existing registration retains gain and near-distance parameters; this
adapter does not duplicate either value in its playback request.

Installed `entity.tbl` has no `Assault Loop` or `Glock Launch` action label.
The rifle firing actions at844–848 and2770–2772 have explicitly empty Foley.
Nonempty primary firing labels belong to creature/melee actions, including
Cutter3802, Tankbot4075, Drone4133, Rock Snake4492/4495, Reeper4552,
Baby Reeper4614, Sea Creature4665, Big Snake4711/4714 and Mutant4772/4834.
They remain independent authored animation Foley and must not be replaced,
used as a fallback for Launch, or suppressed merely because a Launch exists.

## Consumer and boundaries

`src/diagnostic/scene_npc_launch_audio.inc` contains a bounded nine-name
installed mapping. It covers the existing ordinary NPC selections in
`scene_ai_weapon_selection.inc`, the accepted rocket/grenade branches and the
noncontinuous entries in `scene_ai_extra_weapons.inc`. It uses the current
supply catalog's actual weapon name rather than assuming an integer ID.
This is a documented installed subset, not a new generic weapons-table parser.
Name matching follows those exact installed spellings.

`scene_npc_launch_audio_shot(weapon, position)` resolves the retained Foley
group and sample, rejects any known looping sample, then calls the existing
bounded bank/playback service with `campaign_pain_audio_context.player=0`.
This produces positional NPC audio. The old `combat_sound` helper uses
`player=1`, so reusing it for this Launch would incorrectly request flat
player-style playback.

There is no retained voice, per-actor allocation, second sample cache, retry
timer, serialization, load hook or gameplay-state mutation. Seven nonempty
Launch mappings request one one-shot each accepted shot. The two explicitly
empty mappings stay silent. Unmapped weapons return without a Launch request;
in particular Assault Rifle primary remains with `scene_npc_rifle_audio.inc`
and Machine Pistol/HMG remain with the separate continuous owner. The fixed
scope cannot accidentally start their looping samples.

Return value is one for a nonempty Launch request even when sample admission
or playback fails, zero for authored silence or excluded weapons. This is a
presentation counter only. Missing groups/samples, invalid metadata or a full
voice budget never reject a shot and never trigger a substitute handgun sound.
The public12-word observation array reports calls, requests, starts, authored
silence, exclusions, failures, status, weapon, group, sample, voice and RNG.
The existing playback helpers use an additional private nine-word observation
array. Writable tracking is84bytes total, with no heap allocation by this
adapter; the shared bounded bank can load/evict PCM as it already does.

## Exact parent integration

1. Include `scene_npc_launch_audio.inc` beside `scene_npc_rifle_audio.inc`
   after `combat_sound` and the shared pain/Foley helpers are defined.
2. Call `scene_npc_launch_audio_reset()` beside
   `scene_npc_rifle_audio_reset()` during the existing startup counter reset.
   No forward declaration or teardown hook is needed for this stateless
   one-shot consumer when reset occurs after the include.
3. In `campaign_enemy_fire_presentation`, retain the existing accepted-shot
   call boundary and rifle burst call. Dispatch MP/HMG to the independent
   continuous owner as instructed by its worker. For other supported ordinary
   weapons, add the return from
   `scene_npc_launch_audio_shot(owner->view.weapons[0], owner->eye_position)`
   to `rf_scene_enemy_fire[3]`.
4. Delete the old `label="Glock Launch"` empty-action fallback completely.
   Failure of the new optional Launch request does not restore that fallback.
5. Independently retain the nonempty `owner->selection.action_sounds[2]`
   callback for every weapon, including rifle. It is no longer an `else`
   alternative to weapon Launch. Do not add a fabricated label. Keeping this
   callback before the existing `if(!animate)return RF_OK` preserves current
   `Fire_Weapon_No_Anim` presentation behavior without changing motion starts.
   Routing that callback positionally is a parent-owned adjacent correction;
   this worker does not change its existing behavior in `scene.c`.
6. Do not move the presentation call before accepted projectile creation or
   accepted shot/ammunition consumption. Existing gameplay rejection paths
   must remain silent. No new call belongs in aim, reload or cadence waiting.

No changes to fire timing, projectiles, ammunition, recoil, damage, target
selection, animation scheduling or save data are required. The mapping covers
ordinary primary shots only; suppressed/underwater Launch overrides, NPC
alternate modes, muzzle-tag acoustics and Tankbot secondary missile audio are
not reconstructed or claimed by this slice. Current eye-position routing is
the existing NPC presentation convention, not proven retail muzzle placement.
