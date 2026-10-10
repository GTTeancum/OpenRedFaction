# Sea Creature Sonar: authored PART first pass

Status: integrated after independent source review; uncompiled and runtime-unverified, awaiting the 05:00 Xbox batch. This slice supplies the original particle-only projectile visual; shared Sonar flight, liquid expiry and damage remain independent. It adds no general PART engine, gameplay random draws, impact effect or swimming pursuit.

## Authored input and reader

Original meshes.vpp/SonarAttack.vfx is1642 bytes, VSFX0x40006: 25 effect frames, zero meshes, one PART and one MATL. PART payload1432 contains an80-byte prefix and26 identical52-byte samples. Source reader5420f0 supplies name VParticle02, parent Scene Root, flag4, material0, start0, capacity10, lifetime4480 ticks, zero lifetime variation, zero force references, rectangular emitter0 with zero width/depth and no speed spread. Its opaque byte23 is0 and is not an enable gate.

The helper uses existing VFX directory/material readers and requires exact supported counts, names, options, canonical authored scalar values, finite floats, exact payload consumption and identical samples. It rejects other content rather than skipping records. The mesh-only loader remains unchanged. MATL is the actual one-record type0 material: rate15, glow byte1, SonarAttack.tga, start0, speed1, mode2, brightness1 and opacity0.35. Existing material texture ownership loads the original64×64 texture from maps; no original inputs are changed or checked in.

## Flight-owned particles

Original542a20 allocates ten36-byte particles;542e90 ages/moves particles, then540bf0/540e80 emits new ones. This implementation owns up to64 independent accepted flights, each with ten persistent world-space sprite slots, a complete copied source/flight/basis and a64-bit launch ticket. An explicit accept resets the visual slot, even when the same shooter launches into a reused slot between renders. No actor pointer, model pose or source-slot inference owns an existing particle.

Lifetime is4480×(1/4800), about0.933333 seconds. Emission uses the decoded0.0022321429569274187 sample rate×320×15, about10.714286 sprites/second. Fractional credit is consumed before scanning free slots, so a full pool drops births rather than accumulating debt. There is no frame-zero burst. Normal acceptance/debit succeeds independently of visual capacity and presentation errors.

The zero-spread local birth velocity is(0,-0.254×12,0), transformed first by the authored quaternion(-0.70710670948,-0,0,0.70710682869), then by the retained projectile basis. Flag8 is absent, so projectile speed20 is not added. New roots interpolate the prior/current projectile positions by requested-birth fraction, and new particles receive the original birth-age jitter and corresponding velocity offset. Once born, particles remain in world space and move ballistically; later root motion does not drag older sprites. No forces or gravity apply to this exact record.

A dedicated per-flight rf_random_state, seeded1, consumes the observed four draws for zero-width X, zero-depth Z, birth-age jitter and zero lifetime variance. Gameplay RNG is untouched. The seed and original global interleaving are deliberate deterministic presentation policy rather than bit-exact reconstruction.

Tick runs once per simulation frame and repeated draws/pause frames do not advance it. Missed/nonconsecutive steps, mismatched copied ownership or invalid numeric values retire the affected visual only. Terminal flights immediately discard their slots. Original ordinary owned-projectile teardown486670→489fc0→502b10→5028f0→504620→54b570→542b60→5431f0 frees the PART particle array; an unrelated drain-capable stop API does not establish a required tail. The external/shared-model branch is outside this slice. Existing Sonar reset and transient-save guards clear all unsaved visual state.

## Exact billboard presentation

The original render path5431d0→516fb0→557460 defines normalized ageu=age/lifetime.

- Size is full from birth, then falls linearly afteru=0.445 to zero at1. Original billboard radius is2×sample size0.7953000069×envelope, peaking at1.5906000137. Existing billboard primitives already treat that argument as a half-extent; no further halving applies.
- Alpha ramps from0 to0.35 byu=0.355, remains0.35 throughu=0.635, then fades to0 at1. Alpha is truncated after multiplication by255.
- Brightness1 yields white RGB for this material. The helper does not introduce another lighting model.
- PART flag4 selects normalized particle-age texture sampling. The single original TGA frame stays camera-facing, angle0, using RF_PARTICLE_GLOW_MODE0x06110c42.

The helper reuses rf_particle_world_billboard, rf_particle_render_decode, rf_particle_vertex_encode and the existing rf_scene_particle_sink with camera depth/reciprocal scale and unit UV scale. Native particle depth/zoom/blend behavior is retained; no backend or geometry-material extension is required.

## Bounded policy and remaining verification

Original54cce0 wraps the owning effect at25 frames, while542f68 calculates a wrapped particle age delta with samplecount26. This first pass intentionally integrates the constant track continuously at1/60 second instead of asserting bit-exact loop scheduler parity. Emission credit likewise uses continuous elapsed time. Full sorting/visibility dispatch, exact original root/muzzle timing, global RNG sequence, float-intermediate parity and final render/runtime appearance remain unverified.

Stock64 MiB constraints are retained: one shared original texture, fixed64×10 slots, bounded loader scratch,96 KiB combined admission budget, and no per-frame allocation. Resource failure releases partial ownership and is reported to the parent; it never grants inventory or changes firing eligibility. A runtime visual failure cannot undo a shot. Parent owns scene/resource/lifecycle wiring, TO-DO.MD and the serial Xbox batch. No tests, syntax checks, builds, emulator runs, fixtures, images, PC work or campaign routes were performed for this source-only slice.

Demanded original selected-owned Sonar resources use a 96 KiB startup admission budget. Failure follows the existing startup error path; no silent replacement visual is installed. The planned L17S1 neutral startup does not contain a Sonar owner and therefore cannot establish this visual resource or draw runtime.
