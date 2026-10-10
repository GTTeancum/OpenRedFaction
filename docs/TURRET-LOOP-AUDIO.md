# Turret continuous-fire audio ownership

Status (2026-10-09): source-written and source-reviewed with parent lifecycle
wiring present. No build, test, emulator, fixture, input change or audio capture
was performed by this worker. Compilation and runtime remain unverified until
the parent-coordinated hourly Xbox batch.

## Original evidence and live gap

Read-only inspection of installed `tables.vpp` and `bluebeard.bty` establishes:

- `weapons.tbl`1711 marks Vauss `continuous_fire` and `from_eye`;1723 gives
  .08-second fire wait and1732 selects Vauss5 Fire.
- `entity.tbl`4399 supplies the Auto Turret Head force-launch override Vauss2
  Fire, already selected by the live scene's class-name branch.
- `foley.tbl`2109–2110 maps Vauss2 Fire to `vauss_02.wav`, near10 and gain.9;
  2115–2116 maps Vauss5 Fire to `vauss_05.wav`, near10 and gain.95.
- `bluebeard.bty`17887–17894 and17928–17935 mark those respective samples
  looping from0.

Read-only disassembly of installed `RF.exe` confirms the shared continuous
weapon owner.41a9bd tests actor+81c before another start.41aa3d–41aa95 selects
primary/alternate Launch, applies the class+750 force-launch override, calls
the entity sound router48a9c0 and retains the returned voice at+81c.
41aee3–41aef7 stops and invalidates that voice at firing reset, before any
separate Stop Sound.424fdc–424fef repeats the stop at actor teardown.

The previous `scene_turret_scene_shot` submitted `combat_sound` for every
accepted shot. That adapter discarded the public voice ID, while the shared
sound starter honored the looping metadata. Continuous firing therefore left
independent native/mixer loops without a firing-end owner. It also routed
autonomous turret sounds through the flat player adapter.

## Bounded transient ownership

`scene_turret_audio.inc` owns128 fixed28-byte sidecars, matching the existing
turret capacity:3,584 bytes plus16 read-only diagnostic words. Each sidecar
retains the full host handle, controller handle, weapon, public voice ID, exact
generation-bearing mixer handle, sample and one-attempt marker. It does not
modify the turret structure, combat state, archive inputs or any save layout.

The first accepted Vauss shot starts one loop through the existing Foley,
sample-bank, mixer and native services. Further accepted shots retain it.
Failed or reclaimed starts retain the attempted episode too, so no per-shot
retry/allocation policy is introduced. Unsupported/missing audio remains
optional and cannot reject a shot or gameplay tick.

Every stop and spatial update resolves the public voice ID and also requires
the exact retained mixer handle. A recycled slot cannot be stopped or moved.
Only the matching spatial record is cleared. Autonomous and NPC-operated
sources use positional audio; a player-controlled mount uses the existing
flat player route. Active positional voices refresh from the real live muzzle
during cooldown, including a generated head following its base.

## Firing and owner boundaries

The audio follows the existing port firing policy; it does not claim an exact
reconstruction of original turret AI scheduling or spin-up timing:

- Start at the existing accepted-shot sound point, preserving immediate onset.
- Keep the episode during the existing shot-cadence wait when target,
  readiness, operator and reachable-turn gates still admit firing.
- End on the existing combat release, changed target/action, missing target,
  unreachable aim, rejected due-shot convergence/range/cover, or shot rejection.
  Due-shot-only gates remain due-shot-only; no extra collision/AI queries are
  introduced during cooldown.
- Mounted fire release ends immediately, including during cooldown. A mount
  exit or replaced player/controller ends the old episode before handoff.
- NPC operator bind, unbind and reset close the previous host episode, including
  when the old endpoint is no longer registry-live.
- Host death, generated base/head death coupling and generated retirement close
  immediately. Owner/combat teardown closes all retained turret voices.
- A changed weapon/controller or full host generation closes before reuse.
  Frame rewind, frame-zero priming, inhibited scene ticking and fatal turret
  tick errors cannot leave previous loops active.

No targeting, cadence, burst counters, ammunition, spread, damage, projectile
acceptance or save fields are changed. HEAP remains on its existing projectile
sound path and explicitly releases a prior Vauss episode.

## Parent integration

The helper is included by `scene_turret_scene_combat.inc`, after operator
definitions and shared audio services. Earlier turret owner/combat consumers
have narrow forward declarations. Parent `scene.c` declares close/reset and
calls them before audio-ID/mixer teardown and at fresh scene startup;
`scene_world_load.inc` closes before successful world publication. Those hooks
are present in the shared working source at this review. Rejected restore
preparation does not touch live audio. No voice is serialized or replayed during
assignment; the next genuinely accepted shot may begin a new transient episode.

`rf_scene_turret_audio[16]` reports accepted Vauss sound requests, episodes,
starts, exact stops, retained shots, failures, stale handles, spatial refreshes,
current attempted episodes, last host, controller, public voice, raw handle,
sample, last status and fixed sidecar bytes. It is observation only and does
not grant weapons, manipulate input, invoke fixtures or force playback.

The parent batch should establish compilation first and report any natural
turret runtime evidence separately. Audible output, multi-turret concurrency,
controller handoffs and firing-end playback are not claimed verified here.
