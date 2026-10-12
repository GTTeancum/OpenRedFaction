# Constant Nano-shield imagery

Source-written on the published `6d1bf09b` base; no build, syntax check,
fixture, runtime session or image capture was run by this task. The parent
owns composition, independent review and the hourly Xbox build. This slice
does not change shield-break gameplay, armor, Event76, sounds or save formats.

## Original evidence

The addresses below were read from the installed `RF.exe` with static
disassembly; asset metadata was read directly from the installed VPP inputs.
No original-game visual reference or disc session was used.

| Original address | Behavior used |
| --- | --- |
| `422ab6..422b08` | Clear actor850; class724 bit02000000 allocates the constant shield owner, loads `NanoShieldConstant.vfx` via502a60 and starts loop0 at speed1 via503390. Filename at595a10. |
| `42cca0` | Admission: actual class bit02000000, current armor at38 strictly positive, actor814 bit20 clear. Existing `rf_entity_armor_immunity` implements this predicate. |
| `4210c0..421100` | Under the predicate, obtain attachment position through48ac70, use actor48 orientation, and update the shield handle with global simulation delta through503360. OFF does not reset its phase. |
| `4222d4..422312` | Same predicate and attachment position/basis, followed by503100 draw. |
| `48a0e9`, `503210`, `5011f0` | Actor26c records the first skeleton bone with a negative parent, or-1. This is not an eye or named attachment tag. |
| `48ac70` | With root index-1, copy actor3c position; otherwise503230 obtains the evaluated root translation and4fb9d0 transforms it by actor48 then adds actor3c. |
| `501af0..501b34` | Type3 initialization clears stop4, sets loop2 and clears seconds/frame. |
| `54ccf8..54cd78` | Add delta to stored seconds, multiply by15, and wrap at header frame count using count/15 duration. Constants589854=15 and58a268=0.06666667014360428. |
| `42500e..42502d` | Close the model handle through502b10 before freeing the actor850 owner. |
| `4b9b60`, `4ba380` | Event76 clears/sets actor814 bit20. The existing setter remains authoritative; no replay is introduced. |

The installed `entity.tbl` marks Capek and masako as the Nano classes. The
68 installed campaign RFL records inspected contain these authored owners:

| Level | Nano actor UIDs |
| --- | --- |
| L8S4 |8359,10357 |
| L9S1 |5806 |
| L9S2 |5845 |
| L11S3 |4938,10623 |
| L20S2 |18301 |

Two instances therefore cover the maximum installed per-level demand.
Existing `scene_masako_seed.inc` appends `masako_endgame` in L20S2, making
that level's reconstructed demand two as well. The implementation selects
by actual class flags and registry identity, not these names or UIDs. Demand
above two refuses this optional owner; it never alters gameplay to fit.

## Authored resource and memory contract

`meshes.vpp` contains `NanoShieldConstant.vfx` at4126720, size409913,
VSFX40006, header duration32. Its exact four records are:

| Record | Payload bytes | Decoded use |
| --- | ---: | --- |
| SFXO Plane01 |649 |4 vertices,2 faces, billboard flags11 |
| SFXO Sphere01 |408718 |114 vertices,448 faces, animated-UV flags110 |
| MATL NanoShieldGlow01.tga |195 |glow1, brightness1,32 opacity samples |
| MATL NanoShield01.tga |191 |glow1, brightness1,32 opacity samples |

Both SFXOs have Scene Root parent,33 samples, rate15, start0,
end2.1666667461395264 and keyed timing flags62 after the edge flag byte.
The initial enabled byte is0; that is not a reason to reject these meshes.
Shared SFXO/key/UV and MATL evaluators consume the authored records.

The first texture is a64x64 RGBA TGA in maps1.vpp (offset71305216,
encoded16428 bytes); the second is128x128 in maps2.vpp (offset72699904,
encoded65580). Decoded storage is16384+65536=81920 bytes. Native allocations
are already whole4096-byte pages, so padding is0. Before shared texture
loading, the first matching archive entry must have the exact TGA size,
type2,32-bit dimensions. After loading, both expected primary bindings must
point to their corresponding one-frame animation and a nonnull decoded
format7 image of the exact byte count. RF_OK with a missing binding is
explicitly rejected. Secondary/reflection bindings must be absent.

The hard feature ceiling is1048576 bytes for both retained resources and
loading peak. On the32-bit Xbox ABI, source-derived accounting is:

| Component | Bytes |
| --- | ---: |
| Owner including two104-byte identity/clock slots |256 |
| Owner pointer, pending archive references, diagnostics |68 |
| Two shared meshes: payload409367 +66 frame views at112 +2 owners at308 |417375 |
| Shared material bank:386 payload +416 views +20 owner |822 |
| Four instance allocations:2 x (136 plane +12160 sphere) |24592 |
| Texture binding/slot storage528 +image descriptors40 +pixels81920 |82488 |
| Fixed loader/render stack allowance |16384 |
| Conservative heap rounding, metadata and page slack allowance |65536 |
| **Accounted retained total** |**607521** |

The largest overlap is both retained meshes and MATL plus the408718-byte
raw SFXO input and264-byte directory. It precedes instance/texture allocation:
82244 fixed/allowance +417375 meshes +822 MATL +408718 raw +264 directory
= **909423 bytes loading peak**. The raw input and directory are released
before instance and texture creation. The later607521-byte retained total
is smaller. Exact source-derived owned payload without the conservative
81920-byte allowance is525601 retained /827503 peak. These are arithmetic
from32-bit structure layouts, not measured build/runtime results;
`rf_scene_nano_visual[1..2]` reports actual `sizeof`-based accounting.

Every variable allocation receives the remaining capped budget. Shared
loaders check the requested size before allocating. The allowance remains
charged during every stage, including failures. Preflight prevents a changed
VBM or oversized texture from introducing unaccounted native page padding.
No image, geometry, sorting or instance buffer allocation occurs during a
frame. The existing render queue and polygon clipper are reused.

Admission is once per scene, at the end of its first frame after initial
world-load publication and core animation/session allocations. It requires
at least4352 free pages (16 MiB reserve plus the entire1 MiB optional cap).
Failure leaves the visual owner absent and records a diagnostic; it does not
fail core startup. Archive references are borrowed from the enclosing scene
call, whose normal teardown clears the pending references before closing
the archives. No archive copies are retained.

## Timeline, identity and rendering

Each instance binds the actor slot, full generation-bearing registry handle,
UID, class, skeleton, published pose pointer and matrix storage. Resolution
also verifies the entity registry, loaded model registration, class-to-
skeleton mapping and bone count. A removed/replaced identity retires its
visual slot before reuse. The current armor/disable-bit predicate is read
again before queueing and drawing; no cached eligibility controls gameplay.

Only the normal NPC simulation tick advances seconds, once per frame. The
constant32-frame loop uses bounded floating remainder with duration32 times
the original1/15 constant. This replaces repeated subtraction for arbitrary
large deltas, so exact x87 rounding is not claimed. Pauses/repeated rendering
do not age the effect. OFF/zero-armor intervals retain phase and suppress
draws; ON resumes that phase. New identities and successful load publication
start a fresh cosmetic timeline.

Queueing requires a complete current pose generation, transforms the first
root's evaluated translation by the actor's published body basis, and
captures that basis/root and pose generation. Drawing rejects changed or
stale generation/identity, removed, disabled and hidden owners. It does not
evaluate or advance the actor pose itself. Ordinary room/frustum visibility
applies; temporary stale poses suppress the cosmetic draw.

Plane01 uses the existing authored billboard projection. Sphere01 uses
shared keyed geometry/animated UV evaluation, original-facing helper and
the bounded world-quad clipper with a repeated final triangle corner. Both
materials are additive white with authored opacity tracks. Actor-level
queue order is used; the additive faces do not receive a separate depth-sort
buffer. Exact original effect-list ordering and pixel-level parity remain
unclaimed. No lighting, collision, model ownership or gameplay timer is
changed by this visual owner.

Submission uses the ordinary `rf_scene_draw_particles` path and its current
camera, including scope projection. Image allocations stay alive until scene
teardown. Sink errors propagate to the existing renderer error fence; normal
success uses its existing HUD completion fence. No new renderer lifetime
guard or global texture mutation is introduced. Instances are retired before
mesh/material/image teardown. Actor unregister and NPC-owner teardown also
clear transient visual references.

## Parent composition requirement

The feature branch deliberately does not edit `scene_world_load.inc`, per
the parent's ownership direction. In its **final** successful publication
tail, after native storage close, compose:

```c
if(!status && world_published)scene_nano_visual_reset();
```

Do not put this in parsing, validation, early publication, or failure cleanup.
Do not replay Event76 or rewrite RFNC armor/disable state. Preserve the
parent's newer load-transaction and particle-pass GPU fences when composing.
All other scene hooks are included. The reset is already forward-declared
before the world-load include; it clears only transient slots while keeping
allocated image/mesh resources alive. Compilation/runtime remain unverified
until the parent build and tester work.
