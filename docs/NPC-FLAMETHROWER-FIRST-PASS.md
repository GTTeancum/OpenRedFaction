# Authored NPC Flamethrower primary

Update: the parent stock64 Xbox batch compiled and linked this feature in source commit `7168ad0c4704c1f762c44f612302b37ebbe31a14` at 02:09:47 UTC on October 12, 2026. No runtime was performed; earlier source-only handoff wording below describes the pre-integration state.

Source-written against `7fdb1a826018517028340aadf7b7ef96091b0795` on
2026-10-12. The active checkout was not edited by this worker. No compilation,
syntax check, test, gameplay, campaign route, emulator, image, original asset
mutation, or saved-HDD operation was performed. Parent integration and the
scheduled stock64 Xbox batch are pending.

## Concrete missing behavior

A read-only installed entity-record scan found these explicit primary loadouts:

| Archive | Level | UID | Class | Primary / secondary |
| --- | --- | ---: | --- | --- |
| levels1.vpp | L2S1.rfl | 3115 | miner1 | Flamethrower / none |
| levels3.vpp | L19S1.rfl | 9911 | merc_grunt | Flamethrower / none |
| levels3.vpp | L19S2A.rfl | 11855 | merc_grunt | Flamethrower / none |
| levels3.vpp | L19S2A.rfl | 11891 | merc_grunt | Flamethrower / none |
| levels3.vpp | L19S2B.rfl | 12067 | merc_grunt | Flamethrower / none |

At the baseline, none of the ordinary or specialized primary selectors admits
Flamethrower. The `!selected.primary` branch skips these actual equipped actors
before ordinary awareness, pursuit or firing. The player flame implementation
and collectible weapon resource demand do not supply an NPC attack consumer.
This is an implementation gap, not a negative gameplay observation.

## Connected behavior

- Owned, registered living Flame users join ordinary player awareness,
  hostile/friendly NPC acquisition, retaliation, pursuit and authored point or
  finite single-shot orders. Existing catatonic, hidden, holster, pain, seat,
  retreat and scripted-AI gates remain in the parent scheduler.
- Authored five-unit range, 0.1-second fire cadence, fire damage20 and ordinary
  AI scale are loaded from the existing primary reader. The selector rejects
  changed profiles rather than treating another continuous weapon as Flame.
- Each accepted pulse consumes five loaded fuel units, or the remaining1..4
  units for a last pulse. Zero fuel uses the existing finite magazine reload,
  2.6-second deadline, presentation and exhausted-weapon fallback. The early
  target-independent reload completion service now recognizes Flame too.
- A pulse reaches the existing expanding flame-volume contact helper once per
  actual body sphere union. World/movers, detached pieces, vehicle chassis and
  opaque clutter block body contact. The source and related seat/turret owners
  are excluded. Eye immersion in a real liquid inhibits the primary.
- Player-body hits use the current ordinary SP difficulty lookup exactly once.
  NPC hits retain generic fire immunity, existing NPC ignition/burn ownership,
  damage reactions and lethal-death dispatch. Existing source-qualified vehicle
  flame damage is reused. No generic hitscan or extra one-round debit follows.
- Fire presentation reuses the actual selected action2 mapping, shared bounded
  flame particles and the generation-qualified NPC positional loop owner for
  authored `Flame Thrower Ignite 2`. Audio release/death/reload/selection cleanup
  remains in that existing owner.
- Accepted pulse geometry/source and the old-world epoch are copied before
  presentation. Full source and victim identities are resolved around damage
  callbacks. An accepted single-shot queue request is consumed before callbacks.

## Assets and original-source grounding

The installed `weapons.tbl` Flame row declares `continuous_fire`, `flame`,
`no_fire_through`, primary fire damage20, AI range5/5, SP clip100, drain2.0s,
fire wait0.10s, reload2.6s, Start Delay0.10s, the three ignition audio groups,
and the real third-person model `weapon_fthrower.v3d`. The scene's existing
primary reader supplies default AI scale1 when that optional pair is absent.

Direct installed RF.exe disassembly, without execution, confirms:

- `0041a870..0041a929` resolves the supplied actor, guards the weapon index,
  records the per-weapon active byte at actor+46c, and retains onset mode in
  actor+7d0. This is actor-owned continuous firing, not a player-only table.
- `004c8bd7..004c8be9` returns nonplayer SP primary damage as descriptor+120
  multiplied by descriptor+108. The existing `combat_enemy_primary_damage`
  helper remains the port's source-qualified application of that scaling.
- Existing source reconstruction in `PLAYER-WEAPON-LOOP-AUDIO.md` identifies
  actor+81c positional Launch ownership and stop/teardown. Installed Foley and
  `bluebeard.bty` resolve Ignite2 to `FT_Ignite_02.wav`, looping from zero.

RF.exe input SHA256 remains the project's recorded
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
No claim of executing the original continuous attack is made.

## Explicit first-playable policies

Fuel is batched at the damage cadence:100 *0.1 /2 =5 per pulse. It does not
reconstruct original fractional charge, ignition delay, Start/Stop cues or the
continuous emitter's frame scheduling. Aim/eye origin, finite cone, full last
partial dose and ordinary reload are practical NPC policies. Player charge
and its RFAP5 owner are unchanged.

The stream shares the already admitted player flame volume and fire-immunity
policy. It does not add original fire-particle collision, physical Riot Shield
flame interception, Nano projectile debit, flame prop damage or AI canister
alternate. These are deferred fidelity/extension work; the primary is a real
finite attack rather than an inert held item.

Cosmetics use the existing global particle budget and sampling policy. Loaded
third-person weapon resources already include owned NPC weapons. New NPC-only
maskbit10 requests Flame primary metadata, the shared emitter and shared128KiB
impact-material ceiling; it does not itself request a first-person view,
canister model or canister explosion bank. Existing collectible-drop demand
can independently request those player resources as before.

## Persistence and footprint

There is no new per-NPC member, heap owner, frame buffer, projectile pool or
save format. New retained globals are one four-byte resource flag and eight
32-bit observational counters (36 bytes total), excluding executable code.
Particles and optional audio use the existing capped pools.

Loaded/reserve fuel, fire/reload/navigation deadlines, target and playback use
existing RFNC fields. Existing RFAP NPC burns already retain a generation-aware
source and historical authored source UID. Residual mapped fire/reload clip
admission is extended through the exact descriptor-only Flame selector, with
all existing candidate inventory, class, mapping, playback, generation and
resource checks preserved. Candidate validation does not inspect the live
actor's currently selected weapon. No save/load guards are relaxed or removed.
Runtime save continuation remains unverified.

## Files and integration

New:
- `src/diagnostic/scene_ai_flame_profile.inc`
- `src/diagnostic/scene_ai_flame.inc`
- `docs/NPC-FLAMETHROWER-FIRST-PASS.md`

Changed:
- `src/diagnostic/scene.c`: metadata/consumer includes, dedicated post-admission
  dispatch, reset and sparse resource joins
- `src/diagnostic/scene_ai_projectile_demand.inc`: NPC-owned Flame demand bit
- `src/diagnostic/scene_ai_opposed_acquisition.inc`: Flame admission
- `src/diagnostic/scene_ai_reload_completion.inc`: target-independent completion
- `src/diagnostic/scene_npc_reload_checkpoint.inc`: exact residual presentation
- `src/diagnostic/scene_npc_loop_audio.inc`: authored Flame Launch loop

`NPC-FLAMETHROWER.patch` is relative to exact7fdb baseline files. Merge its
`scene.c` hunks with independently staged session/dialogue wiring; never copy
that whole file over the parent's changes. No build-system edit is needed
because these are private includes of the already compiled scene translation
unit. Parent milestone suggestion: "Authored NPC Flamethrower users now have
finite source-written primary flame combat; compile and tester action/save
behavior remain pending (docs/NPC-FLAMETHROWER-FIRST-PASS.md)."

No fresh runtime acceptance is claimed. The source scan also identified actual
`shoulder_cannon` NPC selections absent from ordinary admission; that separate
feature was deliberately left untouched in this cohesive Flame delivery.
