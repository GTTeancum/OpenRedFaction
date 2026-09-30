# Turret death presentation

`scene_turret_death_effects.inc` is a bounded, integrated presentation-only adapter. It reads the selected entity class's DeathSnd, Explode Anim,
Explode Anim Radius and Explode Offset from installed `entity.tbl`, retaining
class-specific values rather than deriving damage from a visual size.

| Class | DeathSnd | Explode Anim | Radius | Local offset |
|---|---|---|---:|---|
| Stationary Turret | Turret Death | explosion 1 | 0.6 | (0, 0.3, 0) |
| Stationary Turret_Plain | Turret Death | explosion 1 | 0.6 | (0, 0.3, 0) |
| Auto Turret Head | empty | absent | absent | absent |

Installed `vclip.tbl` resolves explosion 1 to the generic explosion recipe and
Small Explosion foley. The adapter reuses the existing script-vclip resource,
particle-start, tick, image and material paths. It requests DeathSnd at the
owner origin and vclip foley at the transformed explosion offset, following the
existing generic vclip presentation policy. This is an authored-metadata first
pass, not evidence of exact original death-effect timing or mixed-audio parity.
It performs **no radial damage, terrain cut, physics impulse or weapon launch**.

The existing corpse metadata owns replacement models and optional persistent
emitters; it does not contain these four death-effect fields. Both stationary
classes already use their authored replacement models. Neither enables a corpse
emitter: the Plain class's turret-smoke lines are comments. A future enabled
corpse emitter is counted as unavailable rather than substituted with invented
smoke. The Auto Turret Head has no authored death effect here and remains silent.

The narrow line parser follows existing scene metadata readers: bounded512-byte
lines, quoted strings, comments outside strings, duplicate field rejection and
finite decimal/vector values. It reuses the NXDK-safe decimal reader and does
not call strtod. Class-table scratch is capped at512KiB and freed after open;
profiles are bounded by the existing three-class turret cache and notifications
by128owner slots.

## Integration hooks

1. Include `scene_turret_death_effects.inc` after
   `scene_script_explode_runtime.inc`, when turret owners, combat audio and
   generic explosion services are defined. Add a prototype for
   `scene_turret_death_effects_queue(scene_turret_owner *)` immediately after the
   owner typedef in `scene_turret_owner.inc`; this enables its early damage hook
   without moving full presentation implementation before its dependencies.
2. While the existing effect-tables archive is open, after scripted Explode
   resources have had first claim on the shared budget, call
   `scene_turret_death_effects_open(&effect_tables,maps,map_count)`. It reuses
   existing vclip indices; no new particle bitmap namespace is required.
3. In `scene_turret_damage_receive`, after the live owner crosses to dead and
   selects its replacement model, call `scene_turret_death_effects_queue(o)`.
   Do the same in the actual live-to-dead branch of scripted vitals/Slay
   handling. Retain the existing was-alive guard. Do **not** call from startup
   dead-model selection, rendering, checkpoint assignment or frame-by-frame
   dead-state inspection.
4. Tick `scene_turret_death_effects_tick(stream,frame)` after the frame's ordinary
   damage and before `scene_script_explode_effects_tick`. A death captures its
   transformed position immediately and queues at most once; processing clears
   pending before requesting audio/particles, so exhaustion does not retry every
   frame. Duplicate notifications are counted and suppressed.
5. Call `scene_turret_death_effects_reset_pending()` after successful ordinary
   world-load publication. This discards presentation queued in the pre-load
   world. Restored dead owners generate no notification, so they emit no new
   death audio or explosion. A later death of a restored live turret can notify
   normally. Pending cosmetic history is not serialized.
6. Call `scene_turret_death_effects_close()` at level teardown. It only clears
   profiles/notifications; generic vclip resources retain their existing single
   owner and close through `scene_script_explode_effects_close()`.

## Bounded fallbacks and evidence limits

The generic shared limit remains four explosion-effect owners and512KiB of
resident effect resources. If the death vclip cannot fit, death audio and the
replacement model remain available; the unavailable visual is counted. Missing
class metadata skips that class's death presentation. Particle-pool or effect-
instance exhaustion counts a skipped visual and does not reverse a gameplay
death. Missing sound/voice budget follows existing `combat_sound` behavior and
its audio-error telemetry; an audio request is not proof of audible playback.

`rf_scene_turret_death_effects[8]` reports transitions, audio requests, visual
starts, visual skips, unavailable resources, duplicate notifications, last UID
and errors. Generic explosion counters continue recording emitter creation,
updates and release. A counted visual start proves scheduling, not inspected
pixels. Xbox compilation passes. The native check below covers scheduling and load
suppression; audible output and visual appearance remain unverified. No screenshots or
original-game run were used.

## Stock64MiB native evidence

`artifacts/xemu/turret-save-20260930-130832/report.json` passes60 source
and30 fresh-load frames. One death queues two audio requests (DeathSnd and
vclip foley) and one visual-effect start, with zero skips/duplicates/errors.
The restored wreck queues no presentation. Minimum endpoint headroom is3,211
pages (~12.54MiB). The harness restores its disc inputs. This verifies effect
scheduling and no replay, not inspected pixels, audible playback, or full
particle-lifetime fidelity.
