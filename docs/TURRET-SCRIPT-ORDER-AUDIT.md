# Authored turret event audit (2026-09-30)

No installed Attack, Shoot_At, Shoot_Once or Fire_Weapon_No_Anim event uses a
placed static turret as its shooter or linked victim. A new fixed-point turret
shooter adapter is therefore deferred; the concrete campaign work is two
existing state callbacks described below.

This audit read all 94 installed RFL files directly through the existing VPP,
RFL-section and v180 entity/event readers. It found 46 Attack, three Shoot_At,
25 Shoot_Once and 25 Fire_Weapon_No_Anim events. For Attack, the shooter is
`words[0]` and victims are linked objects (or the named player); for Shoot_At,
linked entities are shooters and the event position is the fixed aim point.
There are nine Stationary Turret and two Stationary Turret_Plain placements,
and no placed Auto Turret Head entity. This is an authored-record relationship
audit, not an original executable run or campaign walkthrough. Generated details
are in the ignored `artifacts/turret-script-order-audit.json`.

All three installed fixed-point orders use skeletal actors:

| Level | Shoot_At UID | Shooter UID | Class |
|---|---:|---:|---|
| L13S3 | 8858 | 8856 | env_guard |
| L14S2 | 10582 | 10438 | merc_heavy |
| L15S1 | 9489 | 8278 | merc_heavy |

L13S3 also links non-entity UID8961. None of the firing orders directly links
one of the 11 static turret owners. Original Shoot_At ON/OFF semantics are
already documented at binary addresses 0x4b95e0/0x4ba2c0 in
`SCRIPTED-SHOOT-AT-FIRST-PASS.md`; that generic entity behavior alone does not
establish that an installed campaign sequence needs a turret-specific adapter.

## Concrete campaign callback gaps

| Level | Event | Links to static turrets | Required behavior |
|---|---|---|---|
| L14S1 | Make_Invulnerable 10185 | 9712, 3674 | Toggle owner object flag bit4 |
| L14S1 | Set_AI_Mode 10192, word0=0 | 9712, 3674 | Catatonic AI (runtime1) |
| L14S1 | Set_AI_Mode 10194, word0=1 | 3674, 9712 | Waiting/resume AI (runtime2) |
| L19S1 | Set_Friendliness 9939, word0=0 | 9877, 9879 | Change neutral turrets to hostile |

Set_Friendliness9939 also links skeletal merc_grunt9930. Both static turret
records and that merc start with authored friendliness1; the event changes
them to0. Omitting the turret callback consequently leaves these turrets neutral
under the current ordinary targeting policy.

At audit time, `campaign_set_friendliness` and `campaign_set_invulnerable` in
`scene.c` handle only the player and skeletal NPC wrappers. Both return
RF_NOT_FOUND for a static turret. Event dispatch counts these as other targets
and continues, so a successful event dispatch alone cannot establish that the
turrets changed state. `campaign_set_ai_mode_acquiring` already forwards static
turrets to `scene_turret_set_ai_mode`; the two L14S1 mode events have a source
integration path.

Exact integration plan, after the parent's current native run releases source:

1. In `campaign_set_invulnerable`, resolve `scene_turret_lookup(handle)` before
   falling back to skeletal NPCs. Set/clear bit4 in the turret's
   `view.flags_7c`. The ordinary turret damage adapter already passes those flags
   to `rf_damage_dispatch_sp`; no separate damage formula is needed.
2. In `campaign_set_friendliness`, resolve the same owner, write
   `damage.effects.affiliation=value`, and clear its target, burst, pending fire
   deadline and angular drive through `scene_turret_combat_release`. The callback
   precedes the combat helper definition, so use a small forward-declared bridge
   implemented after `scene_turret_combat.inc`. Clear pending state immediately,
   including when a save occurs before another AI tick. The existing turret
   checkpoint stores affiliation and combat state.
3. Keep fixed-point turret shooter orders deferred unless a concrete supported
   use case appears. Such an adapter would need order ownership/event UID,
   fixed point, ON/OFF replacement rules and ordinary-save persistence; assigning
   action9 alone currently suspends autonomous turret AI without firing.

## Shooters versus victims in current code

`campaign_script_attack` resolves its shooter only through skeletal NPC bodies;
`campaign_script_shoot_at` similarly assigns linked skeletal owners. Static
turrets therefore do not yet execute these scripted shooter orders. This is a
capability gap, but the inspected installed records do not make it a campaign
dependency.

The parent's current `scene_campaign_vehicle_target` now falls back to
`scene_turret_target`. An ordinary NPC Attack can therefore admit a live static
turret victim, obtain its aim point/health and release the order when it dies.
Hitscan nearest-contact selection and damage share the turret namespace in
`scene_driller_projectile_contact.inc`. Saving an NPC Attack whose target is a
static turret is still not integrated; no authored occurrence was found, so
that remains a lower-priority persistence extension. Thus victim support must not be confused
with scripted-shooter support. No new native test or runtime claim is made by
this audit; the prior isolated Xbox damage and autonomous-fire checks remain
the available evidence.
