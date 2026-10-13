# Scripted explosion sparks and trails

Source implementation, October12,2026. Parent owns the next hourly stock64
Xbox compilation and startup smoke. No build, feature test, emulator session,
capture or HDD access was performed for this change.

## Reachable authored consumer and original evidence

The previous scripted vclip owner resolved all nine recipe slots, then masked
the definition down to central slots0..5. Its live runtime had no sparks or
head/tail consumer. This change retains central ownership and adds optional
slots6..8 to that same renderer/material-tag path.

Read-only inspection of installed VPP directories, table text, RFL event
records and RF.exe disassembly established:

- `vclip.tbl` maps `big_charge_explode` to `shoulder mounted geomod`.
  `explosion.tbl` specifies50 `explosion random bits`, `explosion fire leg`
  heads, head time0.1, head random factor0.25, `explosion smoke leg` tails,
  and explosion play time2.0. All three emitter definitions resolve.
- L9S1 has Explode UID6178, radius4, linked from Delay UID6184. L18S2 has
  Explode UID10237, radius30. Their authored activation/damage/GeoMod records
  are untouched. The existing generic death-vclip callers of this presentation
  owner also receive admitted channels without changes to NPC death dispatch.
- Original48e71f..48e7a9 suppresses trails for flag1 (`no_trails`) or size<1;
  below size8 it truncates a draw in[size-1,size+1), otherwise it uses8.
  Constants5893f8 and58982c decode to1.0 and8.0.
- 48e9aa..48ea88 emits the authored sparks count, resampling the direction
  cone and size-scaled velocity for each particle. 48e5a0 prepares shared
  particle fields; radius scales with effect size, optional lifetime does not.
- 48ea8c..48ec48 seeds up to8 head positions/velocities and emits initial
  heads. Position offsets span the plane normal to the supplied direction;
  head velocity uses cone cosine0.7 and one shared sampled speed times size.
- 48e290 increments elapsed first, emits heads while elapsed<head_time,
  then emits tails at each tracked live head's previous position(+6c).
  Central processing and strict elapsed>play_time expiration follow.
- The commonly authored `charge_explode`/`rocket hit` recipe names missing
  emitter `explosion random bits 2`; its absent optional binding stays absent.

The executable identity/address context is the installed original documented
in `EXPLOSION-RECIPES.md`. This is a bounded reconstructed consumer, not a claim
of original global-pool/random-sequence or screenshot parity.

## Runtime and lifetime

Central creation completes before optional creation. Existing four clip
owners, eight instances per owner, central clock and emitter slots remain.
Sparks and heads use shared pool0; tails follow the newest successfully
created head in each of at most8 lanes. Optional creation leaves64 of pool0's
500 slots available to other bursts. It adds no emitter slots or particle
records; the shared128-emitter/500+1100-particle capacities are unchanged.
An exhausted pool skips cosmetic output rather than failing activation.

Each optional instance owns a small visual RNG. Initial optional emission
does not consume the central creation RNG. Ordinary shared particle stepping,
including tail swirl, still uses the established particle simulation RNG.
Existing particle physics, visibility, room ownership and textured rendering
remain responsible for the spawned records. Scripted direction remains +X,
as in the pre-existing world wrapper; the seed offsets use its Y/Z plane.

The particle pool now has an optional recycle observer (8 bytes on Xbox).
It is called before the free-list append. This owner invalidates matching
head indices immediately, including reuse within the same tick; it never
retains a raw particle pointer. The callback does not mutate/reenter the pool.
Another installed observer causes optional admission to be declined. Losing
the expected observer disables existing trail lanes before dereferencing
their indices. The scene detaches its observer before the particle pool dies.

The final successful-load boundary releases this owner's central emitters,
recycles its tagged particles, and clears instance clocks/tracks. It retains
decoded resources for future ordinary events. Failed/absent loads do not
clear effects. Scene teardown destroys particles, then resources/tracking.
No save sections, damage calculations, GeoMod requests or event activation
semantics are changed.

## Stock64 resource accounting

All central owners load first. Optional admission runs afterward, before
startup event dispatch. It preserves the existing512 KiB aggregate charge:
owner structs + existing material charge + optional tracking + optional image
descriptors/pixels + native texture-page padding. The old material accounting
conservatively charges the embedded material descriptor twice; this change
retains that convention rather than spending its apparent surplus.

Each optional allocation checks the same16 MiB physical-page reserve used by
other optional scene packs. A failed resource/allocation affects only that
channel. Sparks and heads share an image by filename within their owner;
tail admission requires an admitted head. There is no frame-time heap work.

Measured original image headers and 32-bit source-layout accounting:

| Item | Bytes |
| --- | ---: |
| One trail instance:8 positions,8 velocities,8 indices,count,room,RNG |236 |
| One optional owner:mask +8 instances |1,892 |
| Four optional owners |7,568 |
| Four added owner pointers +one pool observer +20 diagnostic words |104 |
| Maximum additional fixed tracking/diagnostics |**7,672** |
| Flame01_02.tga,256x256 RGB24 decodedRGBA8 |262,144 |
| smoketest.tga,64x64 RGBA32 decodedRGBA8 |16,384 |
| Two heap rf_image descriptors |40 |
| Full optional image/tracking charge for one owner |280,460 |
| Reduced128x128 head +64x64 tail +descriptors/tracking |83,852 |

The fixed-tracking16 KiB bound is also a compile-time sizeof constraint.
The central-only effect owner is now2,992 bytes on Xbox,4 more than before.
No pixel assets are added to Git.

Full-size textures are preferred. If a texture larger than128 would leave
less than32 KiB for the remaining channels, its optional load uses a128px
streaming box filter. This avoids the old+new allocation peak of
`rf_image_reduce`; no full-size source bitmap is allocated. The new TGA helper
uses the existing type2/type10,24/32-bit decoding/orientation rules and rounded
RGBA box averaging. Its row sums are2,048 stack bytes in addition to the
existing4,116-byte archive reader and small locals. Existing full-size image
decoding is unchanged. Animated optional replacements are unsupported.

For scale, one big-charge owner with its three central images and full optional
images charges431,192 bytes. A big-charge plus rocket-hit pair has256,428 bytes
of central charge; the reduced optional set brings that to340,280 bytes.
These are header/source-derived costs, not an executed level measurement.
Additional owners, archive substitutions and native fragmentation may cause
declined admission; central output remains available.

Allocator metadata, compiler stack overhead and unrelated scene allocations
are outside the pre-existing512 KiB resource convention. Actual native page
snapshots and byte counters are exposed for the parent's normal integration
observability; they have not been sampled by this task.

`rf_scene_script_explode_optional_stats[20]` contains, in order:
admitted owners; additional charged bytes; fixed tracking bytes; total owner
charge; pool skips; sparks; heads; tails; recycled-head invalidations;
admission skips; last optional status; last admission free pages; conservative
admission peak ceiling; successful-load resets; native image-page padding;
reduced-image count; free pages before/after admission; decoded pixel bytes;
heap image-descriptor bytes. The optional charge equals admitted owners*1892
plus decoded pixels, heap image descriptors and native padding on Xbox.

## Remaining approximations and integration

- Optional memory/pool admission, independent birth RNG,128px fallback and
  supplied scripted +X direction are practical port policies. No original
  global RNG ordering, camera-facing particle detail or rendered parity claim.
- This consumes installed static optional textures and existing physics.
  Original wrapper-specific liquid suppression and particle impact callbacks
  are not newly reconstructed. No conventional bullet impacts are added.
- Source review only; compilation, runtime visibility and functional coverage
  remain unverified. Parent owns the hourly stock64 build/startup smoke;
  testers own feature coverage.

Integration touches narrow scene load/teardown hooks and one final-success
world-load hook. The resource/runtime logic lives in the two existing scripted
effect includes and two new optional includes. Shared changes are additive:
the particle recycle observer and streaming TGA reduction entry point. Preserve
newer parent save-load cleanup calls when composing the one-line reset hook.
