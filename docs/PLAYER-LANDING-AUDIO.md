# Player landing audio

Source-written 2026-10-09 for the parent-owned 05:00 Xbox batch. No compile,
unit test, emulator run, native capture, or audible-success claim was made by
this helper. The implementation is an isolated scene include; parent scene
wiring is supplied separately from the source commit. It depends on the shared
`rf_entity_land_sound_groups_read` parser owned by the jump-audio change.

## Original evidence

`rf_entity_land_process` in `src/core/entity.c` already reconstructs the complete
419830 ordering with explicit callback boundaries. The prior verification and
its limits are recorded in `docs/DEATH-LIFECYCLE.md` under “Complete landing
ordering at effect boundaries.” This change does not rerun that verifier or
claim that its prior results verify the new scene wiring.

Read-only disassembly of the installed RF.exe gives the audio sequence:

- 419839..419868: request a landing sound if contact velocity Y minus body
  velocity Y is strictly greater than 0.25, or the full XYZ speed is strictly
  greater than 0.5.
- 41986e..419891: a negative material uses slot 0. A nonnegative material uses
  its material slot only if that group ID is positive; otherwise it uses slot 0.
- 419893..4198ae: actor flag 0x1000 selects water slot 4 only when its group ID
  is positive. Slot 0 itself can have a zero or negative ID.
- 4198b9..4198ca: pass group and sample index 0 to 434d00. That helper checks
  group and sample bounds, returns the first retained sample ID, or returns -1.
  It does not select a random sample or consume RNG.
- 4198d2..4198e2: pass published position, gain 1 and pan 0 to 48a9c0. Its
  local first-person branch goes through 505560; the other branch is spatial.
- 4198e7..419901: the original player landed byte is a subsequent effect.
  Existing scene landing state remains unchanged by this audio adapter.

Read-only inspection of installed tables.vpp confirms all three player forms,
miner1, parker_suit and parker_sci, declare `$LandSnd:` for Default Land, Metal
Land, Solid Land, Water Land and Glass Land. Their foley.tbl samples are
jumpland_01.wav, jumpland_02.wav, jumpland_03.wav, jumpland_04.wav and
jumpland_06.wav, respectively, all with near distance 8 and authored gain 0.5.
The three complete ten-slot mappings are prepared from the existing entity
scratch input at audio initialization; form changes select the retained form.
No archive read is added to the movement loop. Missing/unknown bindings and
malformed-input rejection follow the shared bounded-port parser contract,
not a claim of full original entity-table parser equivalence.

## Scene integration and ownership

The scene currently handles support acceptance and velocity rebasing itself.
Calling the whole `rf_entity_land_process` again would duplicate existing
physics and stance effects. The new include therefore adapts only the proven
audio gate and material selection; it never modifies body velocity, movement,
flags, support ownership, pose, or jump acceptance.

The parent integration patch:

1. Opens an audio observation scope in the actual player-stance callback, once
   per live frame. Registered on-foot ownership, positive health, noncinematic
   view and relocation suppression are required. Spawn/frame zero is silent.
2. Observes the first walkable mode-3 collision immediately before the live
   `actor_contact` response. This preserves its incoming velocity before the
   response clips it. Wall/ceiling contacts and synthetic routes cannot arm it.
3. Saves a fallback incoming velocity before the ordinary or stance landing
   support commit. Only a successful mode-3 to mode-1 transition followed by
   body/pose publication requests audio. A ground query alone never does so.
4. Uses the accepted support velocity after the existing commit, including
   passive-body support-point velocity. It prefers the captured incoming
   walkable-contact velocity; contact-free support acceptance uses the saved
   pre-support velocity. Later stationary response passes cannot overwrite the
   first incoming impact.
5. Cancels the scope at the scene-frame boundary. The existing common
   `scene_player_impact_relocated` callback also cancels it and suppresses the
   next scope, covering its current respawn, teleport, world-load and vehicle
   placement callers. No audio is attached directly to shared support helpers,
   restoration routines or probe paths.

The existing `rf_player_sound_route` and `campaign_sound_start` retain PCM
admission, budgeted eviction, authored gain, real native-device playback and
voice lifetime. There is no second mixer, PCM bank, artificial audio event or
new RNG owner. Missing samples and exhausted audio resources do not undo or
fail a successful gameplay landing. The local player is the scope of this
change; NPC landing and the original landed-byte/weapon response remain
separate work. This live scope is a bounded port adapter, not complete original
frame scheduling or callback-order equivalence.

## Compact observation surface

`rf_scene_player_landing_audio[16]` contains scopes, eligible scopes,
precontacts, committed landings, gated requests, successful starts, quiet
results, skipped commits, failures, form, material, group, sample, voice,
status, and whether the last commit used preimpact capture.

`rf_scene_player_landing_values[8]` contains incoming XYZ, accepted support XYZ,
full speed and relative Y. `rf_scene_player_landing_sample[64]` holds the actual
selected audio-bank filename. `rf_scene_player_landing_events[16][12]` records
only successful real landing-commit notifications, including quiet ones:
frame, prior mode 3, committed mode 1, material, group, sample, voice, status,
preimpact flag, speed bits, relative-Y bits and retained stance flags. Entries
wrap in commit order; the commit counter identifies how many are valid.
A compact `PLAYER_LAND_SOUND` log names each of the first 16 gated requests
and the first four failures. These describe actual playback requests, not
proof that a native recording contains audible output.

The retained state plus telemetry is approximately 1.1 KiB, with 120 bytes of
load-time mapping scratch and no new retained allocation. Original landing
samples are admitted by the existing bounded bank on demand.

## Parent validation still required

Use the existing ordinary jump/native-audio replay in the consolidated batch.
Observe real jump acceptance, airborne movement, one 3-to-1 commit, preserved
preimpact velocity when collision response has run, selected material/group
and a successful original sample start. Correlate the original landing sample
with native output before making an audible-success claim. Spawn/restore and
neutral grounded frames should not add landing starts. Moving-support,
water-override and other player-form runtime coverage remains unverified;
no new per-slice fixture or separate runtime suite is requested.
