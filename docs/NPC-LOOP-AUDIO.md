# NPC continuous Launch ownership

Status (2026-10-09): the bounded helper and parent scene hooks are integrated
and source-reviewed. Xbox compilation and native/audible playback remain unverified.
No build, test, fixture, emulator, input mutation, image or cleanup was run for
this slice. The parent owns the scheduled validation batch and shared hooks.

## Missing live consumer and original evidence

`campaign_enemy_tick` already admits Machine Pistol and `heavy_machine_gun`
primary shots through `scene_ai_extra_weapons.inc`, using their original
magazine, spread, damage and cadence definitions. Its accepted-shot
presentation previously relied on entity `action_sounds[2]`, which is empty
for relevant authored fire actions. Calling the ordinary discarded-ID sound
helper on each bullet would accumulate unowned loops.

Installed-table evidence:

- `entity.tbl` gives `elite` the Machine Pistol at line 2519 and an empty
  `fire_stand` sound at 2569; the `merc_heavy` HMG overlay is at 3597.
- `weapons.tbl` declares Machine Pistol `continuous_fire` at 974 and primary
  `Machine Pistol Launch` at 1023. The HMG continuous flags are at 1440 and its
  primary `HMG Launch 1` is at 1493.
- `foley.tbl` lines 1913–1914 and 1939–1940 map these to `machine_loop.wav` and
  `Heavymachinegun.wav`. `bluebeard.bty` lines 12047–12054 and 5561–5568 mark them
  looping from 0. The existing metadata-backed playback already honors those
  flags; no loop metadata is synthesized.

Installed `RF.exe` source addresses:

- `4269c1..4269fb` sends continuous firing through `41a870`.
- `41a9bd..41a9c3` guards a new launch with the actor's retained voice at+81c.
- The NPC branch `41aa3d..41aa95` chooses primary+160 or alternate+164,
  routes the actor eye position+7d4 through `48a9c0`, and retains the result
  at+81c. `48aa06..48aa21` selects positional `5056a0` for nonlocal actors.
- `41aee3..41aef7` stops and invalidates the retained voice; `424fdc..424fef`
  also closes it during actor teardown.

These are source/installed-asset findings, not original-game runtime checks.
The port's live NPC scheduler supports primary fire only. This adapter does
not introduce HMG alternate fire, Machine Pistol Special selection, new AI
trigger state, or the separate HMG start-delay/attack/decay presentation.

## Bounded implementation

`scene_npc_loop_audio.inc` has 30 fixed records, matching the ordinary mixer
voice capacity. Each record is 32 bytes: source NPC handle, stable scene slot,
authored UID, exact mixer generation handle, pass marker, admitted weapon ID,
public audio ID and sample ID. Sixteen 32-bit telemetry words make total fixed
tracking 1,024 bytes. There is no heap allocation or second sample cache.

Before use, the scene slot and source UID must belong to an ordinary live NPC.
Both the object registry and entity registry must resolve the complete handle
to that same owner; registration, view and damage handles must agree. Hidden,
dead and invalid sources cannot retain a voice. The source tuple is checked
again at the end of the pass, covering an earlier shooter killed by a later
actor in that same pass. Restore or source replacement closes presentation
before publication; no native handle or episode enters checkpoint data.

The first actually accepted ordinary shot starts the authored primary Launch
with the existing positional sound service. The record retains both the public
audio ID and the complete mixer generation returned after successful playback.
Refresh and stop require both to match; a recycled mixer slot or changed ID-map
entry cannot redirect ownership to another sound. Refresh copies the source's
current eye position and uses the existing spatial gain/pan path.

Each episode gets one playback attempt. Further admitted observations retain
the record even if PCM admission failed or the voice was reclaimed, preventing
per-frame allocation and RNG retries. Capacity exhaustion leaves sound absent
and does not affect the accepted shot. Asset loading/eviction remains under the
existing audio-bank budget; the helper adds no PCM budget.

## Exact firing lifetime

The pass starts by clearing presentation-only seen markers. There are only two
ways to mark an existing episode:

1. The scheduler reaches its actual `!once && frame < owner->combat_due`
   continuation, after all existing source/mode/target, ammo/reload and holster
   gates. This only retains an existing episode; it never starts playback.
2. The scheduler actually accepts a shot after its existing due-shot pain,
   aim, range and obstruction checks. This may start the episode.

At pass end, an unseen owner closes. Thus reload, empty magazine, changed
weapon, rejected mode/target, hidden/dead source, seated/turret suppression,
failed due-shot admission and complete combat-pass skips cannot leave an
unowned loop running. A changed weapon encountered during cooldown closes
the previous record without starting the new one. The accepted last round
still starts normally; the next ammo gate ends its episode.

The cooldown branch intentionally precedes pain/aim/range/LOS checks in the
existing gameplay scheduler. Audio does not move, duplicate or predict those
checks: an episode may remain until the real due-shot branch evaluates them.
This is a presentation policy over the existing port cadence, not a claim to
have reconstructed the original NPC held-fire state machine.

Queued single-fire requests (`once`) bypass ordinary cadence and have no
retained held lifetime. The adapter closes any former episode for that source
and explicitly suppresses continuous Launch audio for that accepted request.
It does not invent a duration or convert the loop into a one-pass sample.
This unsupported presentation branch is intentional; queued gameplay shots,
including no-animation requests, remain unchanged.

Weapon Launch remains independent of nonempty authored action Foley. The
parent retains the latter and the existing rifle burst specialization. Optional
audio errors never change attack acceptance, ammo, AI orders, damage, RNG used
for gameplay, gameplay deadlines, movement, animation or save formats.

## Parent integration contract

1. Include the helper beside `scene_npc_rifle_audio.inc`, after `combat_sound`,
   registered NPC state and campaign audio services are declared. Forward
   declare `close`, `reset` and `remove(uint32_t)` for earlier lifecycle sites.
2. In accepted `campaign_enemy_fire_presentation`, retain the rifle branch;
   otherwise use `scene_npc_loop_audio_weapon(owner->view.weapons[0])` to
   select this adapter and add `scene_npc_loop_audio_shot(slot, once)` to
   requested-sound telemetry. This function returns a request count, not shot
   success. Retain independent action Foley and existing motion handling.
3. Replace only the actual cooldown continuation with:

   ```c
   if(!once && frame<owner->combat_due) {
       scene_npc_loop_audio_tick(i);
       continue;
   }
   ```

   Keep it after ammo readiness and holster checks and before the unchanged
   due-shot gates. No speculative call at awareness or animation start.
4. Surround the entire outer conditional `campaign_enemy_tick` call with
   `scene_npc_loop_audio_begin()` / `scene_npc_loop_audio_end()`, including
   the outer development-fixture skip and inner swim early return. On a fatal
   enemy-pass error, additionally call `scene_npc_loop_audio_close()` before
   propagating the same error, because earlier actors may already be marked.
5. Call `remove(full_handle)` immediately after successful ordinary NPC
   unregister, and when actual NPC death is entered. The latter also covers
   player damage after the enemy pass. For a committed teleport, close at the
   existing successful AI-state/relocation boundary. Never stop by UID or
   low registry slot, and never close merely because candidate validation
   failed before the world/source changed.
6. Close before freeing/replacing NPC bodies, audio bank/mixer/public-ID
   resets, ordinary scene teardown and successful checkpoint publication.
   Use `reset()` with the other scene-start audio resets. Failed private
   checkpoint staging must not close live sound. New playback after restore
   requires a genuinely accepted new shot.

`rf_scene_npc_loop_audio[16]` reports calls, successful starts, exact voice
stops, retained observations, failures, stale audio identities, position
refreshes, active records, peak records, capacity denials, latest status,
fixed bytes, accepted-shot callbacks, suppressed single requests, invalid
sources and closed episodes, in that order.

## Verification boundary

Source inspection covers identity qualification, bounded storage, start/stop
ownership, unchanged gameplay gates and the final parent integration, including
accepted-shot routing, cooldown retention, skipped/error passes, death,
removal, teleport, scene teardown and successful world publication. Compilation,
native device behavior, audible spatial movement and natural NPC firing
remain pending the parent-coordinated Xbox batch. No fixture or original
input was modified to force this branch.
