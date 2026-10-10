# Drone Missile launch and flight audio

Source written2026-10-10 after the02:00 Xbox batch. This slice has not been
compiled, syntax-checked or run. Independent source review found no blocker.
Parent owns scene integration and the next consolidated stock-64-MiB Xbox
validation batch.
No test, emulator, route, fixture, inventory grant, asset change or image was
introduced or executed for this implementation.

## Concrete gap and original evidence

The accepted Drone secondary already creates a real finite-ammunition flight
in the shared eight-entry NPC rocket pool. It previously omitted launch sound
and deliberately excluded flight sound because the shared ordinary Rocket
sample declaration uses near6 rather than Drone's authored near10.

Read-only inspection of the installed original inputs established:

- `tables.vpp/weapons.tbl:2599-2628` defines Drone Missile. Lines2616-2617
  bind `$Launch: "Missile Launch Drone"` and `$Fly Sound: "missile_loop.wav"
  10 0.9`.
- `tables.vpp/foley.tbl:2193-2194` defines the single-variant Missile Launch
  Drone group as `missile_02.wav`, near20/gain0.8.
- `audio.vpp` holds85466 bytes for `missile_02.wav` and130596 bytes for
  `missile_loop.wav`. The latter is already the ordinary rocket flight sample.
- `bluebeard.bty:13645-13664` describes launch as nonlooping and the flight
  waveform as looping with Loop Start0.
- Original `RF.exe:426e5d-426e69` creates the secondary projectile and tests
  success. Only then `426e6f-426e9c` resolves its weapon descriptor160 Launch
  through434da0 and calls48a9c0. Its NPC branch `48aa06-48aa21` starts positional
  playback through5056a0.
- Projectile creation `4c7be2-4c7c05` independently starts descriptor174 Fly
  Sound and retains the voice at projectile2a0. `4c70d5-4c70f9` updates that
  voice's position/velocity; `4c805a-4c806d` stops it on retirement. The existing
  port consumes position without introducing Doppler.

Original name registration and the port's `rf_audio_bank_declare` preserve the
first case-insensitive declaration. This change does not claim to reproduce
original table-loading order. It deliberately supplies authored Drone profiles
per voice while leaving every shared bank row untouched.

## Accepted-shot integration

`scene_ai_missile_launch` invokes `scene_drone_missile_audio_accepted` only for
the exact `campaign_drone_missile` definition, after the actual reserve debit
and pool publication. No rejected launch, pool refusal or exhausted reserve
requests sound. Audio has no error return to gameplay.

The adapter first requests the constructor-owned flight loop, then the
independent nonlooping launch sample, matching the original construction order.
Launch residency/admission failure does not suppress a flight attempt, and a
flight-cap or native-voice refusal does not suppress the launch attempt. There
is no action2/Smash Foley request, duplicate primary presentation, combat RNG
draw, ammo change, cooldown change or weapon-selection change.

Flight uses the existing NPC_ROCKET audio owner at the accepted projectile
slot. `scene_projectile_flight_audio_start_profile` preserves the existing
four-live-flight-voice budget, full mixer-generation/device-ID/sample checks,
failed-attempt ownership and no-retry movement policy. The original starter
remains a NULL-profile wrapper for all existing callers.

The parent-supplied `campaign_sound_start_profile` copies the four spatial
parameters into the new voice before calculating its initial native gain.
Drone flight supplies near10/gain0.9/rolloff1; launch supplies20/0.8/1. Both
derive far distance with `rf_audio_far_distance`. Neither pointer to a stack
profile escapes: the spatial voice retains its own immutable value. Subsequent
listener and projectile movement updates use that retained profile. Ordinary
NULL-profile callers preserve the bank's first-declaration behavior.

## Residency and lifetime

The new helper owns one optional launch pin only. Startup admission follows
mandatory audio and existing optional consumers, accepts at most96KiB of launch
file bytes and leaves256KiB shared-bank headroom. It never evicts anything.
Unknown/looping launch metadata is refused. `rf_audio_bank_reload` publishes
only parsed PCM; no fallible step follows its success before pin publication.
Failed optional admission preserves existing bank declarations/resident PCM.

The flight helper continues to own the existing shared `missile_loop.wav` pin
and two-sample startup budget. Drone adds no duplicate flight PCM or owner
array. As with ordinary Rocket, flight sound is unavailable when that existing
optional two-sample admission failed, including failure of its grenade sample.
This dependency does not suppress independently admitted launch sound or the
missile itself. Maximum new PCM for this slice is85466 bytes.
The copied profile adds20 bytes to each of30 shared spatial rows and30 retained
message spatial snapshots,1200 fixed bytes total, plus the small launch state
and44-byte diagnostic array. No new flight owner array is added.

Every gameplay-time start checks that its PCM is still resident before calling
the native starter. Resident pins keep the ordinary lazy archive branch out of
accepted Drone audio; movement performs no allocation or archive access.

The NPC rocket movement hook now admits exact Drone flights alongside ordinary
Rocket. Terminal/inactive steps stop the owner before contact/damage handling;
step errors, explicit slot reuse, pool reset, successful Drone timeline
retirement, checkpoint forget and shared teardown already close the same slot.
Removing the original shooter does not stop an independently flying missile.
The launch one-shot retires through normal shared native/mixer ownership;
closing its helper releases only the residency pin, not PCM still borrowed by
that one-shot.

Tankbot remains excluded. Its matching authored filenames do not silently
enable another weapon. Existing Rocket/grenade profiles, checkpoint resume,
Drone save guards and all save wire layouts remain unchanged. The new close
hook must accompany the existing flight close at actual audio teardown/reset;
it is not a replacement for flight-owner retirement.

## Parent-owned scene hooks

The helper-owned files are:

- `src/diagnostic/scene_drone_missile_audio.inc`
- `src/diagnostic/scene_projectile_flight_audio.inc`
- `src/diagnostic/scene_ai_rocket.inc`
- this document

Parent owns `scene.c`, `TO-DO.MD` and existing milestone documentation. Required
scene hooks are:

1. Supply `campaign_sound_start_profile(sample, position, volume, category,
   positional, pan, const rf_audio_parameters *override)` and the immutable
   copied override in `campaign_spatial_voice`; preserve the old starter as
   a NULL-override wrapper and clear the override on every normal slot reuse.
2. Declare `scene_drone_missile_audio_load(rf_vpp *)`,
   `scene_drone_missile_audio_close(void)` and
   `scene_drone_missile_audio_reset(void)` beside existing optional-audio hooks.
3. Include `scene_drone_missile_audio.inc` after
   `scene_projectile_flight_audio.inc`, before `scene_ai_rocket.inc`.
4. Call the void loader with `campaign_audio_open`'s local audio archive after
   the existing optional admissions and before final bank counts. Do not gate
   it on `scene_drone_missile_resources`: audio opening precedes that later
   gameplay resource-demand calculation.
5. Close its residency pin at audio reopening and scene teardown before the
   bank is cleared; reset its diagnostics at ordinary fresh-scene reset.

`rf_scene_drone_missile_audio[11]` records accepted calls, launch requests,
launch starts, failures, last status/sample/voice, resident launch bytes, launch
readiness, flight requests and latest projectile slot. Existing flight audio
telemetry records loop starts/stops/moves/refusals. These counters establish
only requests and admitted voices, not audible output.

## Remaining limits

Source review and compilation do not establish actual firing, native audible
output, moving attenuation, terminal closure or restored-timeline runtime.
Exact original muzzle placement, flight visuals, Drone-specific impact
presentation, save persistence and original registration-order fidelity remain
outside this bounded audio implementation.
