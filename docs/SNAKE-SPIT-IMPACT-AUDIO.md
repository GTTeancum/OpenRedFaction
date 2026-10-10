# Snake Spit: retained-contact impact audio

Source-written against `140f81e7fa09eaca9bdbec4926e546252ba67962`, after
Small/Big mechanics and shared flight presentation. No compile, syntax check,
test, fixture, runtime, image, grant, forced event, route or original-input
change was performed. Parent owns integration and the scheduled08:00 Xbox
batch. Playback, audible output and stock64MiB admission remain unverified.

## Original inputs and ordering

Read-only `Installed_Game/RF.exe`, SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:

- World presentation4c5912–4c5996 selects projectile1d0 material, resolves its
  group with434da0, retries the same table's Default only for sample-1, then
  requests positional5056a0 at projectile1b4, volume1.
- Ordinary world collision reaches this presentation at4c53f2 after radial
  dispatch4c53a8. Other effects and special branches are unchanged/deferred.
- Physical shield handling calls presentation4c5820 at4c4f9b before shield
  durability processing4c4ff0–4c501b. Return4c5023 suppresses radial even on
  the breaking hit. Shield presentation is not the actor's Flesh contact.
- Entity dispatcher4c59f0 resolves the stored target first; unresolved object
  returns4c5a48–4c5a51. Nano interception4c5a90–4c5b5d returns before ordinary
  direct/radial/impact audio, including the hit that depletes Nano.
- Entity direct4c6132 and radial4c62f5 precede ordinary dry selection4c6463,
  same dry Default fallback4c64d9–4c64eb and positional playback4c6509.
  This entity branch does not call the world underwater predicate4c98b0.
- Ordinary expiry skips detonation for the actual Spit flags0 and retires;
  expiry never dispatches this terminal-contact adapter. Liquid passage is
  likewise separate from terminal kind1.

`tables.vpp/weapons.tbl`, offset1013760, size108890, independently declares
Small lines2072–2073 and Big2134–2135 as Default `Spit Hit`, Flesh `Impact
Flesh`. Neither row declares an underwater impact group. Both have direct
AP kind2, flags0 and flags2 no_fire_through. The previous Small/Big documents
retain their full mechanics, original placement and profile evidence.

`foley.tbl`, offset645120, size82283, lines1514–1518 maps `Spit Hit` to
Spit_Hit_01/02/03.wav, near7, gain1.0. Lines2502–2506 maps `Impact Flesh` to
Impact_Flesh_01/02/03.wav, near2, gain0.9. The shared playback service retains
those sample metadata; it does not replace Flesh by the firearm Gun Hit Flesh
group. `clutter.tbl`3269–3273 names riot_shield material `solid`; the adapter
reads the already-resolved class material rather than hardcoding Solid6.

The six authored `audio.vpp` entries are16-bit mono22050Hz:

- Spit_Hit_01.wav: offset12566528,76204 file bytes
- Spit_Hit_02.wav: offset12644352,104878 file bytes
- Spit_Hit_03.wav: offset12750848,48334 file bytes
- Impact_Flesh_01.wav: offset57200640,16522 file bytes
- Impact_Flesh_02.wav: offset57219072,17806 file bytes
- Impact_Flesh_03.wav: offset57237504,18946 file bytes

They total282690 retained file bytes, not just decoded PCM bytes. The bank
retains complete WAV storage, so file size is the relevant admission cost.
The unrelated Flesh04–08 files are not admitted by these authored groups.

## Bounded integration

Only two existing source files change. No new scene include, startup hook,
shared Laser callback, sweep, save schema, projectile payload or gameplay
resource gate is needed.

`scene_weapon_impact_audio.inc` preserves the conventional192KiB transaction,
then optionally appends the exact two Spit names using at most6 new sample
rows and288KiB, under the existing64-row capacity and256KiB shared-bank
headroom. Adding Spit directly to the old conventional allowlist would exceed
its192KiB cap and silence all ordinary firearm impacts; the separate append
avoids that regression. Existing conventional readiness remains kind1; Spit
kind2 cannot accidentally enter the NPC-bullet or audio-only-world-ray helper.

The append first scans already-loaded placed seeds for the exact class and
explicit primary pairs Rock Snake/Rock Snake Spit or Big Snake/Big Rock Snake
Spit, with the authored drools-slime flag. The case-folded comparison reuses
`scene_npc_loadout_equal`. No matching placed demand returns ready0/count0/
bytes0/statusRF_NOT_FOUND without incrementing an audio-error counter or
loading PCM. Thus levels with no paired Snake consumer retain their previous
optional-audio headroom. This stage precedes body/inventory construction;
it is authored selected-weapon demand, not a claim of live selected ownership,
visibility, activation or target acquisition. Empty inherited defaults and
later scripted weapon acquisition on an otherwise unqualified level do not
admit this optional feedback. The two actual original owners use explicit
matching primary overrides. No additional archive, parse or hook is needed.

The append reuses the parsed catalog, existing sample array, pin bookkeeping,
Foley RNG and positional voice service. It validates groups/ranges/nonlooping
metadata, deduplicates before sizing, counts only absent PCM as additional
bank bytes and loads before publishing any new pin or readiness. Failure
unloads only appended freshly loaded rows, truncates only its own pending
array tail and leaves conventional readiness, samples, pins and aggregate
admission counters unchanged. Preexisting resident rows are never unloaded.
Success joins the existing close-time pin restoration. The append depends on
successful conventional admission; it does not create an independent fallback
bank. On matching Snake levels it runs before the existing optional projectile/
Tankbot/Drone audio loads, so residency can reduce space for those later optional
consumers despite preserving256KiB headroom. Actual stock64MiB behavior is
unverified. No new lifetime owner or allocation is introduced beyond optional
WAV residency.

`scene_ai_spit_flight.inc` reads the already-selected terminal contact once:

- Static/mover face material through `scene_detached_contact_material` and
  the retained solid/face identity, without a second ray or sweep
- Qualified player/NPC body using its actual retained authored collision
  material; nonhuman NPCs are not assumed Flesh
- Clutter's registered retained owner and material, including immune contacts
- Live vehicle/turret full handle captured before damage, using the retained
  chassis/turret resource material when available
- Existing detached-piece batch/piece identity, with explicit Default because
  that public contact view does not expose its material
- Qualified physical shield with the shield class material before durability;
  stale, unowned, unselected or already-broken shields are silent

Missing material metadata is explicit Default-1. Invalid material indices
are rejected by optional audio only. The point is the retained surface point,
not projectile-center endpoint, target eye, shooter or a recomputed ray hit.
Identity/material qualification is captured before damage, so lethal contacts
still sound even when their callback retires the owner. Shooter death/switch
never changes the already-copied weapon/source/damage identity.

Direct contact is dispatched exactly once; the existing consumed output keeps
Nano silent and stops radial. Big's existing guarded radial dispatch then
runs exactly once, followed by ordinary contact audio. Small's zero radius
never calls it. Physical-shield presentation alone runs before durability,
then retains the same consumed/no-radial policy. Epoch checks prevent old
feedback or damage from continuing after successful timeline replacement.
No positive-damage requirement is introduced. Optional admission, missing
sample, invalid material or voice failure cannot reject gameplay.

The shared actual-contact function resolves one authored sample and its
sample-1 Default fallback, checks pin membership/residency and requests the
ordinary generation-qualified spatial voice. There is no allocation, archive
access or PCM reload in this consumer. Backend native voice allocation and
its existing failure policy remain unchanged. Repeated ticks do not repeat
feedback because the shared flight has already published terminal state.

## Precise liquid limitation

Factory4c7a97 zeroes projectile2f8;4c7aaa–4c7abd copies source actor810 bit
0x2000 into projectile bit1. First liquid contact4c4b8f distinguishes an
initially submerged exit (clear bit2) from initially dry entry (set bit2).
World/shield predicate4c98b0 requires bit2 and damage kind1/2; with Spit's
absent underwater groups, the original entry case resolves to silence.

The current shared flight retains only query_flags, whose1000 bit clears on
first passage. NPC eye-wet ownership is not implemented. This adapter thus
conservatively suppresses world/shield audio after any observed passage.
It matches initially dry entry, but also silences initially submerged exit,
where the original can still use dry groups. Before a first passage, original
bit2 is zero for either start and dry feedback remains supported. Entity
contacts always use dry groups as original4c6463 does. No victim wet flag,
camera, current shooter state or current-point classification is substituted
for the missing original launch state. Liquid movement, splash, lifetime and
radial behavior are untouched; full wet-start/exit parity stays deferred.

## Observability and handoff

`rf_scene_spit_impact_audio[4]` records optional append ready, new rows,
retained file bytes and status. Existing `rf_scene_weapon_impact_audio`
records contact/start/silent/error, actual weapon/material/group/sample/voice
and retained point. Successful total count/bytes include both admitted
families; conventional failure and Spit-only failure remain distinguishable.

Parent should update the prior Small/Big impact-audio exclusion and milestone,
apply both source files and this document, and run only its scheduled batch.
This slice does not claim listening parity, exact global sound RNG order,
acid-splash Vclip/scorch, detached-piece material fidelity or complete wet
ownership. No fixtures or generated gameplay/runtime files were created.
