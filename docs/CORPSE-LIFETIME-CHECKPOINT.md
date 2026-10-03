# Owned corpse lifetime continuation

Integrated on2026-10-03 with a successful NXDK Xbox build and focused
stock64MiB Xbox save/fresh-load continuation. This is the ordinary-save companion of the live
corpse fade and source-retirement adapters. It does not implement a second
fade renderer or timer.

The source constructor already keeps five eligible bodies and marks older
bodies fading for one second (`src/core/entity.c`,416940 reconstruction).
RFNC10 retains the death pose, but currently saves neither the body age nor
fade state. Its deferred reconstruction therefore starts new retention age and
can restart a disappearing body. `scene_corpse_lifetime_checkpoint.inc` carries
those missing facts and assigns them after all deferred body transfers.

## Wire and ownership

RFCL1 is the outermost companion of `RF_WORLD_VEHICLE`: RFCL1, then the existing
RFTU3/2/1, RFNS2/1, RFVA and vehicle bytes. Empty inner payload is valid. RFNC10
and existing nested formats remain byte-for-byte unchanged. Saves without
owned bodies keep their existing vehicle bytes; legacy saves have no RFCL and
retain their prior first-pass restoration policy.

The16-byte header is magicRFCL, version1, inner byte count and corpse count.
Rows follow the inner payload, in the source corpse linked-list order. Each
48-byte row contains UID0, class4, age seconds8, fade seconds12, flags29c16,
object flags20, corpse health24, current/class temperature28/32, motion36,
death action40 and reserved zero44. No address, model handle or pool slot is
persisted. Age is relative to the save clock and rebased to the restore clock;
list order preserves original equal-age retention ordering. Float values are
binary32, like their live owners.

The stage is1332 bytes, the fixed pending copy plus telemetry/scope is1368
resident bytes, capture uses1320 stack bytes, and application uses240 stack
bytes on x86. At most30 rows add1456 wire bytes. Admission is charged against
the ordinary load's existing2MiB budget. Reconstructed bodies use the existing
30-slot/96KiB owned-pose limits; this helper allocates no model or physics data.

Admission supports the current single-clip owned skeletal death, including
its active fade, protected retention bits and cooled temperature. Moving
corpse transforms/velocity, attached items, burn/emitter owners, negative-health
deletion, pending seek, timed/blended action tails and timer-wrap-crossing age
remain rejected. The loaded class/motion/resource identity is checked against
the separately admitted RFNC row. Unsupported state is not silently omitted.

## Integrated engine hooks

1. Include the new file after `scene_campaign_history_checkpoint.inc` and
   before `scene_world_snapshot.inc`/`scene_world_load.inc`. Add early static
   prototypes for reset, apply(int32_t), scope(uint32_t) and
   fading(const rf_corpse_owned*). Reset in fresh `campaign_live_corpses_open`.
2. In `scene_corpse_checkpoint_death_capture`, change the owned-corpse compound
   reject from `flags_29c&9u` to `flags_29c&8u`; immediately after that reject
   succeeds, if `flags_29c&1u`, require
   `scene_corpse_lifetime_checkpoint_fading(owner)==RF_OK`, preserving the
   existing failure path. Do not relax any other flags/effects/pose guards.
   The helper rejects this extra support outside ordinary world capture.
3. In `scene_world_snapshot`, bracket **only** its RFNC export call with
   `scene_corpse_lifetime_checkpoint_scope(1)`/`scope(0)`. Reset the scope before
   `SNAP_END` can jump on errors. At the end of successful vehicle wrapping,
   before `SNAP_END(RF_WORLD_VEHICLE)`, wrap once more with
   `scene_corpse_lifetime_checkpoint_capture(buffer+at,capacity,bytes,now,&wrapped)`.
   A failed companion capture rejects the entire save.
4. In `scene_world_load`, declare `scene_corpse_lifetime_stage *corpses=NULL`.
   Immediately after outer world decode, extract vehicle slice and call
   `scene_corpse_lifetime_checkpoint_prepare(data,bytes,budget-cost,&corpses,
   &vehicle_data,&vehicle_bytes)`. Add allocated_bytes to cost with the usual
   bounds check. Retain this unwrapped data for the existing RFTU/RFNS loaders;
   remove their later reassignment from the original envelope vehicle slice.
5. After initial RFNC decode, before resources admission/free(rows), call
   `scene_corpse_lifetime_checkpoint_admit(corpses,rows,rows_count)`. This is a
   read-only UID/class/dead-pose/motion join. Existing world/NPC semantic and
   placement stages still run unchanged. No new mutation during admission.
6. In the existing assignment-only transaction, after NPC assignment and
   before returning to ordinary ticks, call
   `scene_corpse_lifetime_checkpoint_assign(corpses)`. This copies only admitted
   scalars; NULL clears a stale pending batch for legacy loads. Close the heap
   stage at the loader's common done label.
7. In `campaign_live_corpses_tick`, immediately after
   `scene_corpse_checkpoint_tick()` has completed **all** deferred creations,
   call `scene_corpse_lifetime_checkpoint_apply(now)` and return its error
   before any per-corpse update. Application validates every restored UID and
   model/motion owner before changing any timer/list. It restores list order
   and rebases ages together, overriding transient constructor retention
   choices. It never invokes damage, death, audio, loot or animation seek.

If deferred presentation allocation fails, apply returns a real error rather
than silently inventing a new fade duration. This follows the existing staged
presentation restore boundary; body allocation still occurs after world
publication. Full transactional reservation of all presentation resources is
not added by this slice. The source-retirement adapter is required so the body
that expires later remains a valid retired actor in the next RFNC save.

## Focused parent-run fixture

`python tools/xemu_corpse_fade_save.py` reuses the existing six copied authored
miner8432 records in emptyCTF06. One ordinary Slay creates six owned bodies;
source92frames saves the oldest during its existing one-second fade, following
an ordinary one-second Slay delay that lets the player settle first. A fresh
120-frame load samples the early restored opacity, checks the exact saved
age/fade bits were applied, verifies no death/audio/drop replay, and checks
exactly that body retires. Its endpoint ordinary save must contain one retired
RFNC identity plus the five remaining RFCL bodies, with no stale fading row.
Disc inputs are restored in finally, with the normal restoration build.

No host input, images, PC runtime or campaign traversal. The harness checks
immutable save bytes and live guest owners; endpoint counters are interpreted
separately from the normal teardown that releases all remaining bodies.

## Xbox result (2026-10-03)

PASS: `artifacts/xemu/corpse-fade-save-20261003-132725/report.json`.
Source92frames saved six owned bodies with oldest UID913100 age0.516seconds
and fade0.466667seconds. Fresh load120frames restored the exact saved age/fade
bits and six model/pose owners; frame11 observed decreasing alpha76. Exactly
that oldest body expired, unregistered its source and released its physics.
The endpoint ordinary save contains its retired RFNC identity and the other
five RFCL owners, with no replayed death, Slay, loot or audio events.

The source save is22164bytes; endpoint21996bytes. Load staging uses842984bytes
and the minimum endpoint has3126 free pages (12.21MiB). Normal level teardown
leaves no owned body/model resources. Build and original disc inputs restored.
No visual appearance, audio playback, burn/carried bodies, timer wrap or moving
corpse physics claim is made; native alpha submission is functional evidence.

The first source32-frame attempt (`corpse-fade-save-20261003-132401`) rejected
at the player-save gate before reaching NPC/RFCL capture. The corrected fixture
uses the ordinary one-second Slay delay, allowing the falling CTF06 spawn to
settle while still capturing the corpse during its existing one-second fade.
No runtime save guard was weakened. The passing run predates the additional
explicit landing telemetry sample; ordinary player capture itself requires
landing1. Future harness runs also sample that field directly.
