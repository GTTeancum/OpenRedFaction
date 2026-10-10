# Ordinary NPC bullet impact audio

Source-written 2026-10-09 against frozen a68c1a35. Integrated after the completed 23:00 batch; no build, test, syntax
check, fixture, emulator, route or original-game execution was performed.
Compilation, bank admission and natural playback remain unverified.

## Practical live consumer

The accepted NPC single-ray and shotgun paths now request the existing
resident dry weapon-impact audio after an actual qualified contact:

- Player or NPC body, after held-shield and nano interception, retaining the
  selected body's entry point and authored collision material
- Detached piece successfully selected and damaged by the existing fragment
  helper, using its retained point and explicit Default material
- World/mover stop reported by that same helper, using the shared audio-only
  ordered material query limited to the existing nearest piece/body/vehicle
- Direct clutter hit after the existing damage call succeeds, using its
  selected fraction and explicit Default material
- Vehicle/turret contact with a live owner captured before damage, followed
  by successful dispatch and handled=1, using its retained point and Default

Playback does not require positive damage: a qualified invulnerable contact
is still an impact. Conversely, handled=1 alone does not authorize vehicle
audio because the shared dispatcher also consumes stale owners. A real
lethal contact keeps its captured qualification if callbacks retire it.

The fired weapon and audio origin are retained across damage callbacks. The
player's authored collision material is used for player contact; Flesh is
not guessed. NPCs use collision_material, the retained object1fc material.

## Owned-image source grounding

Read-only disassembly of Installed_Game/RF.exe, the same owned image recorded
in WEAPON-IMPACT-AUDIO.md, supplies the entity-contact evidence:

- 4c59f0 is the existing entity-contact dispatcher. Its interception and
  specialized effect branches precede ordinary impact presentation.
- 4c6463..4c6471 reads projectile1d0 material and descriptor178+material*4,
  then resolves the authored Foley group with434da0.
- 4c64d9..4c64eb retries the dry Default entry only when the resolved sample
  is exactly-1, including the material0 self-retry already implemented.
- 4c64f5..4c6509 requests positional5056a0 playback at projectile1b4 with
  volume1. This ordinary entity branch is shared; it has no player-only
  shooter gate.
- World presentation4c5912..4c5996 independently selects material/default
  and plays at the actual contact. Its underwater predicate remains a
  separate branch, never inferred from alternate trigger state.

The original entity branch also contains special overrides at4c6481 and
4c64a7. Those flags/weapons are not reconstructed by this ordinary dry
adapter. Shield, nano, melee, sniper/rail and projectile-special consumers
are deliberately outside this slice.

## Integration and ownership

- Include scene_npc_bullet_impact_audio.inc immediately after the existing
  scene_weapon_impact_audio.inc.
- combat_enemy_fragment_shot_contact adds only an optional out-contact to
  the original fragment/world procedure. Its legacy wrapper passes NULL,
  retaining the original API for melee, turret, clutter and existing tests.
- campaign_clutter_enemy_fire_contact similarly reports a direct accepted
  clutter fraction, or-1 for a miss/cover. Its legacy API remains unchanged.
- Ordinary NPC single-ray and shotgun consumers use these observations;
  the existing blocked/consumed values alone still control gameplay.
- The vehicle guard depends on the separately prepared shared
  scene_driller_projectile_contact_live helper in
  scene_driller_projectile_contact.inc. Parent must integrate that helper
  before this patch. It is not duplicated here.

No bank, PCM, sample catalog, owner, archive, save record, timing field or
gameplay RNG is added. The optional 64-sample/192KiB resident bank, existing
generation-qualified spatial voices and separate sound RNG are reused.
Audio remains void/best-effort and cannot reject or change an accepted shot.
Shared rf_scene_weapon_impact_audio counters record these requests too.

## Explicit silent/deferred cases

Sniper and rail are excluded even though their groups are resident. Turret,
mounted and melee callers keep the original silent wrapper. An intercepted
held shield or nano contact does not emit a body sound. A blocked fallback
after a rejected shield commit stays silent because only obstruction, not
its final qualified contact, is retained there.

Clutter-selected rays blocked by world/rubble remain silent: consumed alone
cannot identify an accepted clutter hit. The later legacy fragment call
inside that helper is unchanged and does not emit audio. These exclusions
avoid a second target-selection or damage pass just for feedback.

The shared ordered world query retains the documented mover/static overlap
limitation. Exact skeletal contact, underwater projectile-state ownership,
special original entity overrides and complete precision parity are not
claimed. Acquisition/visibility checks, shots not fired, and genuine misses
produce no new impact request.
