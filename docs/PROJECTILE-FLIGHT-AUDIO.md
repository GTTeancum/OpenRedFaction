# Ordinary rocket and grenade flight audio

Source-written and parent-integrated2026-10-09; awaiting the next hourly build. No
compile, syntax check, test, emulator, fixture, route, grant, image or original
execution was performed. Xbox admission and playback remain unverified until
the parent-coordinated hourly batch.

## Concrete missing consumer

Ordinary rocket impacts, grenade timed/object detonations, remote attachment
and remote detonation already invoke sound. Adding another explosion hook
would duplicate existing feedback. The real gap is the ordinary projectile's
retained `$Fly Sound`: neither `missile_loop.wav` nor `Grenade_Tick.wav` had a
live scene consumer before this change. The existing code requested grenade bounce/explosion feedback and rocket
launch/explosion feedback without requesting their authored flight loops.

The helper covers accepted player/NPC Rocket Launcher and Grenade pool entries.
Tankbot missiles share an NPC pool but use a different authored sound profile;
they are deliberately excluded. Remote charges have an empty Fly Sound, so no
loop is fabricated. Vehicle missiles, torpedoes, other projectiles and near-miss
Foley are outside this bounded implementation.

## Owned original evidence

Read-only source inspection of `Installed_Game/RF.exe`, known owned image
SHA-256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:

- `4c3d56..4c3d6d` parses `$Fly Sound:` through `434620`, storing the returned
  sample ID at weapon descriptor174. It is a waveform declaration, not a
  Foley group resolved randomly per frame.
- `434620..43468e` returns-1 for an empty name. `43468f..4346be` reads near
  distance and gain, then calls registration5054b0 with rolloff1.
- `4c7be2..4c7c05` checks that descriptor174 is nonnegative, calls positional
  playback5056a0 at projectile3c with gain1/category0 and retains its result in
  projectile2a0. Empty declarations retain-1 at `4c7c0d..4c7c14`.
- `4c70d5..4c70f9` updates a nonnegative projectile2a0 sound with5058c0,
  passing the moving projectile position at e4 and velocity at144, gain1.
  The port consumes position; Doppler is not newly implemented.
- `4c805a..4c806d` calls505a40 on the retained voice and resets projectile2a0
  to-1 during projectile retirement.

The original `tables.vpp/weapons.tbl` Rocket Launcher binds
`"missile_loop.wav" 6 0.9`; Grenade binds `"Grenade_tick.wav" 6 0.9`.
`bluebeard.bty` declares both as looping sounds with Loop Start0. The original
`audio.vpp` directory gives130596 bytes for missile_loop.wav and22338 bytes for
Grenade_Tick.wav,152934 file bytes in total.

Existing name-based registration deliberately retains first-declaration
parameters. The Foley table already declares the same grenade waveform under
`Grenade Tick` at near5/gain.8, before this optional loader. The helper requests
the authored Fly Sound parameters6/.9 without overwriting that shared sample's
prior declaration. Exact registration-order/attenuation parity is not claimed.
The original direct sound lifecycle is implemented without reopening the
shared registration ordering as unrelated fidelity work.

## Files and parent integration

New authoritative files:

- `src/diagnostic/scene_projectile_flight_audio.inc`
- `docs/PROJECTILE-FLIGHT-AUDIO.md`

Parent-owned source copies and the proposed lifecycle patch are outside the
checkout under `/workspace/shared/projectile-flight-next/`; the single patch
is `lifecycle-hooks.patch`. It covers these existing parent-owned files only:

- `scene.c`: declarations, helper include, optional startup admission,
  audio-open/teardown/reset boundaries, player launch and movement
- `scene_grenade_gameplay.inc`: successful release publication, movement,
  terminal/object consumption and frame-zero reset
- `scene_ai_rocket.inc`: ordinary Rocket Launcher publication, movement,
  terminal consumption and reset, excluding Tankbot Missile
- `scene_ai_grenade.inc`: accepted publication, movement, terminal consumption
  and reset
- `scene_ai_projectile_checkpoint.inc`: retire old transient voices at commit;
  expose a separate resume helper over the validated published arrays
- `scene_world_load.inc`: resume only after every fallible publication and
  native storage-close operation succeeds

Do not replace the full copied files over unrelated parent edits. Review and
apply only the patch hunks. No existing authoritative source, TO-DO.MD,
original asset, build configuration or checkpoint format was edited by the
helper.

### Exact lifecycle boundaries

The helper is included after `combat_sound`, where all audio services exist,
and before the gameplay projectile includes. Forward declarations allow
`campaign_audio_open` and `campaign_close` to call reset/close/load earlier.

`scene_projectile_flight_audio_load(&archive)` runs after mandatory startup
sounds and optional conventional impacts, before final bank counts. Refresh
`rf_scene_live_audio[0]` afterward because the missile waveform may add a bank
declaration. Its void return cannot reject level loading.

`start(family,slot,position)` runs once only after the accepted projectile is
published and its existing ammo debit succeeds. Call `stop(family,slot)` before
reusing that inactive pool slot. The starter also defensively retires a prior
sound owner, without touching flight state.

`step(family,slot,active,position)` runs immediately after a successful flight
step, before terminal damage, shield consumption, detonation or expiry handling.
An inactive terminal closes before any subsequent early continue/return.
A live projectile updates its retained spatial position. A failed flight step
closes that slot before preserving the existing gameplay error return.

`stop_family` precedes the existing grenade/NPC pool memset operations.
`close` runs before native ID-map/audio-bank reset and scene teardown. It also
ends temporary sample pins. Changing weapons, running out of ammo or removing
the original thrower does not stop a projectile that remains independently
active.

`forget` runs at `scene_ai_projectile_checkpoint_assign` entry, after the
world loader has validated candidates, before replacing any projectile arrays.
It closes stale voices while keeping admitted PCM. Assignment itself never
starts a voice: occupied-vehicle publication later in the same world-load
commit can still fail. The separate resume helper is called at the final
successful world-load boundary, after the native storage-close operation too.
A local resume flag is armed only by completing publication with a present
projectile component; the call additionally requires final status0. Thus an
occupied-vehicle publication failure, storage-close failure or absent component
cannot create new flight audio. Resume walks only active published ordinary
rocket/grenade slots. No voice IDs or audio phase enter the save. Failed
optional audio cannot reject or change restoration. A legacy absent projectile
component still closes old transient sounds without inventing a restored flight.

## Bounded ownership

The helper has74 fixed transient owner records:50 player rocket slots and
8 each for player grenade, NPC rocket and NPC grenade. Each record is16 bytes,
with two small sample-pin records and diagnostics. No projectile pointer,
registry handle or thrower lifetime is used as the sound owner. The gameplay
pool's explicit publication and retirement boundaries define the episode.

At most four flight voices can be admitted simultaneously. Every start records
its attempt even when PCM is unavailable or native/mixer capacity is exhausted;
movement never retries a refused/stolen voice. Terminal state/reuse/reset ends
the episode. The4-voice limit is an explicit port budget rather than a retail
constant; existing global30-slot native admission remains authoritative and
never steals an active ordinary voice.

Each sound retains both the nonnegative device ID and complete mixer handle,
plus the sample identity. Moves/stops requalify all of these against the shared
mixer/spatial owner, preventing stale IDs or recycled low slots from operating
on a different generation.

Startup admission permits exactly the two supported nonempty, loop-from-zero
waveforms, at most192KiB total file PCM, retaining256KiB free shared bank
headroom. It never evicts another sample. Failure rolls back only new PCM not
yet exposed to voices; existing declarations, borrowers and eviction flags are
preserved. Successful admission temporarily pins those two bank rows; closing
restores previous flags without unloading borrowed PCM.

Live launch/step/retirement performs no archive lookup, file read, waveform
reload or heap allocation in the helper. The existing backend owns native
voice creation and borrows the same pinned PCM pages; no private PCM bank or
per-projectile copy is introduced. All audio APIs are void presentation
observers and leave damage, ammo, trajectories, contacts, lifecycle, firing
cadence, RNG and save layouts untouched.

## Diagnostics and verification boundary

`rf_scene_projectile_flight_audio[16]` exposes requests, starts, stops, moves,
stale generations, errors, voice-cap refusals, live/peak voices, admitted sample
count/bytes, ready state, last status/family/slot and fixed tracking bytes.
The existing opt-in combat trace emits `PROJECTILE_FLIGHT_AUDIO_START` only
for a successful admitted voice. No new gameplay fixture or input route exists.

Source review covers accepted publication, failed admission, terminal shield
consumption, bouncing/resting grenades, projectile independence from shooter
lifetime, slot reuse and checkpoint replacement. Those observations are not
runtime validation. Remaining parent checks are the scheduled Xbox build and
stock64MiB startup admission, followed by proportionate natural projectile
playback/lifecycle coverage when available. Audible output, native stop behavior,
overlapping-voice admission and checkpoint audio reconstruction are unverified.
