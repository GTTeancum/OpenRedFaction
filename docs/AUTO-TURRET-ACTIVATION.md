# Generated Auto Turret activation

2026-09-30: implemented in new `scene_turret_generated_activation.inc`, not yet
integrated or built by this helper. No native run, PC build, image or original
runtime was used. Parent owns integration and Xbox validation.

The ordinary NPC combat loop selects a weapon before its sight checks. The
authored Auto Turret base is unarmed, so it never reaches that awareness code.
This adapter gives the generated pair a bounded awareness step through the
same target/hostility/disguise and world-cover callbacks used by head combat.
It does not give the skeletal base a gun or synthesize an authored child UID.

## Concrete hooks

Include after generated ownership, turret-combat types, `campaign_enemy_mode_admits`,
NPC motion helpers and `combat_sound` are defined. A forward declaration of
`scene_turret_generated_activation_allows(uint32_t)` may precede the generated
owner include when `scene_turret_generated_ready` is defined earlier.

1. Call `scene_turret_generated_activation_reset()` before creating this level's
   generated heads. After each successful factory call, invoke
   `scene_turret_generated_activation_initialize(head_handle)`. Initialization
   only records the head identity, sets base AI class bit `0x20000000`, mirrors
   the flags into firing state and keeps both base weapon slots at -1. It
   preserves readiness already owned by checkpoint restoration and never sets
   ready bit0 by itself.
2. At the top of ordinary NPC combat, before pending single-shot handling,
   skip actors for which `scene_turret_generated_activation_suppresses(handle)`
   returns true. This includes a surviving base whose head died. Inventory,
   damage, animation and ordinary actor lifetime remain with their existing
   owners. Keeping its weapon slots at -1 also suppresses existing armed
   steering/pursuit branches. Do not copy the Head's Vauss into the base.
3. In `scene_turret_scene_tick`, build the existing backend as today, call
   `scene_turret_generated_activation_tick(frame, &backend)`, propagate any
   failure, then call `scene_turret_combat_tick`. Frame0 performs no awareness:
   ordinary world restoration still owns that startup boundary. Publish the
   current generated tag position before these eye/cover queries.
4. Extend `scene_turret_generated_ready` to also require
   `scene_turret_generated_activation_allows(head)`. The readiness latch remains
   saved when the base becomes catatonic or hidden, but those conditions must
   inhibit firing immediately. The gate rejects dead/missing/coupled-dead bases,
   hidden/removed flags, AI-blocked bit0x100, catatonic action1 in either mirror,
   active scripted animation, and the existing mode-policy exclusions.
   Waiting2 is eligible. Do not make the latch alone the combat permission.
5. Before capture of the generated companion, call
   `scene_turret_generated_activation_checkpoint_admit()`. Installed bases have
   no action28 and pass. A modified class with an in-flight optional ready
   action returns `RF_NOT_FOUND` because RFTU2 does not encode its pending phase.
   Dormant and fully ready states remain representable. This narrow rejection
   avoids silently restarting an unsupported animation at a save boundary.
6. After a successful whole-world/generated checkpoint assignment, reset the
   transient activation bank with `scene_turret_generated_activation_reset()`.
   Saved base flags and playback remain untouched. The next eligible tick
   rebuilds frame guards and mirrors the saved ready bit. Do not reset flags,
   invoke the factory again, or acquire a target during load publication.

The helper owns only a fixed128-entry array (head handle, last frame, optional
action-pending bit) and eight diagnostic words; it adds no heap allocation.
`rf_scene_turret_activation` reports sight checks, stimuli, ready transitions,
action starts, inhibited ticks, base-fire suppression, last base UID and error.
Scene reset and checkpoint transient reset clear these counters deliberately.

## State behavior and evidence

Dormant eligible pairs check sight every30 frames, staggered by base slot.
Candidates use the existing head's authored FOV and Vauss range, live-target
policy, disguise policy and source-aware world/clutter cover. The base itself
is explicitly excluded. Motion-only AI11 requires a moving candidate for
initial sight acquisition. Existing base combat alert/scripted activation can
also supply the stimulus; no alert is fabricated on creation. The exact original
awareness cadence is not established: this is a practical port policy.

After a stimulus, action28 (`idle_to_ready`) is started once through the shared
motion residency and action-playback APIs when it exists. Completion is checked
with `rf_motion_action_active`; no guessed duration is used. Active restored
playback is recognized without restarting it. An absent action transitions
directly to readiness, while a declared action that fails to start reports a
format error instead of an endless retry. Scripted animation ownership suspends
this work.

Readiness requests logical state1 (`attack_stand`) through the existing motion
controller, keeps the base unarmed, sets bit0 in both `view.flags_7d0` and
`firing.flags_7d0`, and synchronizes the original ready-completion `flags_810`
bit0x10 across view, damage and firing mirrors. Later ready ticks maintain the
authored stance without restarting an already selected state. Head targeting,
aim, cadence, ammunition policy and damage remain in the existing head combat.

Original evidence is recorded in [the ownership audit](research/AUTO-TURRET-BASE-HEAD.md):
403076 supplies base AI class bit0x20000000; 401b64..401b9e/408e90 require the
base ready bit for its Head; 407375 handles absent ready action;
4073c0..40741a wait for action28 completion, set readiness through408ea0 and
retain unarmed base behavior. Installed tables provide stand/attack_stand but
no idle_to_ready action. The helper therefore does not block real campaign
bases on an animation they do not have.

## Limits

This is first-pass activation, not a reconstruction of the original complete
alert scheduler. Acoustic awareness, automatic lowering when targets disappear,
scripted target handoff from base to head and optional ready-action persistence
remain outside this slice. The existing backend must separately exclude the
linked base from actual bullet-body candidates; sight exclusion alone is not
self-hit protection. No functional success is claimed before integration and
a focused Xbox check.

## Script removal companion

`scene_turret_generated_retirement.inc` is a separate, source-only helper,
not integrated or built by its author. Current `campaign_remove_object` retires
an NPC base by unregistering it and closing its body, but does not visit its
generated head. Generated head removal itself currently has no matching branch.
This leaves the child registered without its parent. Separately,
`campaign_adjust_vitals` has a direct turret zero-health transition that queues
death presentation without joining generated-pair death; that is a real death
and must use the existing death join, not this removal helper.

The helper keeps a removed head's stable registration as a hidden,
noncolliding tombstone until level teardown, following the current clutter
removal policy. It preserves health, armor, dead/model state and the existing
death-dispatch reason, clears the physical link and firing cadence, and cancels
pending head death presentation. It does not request damage, kill events,
explosions, corpses or model replacement. The separate removal marker uses the
real base/head generation handles; it is not an authored UID or a death reason.

Removing the base retires its head first, then the caller continues ordinary
NPC removal. Removing the head leaves a surviving base visible and damageable,
but makes it catatonic/unarmed, clears readiness and outstanding combat/movement
orders, and prevents reactivation through the retired-head gate. The actor's
existing body and animation resources remain owned normally; the companion
does not claim to reconstruct the original removal animation policy.

Required parent wiring:

1. Include the new file after generated ownership/activation, turret combat,
   `scene_burning.inc`, pending turret death effects and NPC command helpers.
   Add early prototypes for its removal/query functions where necessary.
2. Before each fresh generated factory, call
   `scene_turret_generated_retirement_reset()`. It rejects a reset while live
   generated bindings remain. After teardown, reset only after
   `scene_turret_generated_close`. Do not erase markers on ordinary activation
   reset or in-session load.
3. At the beginning of `campaign_remove_object`, call
   `scene_turret_generated_remove(handle, now, &handled)`. Return any error;
   return success when handled is1 (a generated head). Otherwise continue its
   existing NPC/clutter removal. A generated base returns handled0 after its
   child retires so the existing unregister, body close and persistence steps
   still execute. Use the current scene time modulo RF_TIMER_PERIOD; the helper
   calls no removal callback and does not recurse into the caller.
4. Skip `scene_turret_generated_retired(head)` before generated tag publication
   in `scene_turret_scene_tick`; otherwise head-only removal reaches the existing
   linked-handle mismatch guard. Also reject that query in activation's allows
   predicate and skip retired rows before activation initialize/tick. The head's
   hidden flags already make normal contact/draw/target consumers inactive.
   Keep generated base handheld suppression in place after head removal.
5. Return success early for a retired head in `scene_turret_generated_death_join`
   and reject retired-head damage/vitals changes before terminal transition.
   No subsequent stale reference should manufacture death effects for an owner
   already removed. For an ordinary, nonretired turret whose health adjustment
   reaches zero, add the existing generated death join immediately after the
   `campaign_adjust_vitals` turret death block.
6. Call `scene_turret_generated_retirement_checkpoint_admit()` before generated
   snapshot capture. It explicitly rejects a removed pair. Existing RFTU2 has
   no separate removal state and its base cross-admission rejects retired NPCs;
   encoding removal as fake health0 or a kill reason would be incorrect. Removed
   pair save/reload remains an open integration item requiring a versioned
   representation or a deliberately supported terminal-owner policy.

Removal admission checks both registrations, stable combat sidecar identity and
absence of unsupported child possession/burning before mutation. It never
compacts the turret array or reuses an owner slot. Base burning is extinguished
through the existing owner service before base unregister; head-only removal
does not extinguish unrelated damage still acting on the surviving base.
`rf_scene_turret_retirement[6]` records base/head removals, repeated calls,
last base UID, removal reason and error. No native success is claimed.
