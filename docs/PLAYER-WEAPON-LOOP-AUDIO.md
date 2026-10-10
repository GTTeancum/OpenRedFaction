# Player weapon-loop ownership

Status (2026-10-09): shared owner and flamethrower input integration are
source-written. Parent-owned scene lifetime hooks and Assault Rifle integration
are a separate part of the same slice. No build, test, gameplay fixture, emulator,
campaign route, image or audible-output check was run by this implementation
worker. Runtime remains unverified until the parent-coordinated Xbox batch.

## Established live gap

The existing flamethrower input starts `Flame Thrower Ignite 2` once when its
ignition delay expires. `combat_sound` discarded the returned voice ID. The
shared `campaign_sound_start` already honors installed loop metadata, so that
voice continues indefinitely unless explicitly stopped. Release, exhaustion,
reload, deselection, unarmed/ownership loss, death, underwater inhibition and
alternate-canister inhibition reached `scene_flame_input_stop`, which played
`Flame Thrower Ignite 3` without stopping the ongoing loop. Repeated firing could
therefore retain multiple obsolete loops in the bounded voice pool.

The ordinary Assault Rifle also submitted `Assault Loop` for every shot. Its
primary burst sound and alternate continuous lifecycle require distinct hooks;
the generic owner must not turn primary bursts into continuous firing sounds.

## Original evidence

Read-only disassembly of installed `RF.exe` establishes one actor-local firing
sound owner at `actor+0x81c`, rather than one owner per shot:

- `41a9bd..41aa95` skips a new continuous launch while that owner is present,
  handles the existing release sound, and retains the resulting launch voice.
- `41aa0a..41aa38` schedules the local-player start delay from weapon `+0x200`.
  `41e5a3..41e5cf` consumes that timer and checks whether firing is still active.
- `41e642..41e688` selects the primary Launch group at weapon `+0x160` through
  `434da0`, starts flat local-player audio through `505560`, and stores the
  result at `actor+0x81c`.
- `41aee3..41aef7` stops and invalidates `+0x81c` before
  `41af01..41af52` resolves and plays the authored Stop Sound (`+0x204`).
  The existing `rf_weapon_reset` reconstruction retains the same stop ordering.
- `424fdc..424fef` also stops and invalidates `+0x81c` during actor teardown.

Installed `weapons.tbl` names `Flame Thrower Ignite 1` as Start Sound, `0.10` as
Start Delay, `Flame Thrower Ignite 2` as Launch and `Flame Thrower Ignite 3` as
Stop Sound. `foley.tbl` resolves those one-sample groups to `FT_Ignite_01.wav`,
`FT_Ignite_02.wav` and `FT_Ignite_03.wav`, each near distance8 and gain0.8.
`bluebeard.bty` lines3937..3944 declare `FT_Ignite_02.wav` looping from0;
the neighboring ignition and release sounds are nonlooping.

These are static source/asset findings. They do not claim an executed original
behavior probe or verified audible playback.

## Shared consumer

`scene_weapon_loop_audio.inc` owns one transient public voice ID, player handle,
weapon ID, resolved sample and attempted-start flag. It uses the existing Foley
selection, bounded bank, software mixer and native audio adapter. Installed loop
metadata must declare a supported zero-start loop. There is no second mixer,
new voice-allocation policy, repeating one-shot approximation, retry timer or
per-frame sample load.

`scene_weapon_loop_audio_start(weapon, name, position)` starts only once per
continuous firing episode. Subsequent admitted ticks retain the same owner,
including the failed-start state. Changing the player or weapon first closes the
old owner. The existing sample chooser does not advance RNG for a one-sample
group. A missing asset or exhausted audio budget cannot fail the gameplay tick.

`scene_weapon_loop_audio_stop(weapon)` closes only a matching weapon's owner.
This qualification matters because flame resources continue their inactive tick
while a different weapon is selected. `close()` is unconditional and silent for
relocation, death, ordinary restore and teardown; `reset()` also clears counters.
Every native stop first resolves the retained public ID to the complete mixer
generation. A stale/recycled slot cannot stop an unrelated voice.

The flame stop function now closes its loop before its existing release sound.
All existing input, ignition-delay, fuel, damage, reload, canister and animation
state changes retain their previous ordering and values. The input reset also
closes the flame owner. An ordinary saved ignition2 does not serialize device
IDs or replay ignition1 during assignment; its next genuinely admitted firing
tick acquires one new loop. No save format changes.

## Parent integration and verification boundary

Include the helper beside the existing weapon-audio consumer after
`combat_sound`. Close it before resetting audio IDs/mixer/native devices or
closing the sound bank, at the existing player-death transition and in the
shared player-relocation boundary. Reset its telemetry during scene startup.
The flame input and checkpoint reset call sites are already source-written.

`rf_scene_weapon_loop_audio[12]` reports requests, starts, actual stops, retained
episodes, failures, stale handles, last status, owner, weapon, public voice ID,
sample and attempted state. These are read-only observations; they cannot force
selection, firing, inventory or audio admission. Source inspection supports the
ownership fix; compile/runtime/audio-output claims await the scheduled batch.

## Assault Rifle integration

The same retained player owner is used only for alternate continuous fire.
Original4c8350 distinguishes primary bit2 and alternate bit4; installed Assault
Rifle has only alt_continuous_fire. The primary burst path426a00–426a54 selects
descriptor44c once when the full burst begins. Parser4c3a21 and43468f–4346be bind
assault_burst.wav with near8, volume.9, rolloff1; its installed Bluebeard metadata
is nonlooping. Assault_loop.wav loops from0.

The prior live player branch requested Assault Loop for every bullet. It now
plays the bounded original raw burst sample once on an accepted primary shot
whose pre-step trigger remaining count was zero. Pending burst rounds are
silent in this layer; gameplay cadence, bullets, damage and ammo are unchanged.

Alternate audio begins on the first accepted alternate shot: Assault Attack,
the authored.10-second/six-frame presentation delay, then the shared retained
Assault Loop owner. A per-frame audio-only admission check occurs after enemy
damage and before special-weapon early returns. Release, primary-mode switch,
valid reload, empty magazine, lost ownership, death or mounting closes the
episode; the loop is stopped before Assault Decay. Accepted selection closes
before changing equipment. Relocation/load/teardown closes silently without
inventing a serialized device handle or replaying a firing event.

This is a practical presentation adaptation to the existing fixed-tick trigger.
It does not shift shot cadence to reconstruct the original start delay.
Release-before-delay closes and emits Decay immediately; exact original pending
tail timing is deferred. NPC rifle audio uses the separate bounded consumer
described below. Source-only review/compilation/runtime status must be reported
separately.

Original automatic-empty reset is explicit at419c79–419ca8. The normal local
manual reload path enters425280/4a9f50 state8 and skips the NPC/special direct
reset at425607. All indirect manual-reload edges have not been reconstructed.
Closing at accepted manual reload and effective alt-to-primary arbitration is
therefore bounded scene lifetime policy, not a claim of exact original crossover.

## NPC primary rifle burst audio

Status (2026-10-09): `scene_npc_rifle_audio.inc` and the parent-owned `scene.c`
wiring are source-written and independently source-reviewed, awaiting the
parent's20:00 consolidated Xbox batch. No build, test, new gameplay fixture,
emulator run or audible-output check was performed by this worker.

The NPC fire-presentation fallback previously requested `Assault Loop` on every
accepted bullet and discarded each returned voice. Installed `foley.tbl`
lines1861..1862 map that group to `assault_loop.wav`, and `bluebeard.bty`
lines1794..1801 declare its zero-start loop. An unowned voice therefore had no
ordinary burst-end stop boundary.

Read-only disassembly of installed `RF.exe` confirms that the burst path is
shared by actors, rather than being a player-only presentation branch:

- `426a00..426a25` requires the burst flag, a present burst descriptor and
  actor `+0x500` equal to the authored full count before playing the sound.
  Other rounds jump past that sound request.
- `426a2b..426a54` uses weapon descriptor `+0x44c` and the actor-aware sound
  router `48a9c0`; the burst branch does not retain an `+0x81c` loop owner.
- `48a9c0..48aa25` routes ordinary NPC sound through positional `5056a0`;
  the eligible local-player branch instead uses flat `505560`.
- Installed `weapons.tbl` lines912..916 declare primary burst mode, count3,
  delay0.1 and `assault_burst.wav` with near distance8 and gain0.9.
  `bluebeard.bty` lines1780..1785 mark that sample nonlooping. The existing
  player consumer already declares the same sample with default rolloff1.

The bounded helper observes only the accepted-shot boundary. Capture
`combat_burst_remaining` immediately before `campaign_enemy_cadence`, then
pass that value, the selected primary definition, the queued-single flag and
the NPC eye position to `scene_npc_rifle_audio_shot`. Zero or a value at least
the authored count starts a new burst, matching the existing cadence reset
rule. A positive value below the count is a continuation and requests no sound.
The cadence call itself, target/aim/cover gates, ammo, reload, damage, script
queue and attack-motion code remain unchanged. Failed or denied gameplay
attempts must never call this consumer.

For Assault Rifle only, its return value replaces the generic action-label
sound request in `campaign_enemy_fire_presentation`; other weapon sound labels
retain their existing behavior. The return counts a sound request rather than
successful playback, preserving the meaning of `rf_scene_enemy_fire[3]`.
Neither unavailable audio nor an exhausted voice budget retries on continuation
rounds or falls back to `Assault Loop`.

Queued primary `Shoot_Once` and `Fire_Weapon_No_Anim` requests bypass ordinary
burst cadence in the current adapter. Each still spends one round and remains
one gameplay shot, with the latter's animation suppressed. Each receives one
nonlooping burst sample even when a regular burst is pending. This is a
practical current presentation mapping, not exact reconstruction of the
original event's actor burst state. Original `4badfc..4bae0c` calls primary
fire `425830` from `Shoot_Once`; no additional gameplay shots or original
burst-owner state are invented here.

The helper declares through the existing bounded bank, which deduplicates the
player/NPC sample slot by name; shared playback supplies positional admission
and natural one-shot retirement. It rejects metadata that marks the burst
sample looping. There is no per-NPC persistent state, cached sample lifetime,
owned loop, stop callback, new save field or audio RNG use. Include it beside
`scene_rifle_audio.inc` and call `scene_npc_rifle_audio_reset` during ordinary
scene startup to clear only the ten-word read-only telemetry array.

`rf_scene_npc_rifle_audio[10]` reports accepted rifle shots, sound requests,
starts, suppressed continuation rounds, failures, last status, sample, last
public voice ID, pre-cadence remaining and queued-single flag. The public ID is
an observation only and is never used as an owner or later stop target.
