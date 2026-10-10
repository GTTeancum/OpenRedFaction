# Positional NPC firing and reload action Foley

Status (2026-10-09): source-written and read-only reviewed against the installed
original executable and table metadata. Parent owns `scene.c` wiring and the
scheduled Xbox compilation/runtime batch. This worker performed no builds,
tests, emulator runs, PC validation, new fixtures, captures, cleanup or commits.
Playback and actual pan/attenuation remain unverified.

## Routing defect and original evidence

The existing accepted-fire presentation independently retains entity
`action_sounds[2]`, while `campaign_enemy_reload_presentation` consumes the
selected actor/weapon overlay's `action_sounds[39]`. Both previously passed
their label to `combat_sound`, which sets `campaign_pain_audio_context.player=1`
and takes flat local-player routing. That is separate from weapon Launch audio.

Read-only disassembly of installed `RF.exe` establishes the generic action
helper's actual sound path:

- `428c90..428cb2` checks the action index and mapped motion.
- `428cb4..428cc6` starts the mapped motion through `5033b0`.
- `428ccb..428cd4` checks the caller's sound-enabled byte.
- `428cd6..428ce8` loads the action's independent Foley group, calls `434da0`,
  and skips an absent selected sample (`-1`).
- `428cea..428cfb` submits `5056a0(sample, actor+0x3c, 1, 0x173c378, 0)`.
  This is positional playback at the actor body position. The returned voice
  is discarded; this helper retains no playback owner or stop deadline.

The existing `rf_scene_npc_death_sound` already implements this registered
action-sound callback. Its historical name does not imply a death mutation:
it qualifies the actor, resolves/selects its requested Foley label, reuses
bounded bank admission and calls the positional shared service with
`player=0` at `owner->published`. It changes audio counters/RNG only.

The new wrapper reuses that callback rather than adding another selector,
PCM loader, player-route implementation or voice owner. It passes the same
`combat_sound_random` used by the two former call sites; death/pain callers
and their own supplied RNG remain unchanged.

## Installed labels do not require a loop owner

A read-only scan of every exact `fire_stand` and `reload` declaration in
installed `tables.vpp/entity.tbl`, followed through the declared Foley groups
and `bluebeard.bty`, found these 13 nonempty groups and 15 samples. Alternate
fire actions and differently named actions such as `fire_stand2` are not
included. All 15 samples have no authored loop flag.

| Action Foley | Representative entity lines | Foley line | Sample | Bluebeard line |
|---|---|---:|---|---:|
| Ultor Reload | 352, 745, 843, 3043 | 1051 | ultor_reload.wav | 17652 |
| Ultor Reload2 | 2357, 2384, 2465 | 1054 | ult3_idle_reload.wav | 17628 |
| Cutter Attack | 3802 | 1437 | cutter_attack.wav | 2672 |
| tbot attack melee | 4075 | 1572 | Tankbot_attack_melee.wav | 16399 |
| drone attack | 4133 | 1424 | Drone_Attack_01.wav | 3180 |
| RSnake Spit | 4492, 4711 | 1499 | RSnake_Spit.wav | 15449 |
| RSnake Bite | 4495 | 1496 | RSnake_Bite.wav | 15422 |
| Reeper Attack | 4552 | 1220 | reeper_attack_01.wav; reeper_attack_02.wav | 14947, 14953 |
| Breeper Attack | 4614 | 1247 | Breeper_attack_01.wav; Breeper_attack_02.wav | 2118, 2124 |
| Sonar Attack | 4665 | 1277 | Sonar_Wave_02.wav | 15897 |
| BSnake Bite | 4714 | 1490 | BSnake_Bite.wav | 2215 |
| mutant swing | 4772 | 1338 | Mutant_swing.wav | 14324 |
| mutant2 attack | 4834 | 1346 | Mutant2_attack.wav | 14180 |

Human firearm primary action Foley is generally empty and stays silent.
The wrapper never substitutes `Glock Launch`, a first-person reload group,
or any other invented label. `Ultor Reload` and `Ultor Reload2` remain distinct.
The existing registration owns each sample's near-distance and gain; source
unity gain is applied through the same callback without duplicating those
authored values. Multi-sample Reeper/Breeper groups draw RNG once in the
existing chooser; singleton groups draw none.

## Bounded adapter and failure behavior

`src/diagnostic/scene_npc_action_foley.inc` accepts only existing action IDs
2 and 39 and obtains the label directly from the selected NPC overlay. It
does not contain a second hardcoded label mapping. Invalid slots/actions,
missing groups, malformed sample spans, failed bank admission and full voice
budgets remain optional presentation failures.

Before calling the existing callback it reads the whole selected group,
bounded by the retained Foley limit of 4096 sample IDs. A group containing
any known looping sample is rejected before selection or playback. This
preflight draws no RNG and does not resolve a second random choice. The
callback repeats the read-only group-name lookup, then chooses once. A future
changed asset cannot leak an endless discarded voice or acquire an invented
cutoff. Missing metadata uses the shared service's existing nonloop default;
the actual installed samples listed above all have explicit metadata records.
Absent sample IDs (`-1`) are left for the normal chooser/absence path.

The return value means a nonempty authored request, not playback success.
The fire presentation can retain its sound-request counter without letting
an audio error undo a shot. Reload explicitly discards that return value and
continues exactly the existing motion path. The new eight-word observation
array reports calls, requests, starts, silence, failures, last status, slot
and action. The existing nine-word `rf_scene_npc_action_audio` array supplies
the actual selection, loading, sample, RNG and playback detail. New retained
storage is 32 bytes; there is no allocation, per-NPC owner, timer, save tail
or cleanup hook in this adapter.

## Integration

Parent-owned `scene.c` wiring:

1. Include `scene_npc_action_foley.inc` after `combat_sound_random` and the
   existing action callback are defined, beside the other NPC audio includes.
2. Call `scene_npc_action_foley_reset()` in the ordinary startup audio-counter
   reset beside the NPC Launch/rifle reset calls.
3. In `campaign_enemy_fire_presentation`, remove the now-unused `label`
   local and replace only the nonempty-label `combat_sound` block with
   `rf_scene_enemy_fire[3] += scene_npc_action_foley_play(slot, 2);`.
   Keep this independent of the rifle/continuous/ordinary weapon Launch
   branch and before the existing `if (!animate)` early return.

Worker-owned `scene_ai_reload.inc` now calls
`scene_npc_action_foley_play(slot, 39)` where its former flat sound call was.
No change is made to its action39 mapping, motion loading or return status.

These hooks deliberately preserve the current accepted-shot/reload-start
boundaries, including no-animation shots and optional missing clips. The
original generic helper itself gates on mapped motion and the sound flag;
this slice corrects routing and origin without claiming to reconstruct every
original action-start call site or changing the port's established gates.
Weapon Launch remains independent and keeps its current eye-position origin.
NPC action Foley now uses the original body/published origin. No gameplay,
reload duration, ammunition, AI cadence, animation scheduling or save change
is required. The parent should update the shared open milestone and mark
this slice unverified until the consolidated Xbox batch.
