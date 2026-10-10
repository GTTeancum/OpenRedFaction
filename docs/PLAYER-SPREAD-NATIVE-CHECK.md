# Optional original Pistol spread observation

Integrated after parent source review on October 10, 2026. The gameplay
consumer was committed at `17bf1b0ec53393d693b7a564ab16f96ea6c5ae8f`.
The parent owns the scheduled 07:00 Xbox build/batch and cleanup. No syntax
check, compilation, test, validator, recipe or emulator execution was performed
for this observer. There is no new runtime result yet.

## Deliverable and admission

- `tools/xemu_original_auto_reload.py`: integrated opt-in extension of the existing helper.
- Opt in with `--observe-primary-spread` alongside the existing required
  `--parent-hourly-batch`, clean successful `--consumer-build`, and new external
  `--out` evidence directory. No additional guest selector or input is added.
- No flag: previous symbols, predicates, limits, recipe and
  `PASS_ORIGINAL_AUTOMATIC_RELOAD` status are preserved.
- With the flag: success is `PASS_ORIGINAL_PISTOL_SPREAD`. This requires every
  original initial/final predicate and five additional predicates. Restoration
  failures still override success with `CHECK_FAILED`.

The recipe stays exactly original L3S1, original unchanged spawn/basis, 780
frames, one-frame primary presses at 120,150,...,570, all other fields zero,
then release. The recording generator, allowed selectors, archive checks,
manifest/source pins, single-attempt/session exclusion, stock 64 MiB,
terminal phase 5 / frame 780 / completed replay, ammo, selection, pickup,
grant, death, save/load, transition and full restoration checks are unchanged.
No parent run may substitute a route, grant, altered inventory, fixture or
forced event if these predicates fail. Preserve the failed result and stop.

The October 9 23:00 raw evidence, report, compact proof and expectations remain
untouched. This optional observation is a fresh check against a fresh build;
that earlier run predates the new primary spread consumer.

## Actual observations

The only additional exact linker symbols are:

- `campaign_conventional_random`, one uint32 word, in the existing paused early
  observation and existing terminal snapshot.
- `rf_scene_player_form`, eight uint32 words, at those same observations.

No new probe or per-frame observer is added. The existing early probe reads the
actual guest diagnostic first and rejects unless phase is 2 and
30 <= frame < 120. A missed or late probe fails explicitly; seed 1 is never
inferred from a later state. It then requires actual RNG `[1]`, no active or
historical form data, and the already-read inventory ownership prefix to be
exactly one owned byte for weapon ID 3 (Pistol), all other 63 bytes zero.
The existing checks separately prove selected slot 0, Pistol ID 3, 16 loaded,
125 reserve and zero initial shots.

At terminal, actual RNG must equal the host-side arithmetic expectation:

    state = 1
    repeat 32 times: state = (state * 214013 + 2531011) modulo 2^32

The helper computes only this integer recurrence. It does not execute guest
code, invoke the sampler, create a save, or fabricate an observation. The raw
initial/final words remain in the original observation/result files and are
also summarized with the initial frame, expected state and draw count.

Terminal form words must still all be zero. This closes a real attribution
gap: form entry can select Undercover slot 16 outside the ordinary selection
change counter. The initial-only-Pistol ownership check, retained zero form
history, no-grant/no-pickup/no-recovery guards and unchanged selected-slot /
selection-change checks exclude other conventional-stream weapon consumers.
The original exact 16 actual shots, 16-round automatic reload, final Pistol
16 loaded / 109 reserve and ammo conservation remain mandatory.

## Source evidence for attribution and lifetime

At the reviewed commit, a complete text-reference scan across source headers,
C and included scene files finds these uses of `campaign_conventional_random`:

1. `src/diagnostic/scene.c:2436`: independent static `rf_random_state` owner.
   `include/rf/random.h:6` defines that owner as one uint32 value.
2. `scene.c:15852`: frame-zero initialization to 1 before ordinary import.
3. `scene.c:16169..16175`: new slot 0, slot 16 and non-alternate slot 1 spread
   call, after actual debit/accepted-shot handling and before contacts.
4. `scene.c:16176`: existing slots 13..15 conventional spread call. The
   conditions are disjoint. Those weapons are not owned/selected in this
   accepted recipe. NPC, mounted, sound, effect, Shotgun and AR-alternate
   paths use other owners and do not receive this RNG's address.
5. `scene_weapon_modes_checkpoint_live.inc:56`: RFWM capture reads the value.
6. That file's line 80: RFWM restore assigns it. The existing no-save/load
   predicate, zero storage/checkpoint state and no transition/death exclude
   restore or a new frame-zero reset during the accepted run.

`rf_weapon_spread_ray` in `src/core/weapon.c:1079..1091` leaves zero-angle
calls draw-free, and for a successful positive angle commits the result of
one `rf_particle_cone_oriented` call. `src/core/effect.c:39..75` shows that
oriented sampling invokes `rf_particle_cone_sample` exactly once, which calls
`rf_random_next` exactly twice. `src/core/random.c:2..7` supplies the integer
recurrence above. The authored Pistol primary is 0.5 degrees; the selected
definition and its field are used by the integrated scene consumer. The
ordinary non-Shotgun pellet loop has one iteration per accepted shot.

The stream is not a member of heap-owned `scene_stream`. Its address escapes
only to the reviewed spread helper, which retains no pointer. The terminal
`scene_miner` cleanup releases scene and other owners but has no RNG assignment
or clear. Therefore the existing phase-5 snapshot can read the actual final
value; no pre-terminal substitute is required.

`rf_scene_player_form` is likewise retained diagnostic storage. Its reset
sites are scene initialization, death recovery, or world load. Form
entry/exit increment indexes 3 and 4 in `campaign_set_player_form`;
`include/rf/animation_check.h` exposes its placement use as a const pointer.
Terminal cleanup does not clear these counters. Thus checking them at phase 5
does not silently lose a transient form excursion.

## Meaning of a future pass

A pass establishes that the actual source-owned conventional stream starts
at 1 and ends at the state for 32 draws alongside 16 actual ordinary Pistol
shots, with excluded competing consumers and all original run guards passing.
This is proportional runtime evidence for the new Pistol spread consumer.

It does not establish a particular ray, impact, hit/miss or damage result,
retail spread distribution/basis/RNG parity, AR burst or Undercover firing,
suppressed-mode behavior, audio/visual output, or save/restore continuation.
RFWM already stores this same owner (`weapon_modes_checkpoint.c`, byte +24),
but this run intentionally creates and restores no save. The automatic reload
regression remains covered by its unchanged predicates.

Review these source ownership assumptions again if the parent changes RNG,
form lifetime, selection, grant or save ownership before the batch. Missing or
ambiguous exact symbols, a late initial observation, or any failed predicate
means this recipe does not prove the consumer; do not relax expectations to
obtain a pass.
