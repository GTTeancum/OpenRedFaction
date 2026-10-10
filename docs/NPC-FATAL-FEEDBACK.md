# Ordinary NPC class-fatal feedback

Source-written against `bcb5e4a9ee5b62c90f09e6fc7ffd76b304b350d6`, independently
source-reviewed and parent-integrated for the 13:00 hourly batch.
The added branches are uncompiled and runtime/audio-unverified. No builds,
tests, fixtures, emulator runs, worktrees or campaign routes were performed.
The scheduled 13:00 old-save regression cannot establish new fatal audio.

## Concrete missing consumer and original evidence

Installed miner1 declares `$DeathSnd: "Player Die"` at entity.tbl line 714.
Foley group 20 (foley.tbl lines 163–170) selects Player_Die_01.wav through
Player_Die_06.wav; all six original audio.vpp entries exist. Concrete placed
owners read from the already-used original archives are L3S1 UID789 (12mm
handgun) and L11S3 UID10637 (none). These are source facts, not new encounters.

The selected `death_generic` action instead maps ult2_Death_Generic.mvf to
`Human Death Generic 1` (entity.tbl line 750), Foley group 148 and
HumanDeath_Generic.wav (foley.tbl lines 1123–1124). The existing retained
motion/action-sound path requests that distinct binding at published actor
position. It does not consume class group 20. This resource distinction is
not a claim about the audible contents of either waveform.

L3S1 UID109 and L11S3 UID10493 are guard1; L11S3 UID10636 is eos. Neither class
has a DeathSnd declaration. guard1's Low/Med groups resolve to 115/116. The
Eos Small Pain / Eos Large Pain names do not resolve in installed foley.tbl;
its fallback therefore remains silent. No death descriptor is fabricated.

Original RF.exe boundaries, already reconstructed by the shared core:

- 41a51f–41a548 requests 4196f0 only when incoming damage / base-class health
  exceeds binary64 .001 and kind is not 10. The existing core notification
  remains the sole admission point; there is no unconditional finalizer call.
- 419706–419736 tests base class294->124 against -1, then resolves the
  effective class29c->124. 422809–422819 assigns the same class unless a
  replacement class is supplied. Current registered NPCs have the same
  base/effective class; this adapter does not infer future replacement policy.
- 419715 and 419756–419761 gate and then set actor810 bit4 for one attempted
  descriptor playback, including an absent sample. Position is actor7d4 at
  41973f–419751. The scene pre-latches before callbacks to prevent reentry,
  passing a copied pre-latch state to the shared core.
- 41cb53 stores an omitted descriptor as -1 (ESI initialized at41c45e).
  41976a onward then retains the ordinary predicate, action1/17, 1000ms timer,
  fraction-above-binary64-.3 Low/Med selection and existing808-voice gates.

`rf_entity_damage_sound` and `rf_entity_damage_sound_groups_read` already own
those branch and parser semantics. This slice changes neither shared helper.
See DAMAGE.md and CAMERA.md's player lethal-damage sound section for the
existing historical reconstruction checks, which are not tests of this slice.

## Bounded integration and ownership

`scene_npc_fatal_audio.inc` retains an optional class DeathSnd group array of
at most 640 int32 entries (2,560 bytes). It reuses parser3 while leaving the
mandatory two-group Low/Med allocation, parser failure behavior, metadata
hash and living wrapper unchanged. Allocation/parser failure is cosmetic.
An unavailable row is -2 and skips new feedback; a successfully parsed absent
or unknown descriptor is -1 and enters the original no-descriptor fallback.
This distinction prevents failed optional metadata from inventing fallback.

The additional metadata is included in Foley resident/peak accounting and
freed on scene closure. Existing Foley sample declarations, bounded bank,
idle eviction, spatial playback and the caller's combat pain RNG are reused.
No PCM preload, bank enlargement, new save record or voice owner is added.
Fatal missing samples, exhausted admission or playback errors are counted and
never enter `combat_feedback.status` or prevent lethal gameplay/drop handling.

`combat_notify` dispatches nonpositive-health RF_DAMAGE_PAIN_SOUND to exactly
one route: existing admitted Drone/Tankbot/Spike profiles use their unchanged
robot adapter; other registered NPCs use the new ordinary adapter. Nonfatal
pain and selected-action Foley remain unchanged. One ordinary lethal event
can legitimately request both class feedback and an authored action sound.

The ordinary adapter verifies the captured array slot/pointer, UID and class,
full registry handle, registration/view/effects identities and fatal health.
It copies action, voice, old deadline, eye position, groups and flags before
audio. An explicit descriptor attempts once using bit4 in both view/effects
owners, set before callbacks; the local core flags are never copied back.
An absent descriptor does not consume bit4. Its resolve shim publishes only
the core-computed deadline before optional resolution/playback, after checking
identity and the captured prior deadline. This preserves missing-sample and
already-playing timer behavior without overwriting later callback changes.
No retained NPC pointer is dereferenced after optional audio callbacks.

This is local adapter safety, not a new permission to destroy/replace an
outer damage-dispatch owner during its existing synchronous callback contract.
The broader damage/backend lifecycle remains unchanged. No asynchronous sound
receipt, owner field, timeline state or new checkpoint wire format is introduced.
Existing terminal `death_flags_810` capture/restore already carries bit4 in
both flag owners. Fatal fallback deadline/voice remain the existing
presentation state; this slice adds no promise of active voice continuation.

## Observability and remaining verification

`rf_scene_npc_fatal_audio[12]` records classes, retained bytes, binding hash,
metadata failures, admitted notifications, explicit descriptor attempts,
fallback selections, quiet/repeated requests, errors, last UID, full handle
and descriptor group. Shared pain audio counters continue to record actual
resolution, load and playback requests. These counters are source-written;
no new exporter, fixture or runtime assertion was added.

Required future evidence is focused ordinary fatal dispatch, descriptor
oncebit and fallback deadline behavior, plus optional-error/nonfatal/robot
regression checks at the parent's authorized validation cadence. The 13:00
old-save test is only existing checkpoint/startup regression coverage. It is
not proof of miner1 voice playback, guard fallback, waveform audibility or
new fatal checkpoint behavior.
