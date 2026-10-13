# Nano-shield hit and depletion presentation

Source implementation on published `6d1bf09b54625cade292445139cd9da0779634d4`.
The parent confirmed its newer `b691aa23` has constant shield and Cane warmup,
but none of these hit consumers. This branch adds no save fields or gameplay
policy. No compile, syntax check, harness, test, runtime session or image was
run; the parent owns integration, review and the sole Xbox build.

## Original evidence and installed demand

Static disassembly of installed RF.exe and direct named-entry metadata reads
from installed VPPs were used. The established executable identity is SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
No original execution or visual reference was used. Prior retained research
under `docs/research/secondary-re/campaign-capek-*` supplied the detailed PART
math; those old reports/scripts were read, never executed or modified.

| Address | Consumer retained |
| --- | --- |
|422b31..422b8d|Small/Big/Destroy vclip lookup and shared definitions.|
|42cca0|Existing Nano class/positive armor/disable-bit admission stays authoritative.|
|4c5a90..4c5b1f|Existing adjusted amount: rail100, remote0, grenade instance flag10; Small below100, Big otherwise. Invulnerability skips debit but not feedback.|
|4c5b1f..4c5b4c|Position is actor3c; direction is accepted W1b4 contact minus actor3c, through409fa0.|
|489050..489102|Radial feedback always Big after existing overlap; invulnerability still emits. Direction is objecte4 physics position MINUS blast origin, while cue origin remains actor3c. This differs from direct contact and from the port overlap delta.|
|427df0..427e70|emit_break1 requests Destroy; later selected Hit remains independent. OFF/restore emit_break0 do not request it.|
|4c16e0..4c18b6|Room gate,434da0 Foley selection/5056a0 positional attempt, then free visual slot; no audio handle is retained by the visual.|
|4c1809..4c1861|Destroy/null direction uses identity basis; directed hits use4fcea0, shared as rf_entity_navigation_basis.|
|4c1d50|Retire completed owners before advancing survivors; offscreen owners still advance.|
|4c1e10/4c1dd0|Captured-room draw of captured world position/basis.|
|54ccf8..54cd78|Simulation seconds times15; header5 for Small/Big,29 for Destroy. Six/30 mesh samples are not six/30-frame playback durations.|
|5420f0/540bf0/540e80|Selected Destroy PART decode, left-held birth-rate integration, pre-scan integer demand consumption, shape1 six-draw births.|
|542e90/5414d0/557460|Particle age/motion/drain and lifetime envelopes; renderer never advances them.|

Installed entity.tbl marks Capek and masako as Nano classes. Existing source
inventory covers two authored actors at most per campaign level (L8S4, L9S1,
L9S2, L11S3, L20S2); the reconstructed extra Masako also fits two in L20S2.
Admission uses actual class flag02000000, not names/UIDs. Hit concurrency is
bounded independently: eight Small/Big slots and two Destroy slots, as approved.
These are port limits, not the original128-slot general effect pool.

Installed vclip.tbl defines Nano Shield Small Hit, Nano Shield Big Hit and
Nano Shield Destroy with corresponding NanoShieldHitSmall/Big/Destroy.vfx,
radius1 and Foley `Shield Hit`. foley.tbl supplies ShieldHit_01/02/03.wav,
near10, volume0.9. Existing bank registration/metadata supplies actual spatial
parameters; the selected bluebeard rows are nonlooping with Keyoff0.

## Contact and lifetime integration

Every existing scene_nano_weapon_contact caller now passes its already accepted
body point, bullet_point, projectile receipt or ray/fraction-derived body point.
NPC melee and delayed strikes receive an optional point output from their
existing campaign_enemy_melee_actor_contact query. No collision is repeated,
no target/contact is fabricated, and no payment, damage or admission changes.
The earlier pure point derivation moves ahead of the Nano early return where
necessary. The existing coarse-body/ray policies are not promoted to exact
original mesh collision parity by this presentation hook.

The admitted contact records `depleted` only after scene_capek_shield_break
succeeds, then publishes the existing armor result and dispatches presentation.
Masako also receives Destroy because the receipt belongs to the armor transition,
not the Capek-only fall helper. Invulnerable radial/direct branches emit their
Hit with no debit. No post-hit armor predicate can suppress the final Hit.
Destroy is attempted before Small/Big even when either visual pool is full.
Zero-length/nonfinite directions suppress only directed imagery; sound and
an independently valid Destroy request remain eligible, with a diagnostic.

Each cue resolves a room from the current accepted actor origin, then copies
position/basis. No actor/pose/contact pointer survives the call. Actor removal
therefore does not cancel an already emitted world cue. A scene epoch prevents
publication into a replacement scene after an external audio callback.

Sound uses the existing impact_sound_random, rf_audio_group_choose and positional
campaign_sound_play services, once before visual admission. PART births use that
same existing presentation stream, never gameplay/weapon-spread RNG or a new
per-cue seed. No optional effect owns a voice handle or stops a voice at visual
completion. A full visual pool loses the visual request without audio retry.
Admission failure is optional and reported; no substituted image or invented hum.

All meshes, material banks, deduplicated textures, instance buffers, two100-node
PART arrays and30x13 authored tracks are prepared once. Simulation updates mesh
samples and particle state once per frame; drawing only reads them. Zero/paused
updates do not integrate duplicate fractional frames. Original retire-before-
advance ordering is retained. New cue frame0 is shown before its first advance.
The private particles continue to age after the header duration until empty.

Destroy's selected flags10/shape1/no auxiliaries use an authored sphere shell,
signed speed, captured identity transform, randomized initial age, half-second
life and six RNG draws per birth (including the zero-width lifetime draw).
Only rate tracks3 and4 are positive. Integration preserves per-span carry stores
and drops unfilled integer demand; it does not rebuild culled/offscreen particles.
Tracks have identical first12 fields and are explicitly validated before using
that selected-resource simplification. Nonmatching required PART data refuses
optional visual admission. No shared particle or renderer capacity is enlarged.

The retained meshes use existing keyed/morph/UV evaluation, sphere face culling,
beam/billboard projection, authored opacity and additive material submission.
PART lifetime size/alpha envelopes use(0,.5), white brightness floor and original
truncating alpha conversion. Mesh alpha follows the existing scene mesh adapter.
Queue entries borrow the normal resolved particle camera and2048-record workspace.
Overflow drops submissions. Sink errors propagate to the parent renderer fence.
Exact original effect-list ordering, per-face depth sorting, x87 rounding and
pixel parity are not claimed; generic PART culling/catch-up is not implemented.

## Exact charged ledger

The hard limits are786432 bytes total, split655360 visual and131072 audio.
Both component caps include their conservative allowances and loading scratch.
Admission requires at least4288 free4096-byte pages:16MiB reserve plus the entire
768KiB optional cap. It runs at the end of first frame, after initial world-load
publication and mandatory animation/session allocation. This is source arithmetic,
not a measured stock64 result. Runtime accounting uses actual sizeof values and
loader allocated/resident byte counters, never the table constants as a substitute.

32-bit Xbox layout uses explicit padding around64-bit cue tickets; slot136 bytes,
visual owner11912 bytes. The owner includes copied material views, all200 private
36-byte particles,1560 track bytes, clocks, pointers, culling values and counters.

| Visual component | Charged bytes |
| --- | ---: |
|Owner including slots/particles/tracks and embedded24-byte texture owner|11912|
|Owner pointer, pending archive references, epoch and diagnostics|96|
|Read-only lookup/string reservation|2048|
|Stack allowance16384 plus heap/page rounding allowance49152|65536|
|**Fixed visual charge F**|**79592**|
|Eleven mesh owners, frame views and copied SFXO payloads|72839|
|Three MATL banks (604+604+1462)|2670|
|Eight Small/Big pairs of pairs:8x(764+2684)|27584|
|Two seven-mesh Destroy instance sets:2x1096|2192|
|Eight binding rows and24 texture record reservations|2112|
|Six retained image descriptors|120|
|Six native RGBA images:5x65536+16384|344064|
|**Retained visual total / largest accounted visual peak**|**531173**|

Sequential geometry preparation releases each raw buffer/directory before the
next definition, and releases all raw geometry input before instances/textures:

| Loading stage | Exact accounted visual peak |
| --- | ---: |
|Small: F+7466 meshes+604 MATL+264 directory+4749 scratch|92675|
|Big: F+31892 meshes+1208 MATL+264 directory+21709 scratch|134665|
|Destroy: F+72839 meshes+2670 MATL+360 directory+2457 scratch|157918|
|All instances, before textures|184877|
|All textures retained|531173|

Map preflight requires exact named entry sizes, TGA type/depth/dimensions in the
same first matching archive as the shared loader. All pixel allocations are
whole native pages; GPU submission aliases them. No second GPU texture copy or
encoded TGA heap copy is introduced. Missing binding success from the shared
loader is explicitly rejected.

| Image | Input bytes / format | RGBA bytes |
| --- | --- | ---: |
|NanoFlare01.tga, maps4|39566,128x128 RGB24 RLE|65536|
|NanoShieldHit01.tga, maps2|65580,128x128 RGBA32|65536|
|NanoMuzzleFlash.tga, maps2|45645,128x128 RGB24 RLE|65536|
|NanoVFXShock.tga, maps3|49196,128x128 RGB24|65536|
|NanoHitFlare02.tga, maps3|53205,128x128 RGBA32 RLE|65536|
|Electricity01.tga, maps1|9116,64x64 RGBA32 RLE|16384|

Audio fixed charge is48-byte state+36-byte diagnostics+512-byte read-only
reservation+16384-byte stack/heap allowance =16980. Whole WAV allocations are
26230+47736+12840 =86806, for **103786 accounted audio bytes**. The shared mixer
borrows PCM8/16 directly; no conversion buffer is allocated. Each demanded sample
is charged even if already resident, so the incremental bound is conservative.
Names, exact VPP entry sizes, existing resident size, nonlooping metadata, the
128KiB limit and existing256KiB bank headroom are checked BEFORE any reload.
No bank/pool capacity changes or sample eviction occurs. A failed transaction
unloads only its freshly loaded, unpublished PCM. Successful pins last until
ordinary bank teardown; native borrowers are released by that existing owner.

Audio loads before visual preparation. Therefore the largest combined retained
and loading charge is **531173+103786 =634959 bytes**, below786432. All dynamic
visual calls receive remaining budget; the allowances stay charged throughout
failure cleanup as well. These figures exclude existing shared scene/audio/
renderer allocations, whose caps and ownership are unchanged, not additional
feature-owned memory. If another ABI changes sizeof, the runtime cap still
refuses an oversized optional owner.

## Parent composition and remaining validation

Cherry-pick the source commit while preserving the parent's newer NanoConstant,
Cane warmup, Blind and particle/GPU-fence hooks. In scene_world_load.inc's FINAL
successful publication tail, after native storage close, add:

```c
if(!status && world_published)scene_nano_hit_reset();
```

This branch deliberately does not edit that parent-owned save file. Do not reset
in validation, staging, early publication or failed-close handling. Reset clears
only cosmetic instances, keeps shared resources alive, and never replays Event76,
hits or audio. Normal scene/NPC-owner teardown frees visuals; existing audio-bank
teardown retires PCM after its native borrower reset.

Only source/diff review and named original metadata/disassembly reads were done.
Compilation, actual resource admission, native rendering/audibility and gameplay
feedback remain unverified until parent/tester work. No generated payloads were
created, no saved HDD was accessed, and cleanup remains exclusively parent-owned.
