# Authored NPC landing audio

Source-written against e08f5ce162e8b3c2104308a79570ee14e3445fb9 on 2026-10-10.
Independently source-reviewed and parent-integrated for the 12:00 UTC
parent-owned Xbox batch. No compilation, tests, fixtures, campaign route,
emulator execution or audible-success claim accompanies this change.

## Original evidence

Read-only disassembly of Installed_Game/RF.exe confirms:

- 419839..419868 gates audio on contact velocity Y minus incoming body velocity Y
  strictly greater than 0.25, OR full incoming XYZ speed strictly greater than
  0.5. The referenced binary32 constants at 5893d4/5893c0 are 0.25/0.5.
- 41986e..419891 uses slot zero for negative material, otherwise only selects
  the material's slot when its group ID is positive. Default slot zero itself
  need not contain a positive group ID.
- 419893..4198ae overrides with water slot four only for actor flag 0x1000 and
  a positive water group ID.
- 4198b9..4198ca calls 434d00(group, 0). Disassembly at 434d00..434d36 confirms
  bounded first-sample selection and -1 for invalid group/index. No random draw.
- 4198d2..4198e2 passes copied published actor position (+3c), gain one and pan
  zero to 48a9c0. Its non-local-player branch 48aa06..48aa1c calls positional
  5056a0 with gain one. The player-only landed byte at 4198e7..4198fa follows.
- Original landing velocity rebasing and stance effects start at 419901,
  after sound. This patch does not call the complete rf_entity_land_process:
  the scene already owns support acceptance, impact damage and stance changes.

Installed tables.vpp/entity.tbl declares Capek at line 588 and its Default,
Metal, Solid and Water Land groups at lines 624..627. Foley lines 129..143
resolve them respectively to jumpland_01/02/03/04.wav, each near distance eight
and authored gain 0.5. Original audio.vpp file sizes are 6,554, 8,818, 10,464
and 44,226 bytes. These declarations already exist in the shared Foley bank.
The patch does not declare replacements or preload additional PCM.

The source-owned practical path is the actual Capek consuming Nano shield
break in scene_capek_shield_movement.inc, which calls rf_scene_npc_fall. The
ordinary script/idle ground path can then reach campaign_npc_land and this
consumer when that owner survives landing. This is source reachability, not
an observed shield-break fall or an audible runtime result.

## Integration and ownership

The new include defines optional class mappings, a call-local receipt and a
one-shot audio consumer. Only scene.c and this include change executable code.

1. Audio initialization reuses the already-read entity.tbl scratch data and
   shared rf_entity_land_sound_groups_read parser. Each ten-slot row is either
   fully accepted or all absent. Unknown/malformed groups and allocation
   refusal suppress optional audio without rejecting level/gameplay loading.
2. campaign_npc_land captures incoming XYZ before the existing support commit.
   After successful support contact and collision publication, it copies the
   accepted support velocity, support handle/material, water bit and position.
   Existing impact damage, lethal retirement and Normal/Slow calls remain in
   their original order, with unchanged arguments and error results.
3. Only successful surviving final position publication marks the receipt
   committed. Identity qualification checks the original array pointer/slot,
   full registry handle and registered view, damage handle, authored UID/class,
   body allocation, living/nonremoved NPC flags, accepted support and position.
   Audio cannot create a second impact path or mutate movement/support/stance.
4. campaign_script_step owns one stack receipt and passes it down both ground
   functions. The preceding actor's receipt is consumed at the next loop
   boundary, including the terminal boundary, before acquiring another owner
   pointer. Therefore the new native callbacks never run before the landing
   callers resume their existing actor writes. Every early continue reaches
   this boundary. A gameplay error discards any pending optional audio.
5. Playback uses the captured published landing position, even if the actor's
   later ordinary scripted locomotion advanced it in the same iteration.
   It requalifies exact live ownership around native playback, and after
   optional callback-free PCM admission. If the bank epoch changes, the pass ends successfully
   before borrowing another actor. Actor counts/pointers are rechecked there.
6. PCM admission uses existing rf_audio_bank_reload directly with the fixed
   bank budget. A full bank leaves this optional one-shot silent. The consumer
   deliberately avoids the shared idle-release callback path, which is not
   safe for arbitrary reentrant whole-bank resets. Successful lazy loads enter
   the existing loaded-byte accounting and eviction eligibility; this consumer
   does not evict other samples or change resource budgets.
7. The spatial one-shot uses existing mixer allocation, shared sample gain,
   listener spatialization and native play_mode(looping=0). It captures the
   full mixer/native handle before calling the backend; there is no public
   voice ID, persistent actor voice or follow-up poll needed for a one-shot.
   A failed/stale start stops its retained full native handle in the same
   bank epoch using the unchanged backend/context that accepted the voice;
   software/spatial cleanup independently qualifies each full generation.
   Epoch changes leave retirement to the existing lifecycle reset, avoiding
   a stale stop against a numerically reused voice. Stop callbacks are followed
   by another epoch check. A native backend without stop support is refused. Successful voices finish under existing audio life.

## Bounds and lifecycle

- Optional retained mappings use 40 bytes per admitted class, bounded by the
  existing 640-class admission limit: at most 25,600 bytes. Their bytes enter
  the existing Foley resident/peak accounting while entity scratch is live.
- One stack receipt is reused for the script pass; there is no heap/global
  receipt queue and no per-actor persistent field. Small global diagnostics
  add 124 bytes. The mapping pointer/count/epoch adds 12 bytes on Xbox.
- No increase to the 1,280-KiB audio-bank budget, bank capacity, native voice
  count or PCM cache budget. Lazy sample refusal and voice exhaustion are
  optional; they cannot fail an otherwise accepted landing.
- Bank open/reset clears mappings and diagnostics. Existing scene teardown
  releases mappings alongside the other Foley class maps. The monotonic
  runtime-only bank epoch invalidates receipts; no save format/hash changes.
- No player-code changes, grant/inventory paths, movement changes, timers,
  looping samples, retries, RNG draws, tests or route fixtures are introduced.

## Explicit remaining approximations

- The original requests audio before its velocity/stance effects. This scene
  intentionally preserves its existing support/damage/stance/publication
  sequence and emits after that actor's iteration. Lethal and failed landings
  stay silent. Exact original side-effect timing is not claimed.
- The speed gate uses captured pre-support incoming velocity and the actual
  accepted support point velocity. Rotating/passive support retains the
  existing scene's point-velocity semantics instead of replacing them with
  original contact-center velocity. Ordinary carry/rebase behavior is untouched.
- Water selection consumes the already-owned actor flag 0x1000; this work does
  not add NPC water/swim classification or infer water from the player/room.
- Existing gameplay impact callbacks and callers retain their prior ownership
  assumptions. The new callbacks are placed after those callers finish using
  their actor. This is not a general reentrancy rewrite of gameplay or audio.
- Bounded optional table parsing follows the existing shared parser contract,
  not full original table-parser equivalence. Loop-marked samples are refused
  instead of creating an unowned loop. No audible, memory-pressure or action
  runtime verification has been performed.
