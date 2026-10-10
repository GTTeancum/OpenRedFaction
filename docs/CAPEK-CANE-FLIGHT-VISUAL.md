# Capek Cane: original composite flight visual

Status: source-written outside the active checkout against c621714b; uncompiled and runtime-unverified. Parent owns integration and the next hourly Xbox batch. This optional consumer is independent of finite prepaid Cane mechanics, target acquisition, object-only contact, shields and direct energy damage.

## Original asset evidence

Read-only `Installed_Game/meshes.vpp`, `NanoAttackMissile.vfx`: archive offset4104192,3543 bytes, SHA-256 `4839fdd15615e53461ff0774c4280c9cd446cb53293df31ea8a4048d556d34c6`. VSFX version0x40006 has nine effect frames, two meshes, one PART, one WARP and three MATLs. Physical payload offsets/lengths are:

- SFXO EnergyFlare02:136/987
- WARP VWind01:1131/507
- SFXO EnergyFlare01:1646/987
- PART VParticle01:2641/608
- MATL NanoCaneFlare:3257/100
- MATL NanoMuzzleFlash:3365/102
- MATL Electricity01:3475/68

All objects are Scene Root children. The exact seven-record ordering and supported fields are checked. The generic geometry-only asset loader is unchanged: it must still reject unhandled PART/WARP compositions. No original binary or texture is added to tracked source, and no original input is modified.

Original RF.exe SHA-256 is `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`. Source inspection used static data decoding and disassembly only; no original instructions or new implementation were executed.

## Two authored morph billboards

Both SFXO payloads have four vertices, two faces, rate15, start0, end0.6333333253860474, ten samples, one material and edges.flags0x15. Their serialized enabled byte is0; this does not suppress the original flag1 billboard path. Complete shared `rf_vfx_mesh_open` decoding validates faces, edges, per-frame vertices, UVs and sample counts. `rf_vfx_mesh_sample` supplies the actual animated center and two extras. No persistent vertex instance is needed because the original special renderer ignores the triangle/UV buffers for this path.

Original `53ee90→53ef50→516f70→554bf0` dispatches flag1 to camera-facing billboards. `554dc3/554dd2` pass sampled extra0/extra1 and angle0 to `555b20→555510`; `5555e4..555605` halve both extras. They are full width/height, not a radius and not the original sideways YZ vertex triangles. The first frames are approximately0.800813×0.790885 for EnergyFlare02 and1.637420×1.621822 for EnergyFlare01. The consumer preserves all ten decoded samples and shared interpolation, places their centers with the accepted flight's current basis/root, and renders the real camera-facing rectangles.

The bounded rectangle adapter reuses the existing camera transform, center projection, billboard prepare/project, clipping and vertex encoding. Square dimensions1×1, radius=half-width and corner-only scaleY multiplied by height/width reproduce the authored rectangle at angle0. Camera scaleZ and half-width depth bias remain unchanged, matching `5556d2..55572a`. Projection/camera ownership is never modified. There is no arbitrary radial mesh or Laser/TriBeam substitution.

Both flare MATLs are type0, rate15, glow1, start0, speed1, mode2, brightness1 and nine original animated alpha samples. The shared material evaluator retains those tracks, including the complementary pulsation. Original `554de2..554e18` rounds alpha through `504e90` (add0.5 before truncation); the helper does likewise. White RGB and `RF_PARTICLE_GLOW_MODE`0x06110c42 implement the authored additive SRC_ALPHA/ONE blend with existing depth testing and no depth writes.

## Authored PART and its named WARP

PART reader `5420f0` supplies flags0x10, one named force reference `VWind01`, start0, samplecount10, material2,50 slots, lifetime3200 ticks, zero lifetime variation and spherical emitter kind1. The eighty-eight-byte prefix is followed by ten identical52-byte samples. Local origin is zero, quaternion identity, sphere radius0.22832441329956055, stored depth0.15675613284111023, size0.1, speed0, spread0.05079999938607216 and emission rate0.015625. Kind1 does not consume the stored rectangular depth/quaternion. All values and constant samples are checked before admission.

`54290d..542971` applies speed/spread×12; the resulting signed radial birth speed is uniform in approximately[-0.6096,+0.6096]. `540ef1→4fad10` supplies the two-draw uniform sphere direction. Position lies on the original sphere surface. The source root is interpolated between the preceding/current accepted flight positions; newborn age jitter offsets its position by birth velocity. Flag8 is clear, so speed20 of the gameplay projectile is not added. Lifetime is3200/4800, approximately0.666667 seconds. Emission is0.015625×320×15=75 births/second. The50 slots are retained for each of the16 flight slots. Credit is consumed before free-slot admission, and full pools drop births without accumulating debt. There is no frame-zero particle burst.

Original `557460` gives billboard half-extent2×0.1, with size and alpha full until normalized age0.7 then fading linearly to zero at1. PART alpha is truncated after×255. Flags0x10 randomize each sprite's angle; flags4 are clear, so texture time is particle age×15 rather than normalized time. Material2 uses constant brightness/alpha1 and glow1. The one original Electricity01 TGA supplies the actual sprite shape.

The force is not omitted. `56a9c0` decodes WARP kind1, ten identical48-byte samples, center(4.3762518942003226e-8,4.3762518942003226e-8,-1.0011696815490723), quaternion(0.5,-0.5,0.5,-0.4999999701976776), strength-877824 and remaining scalars0,0,0,1. `56af20` samples it; `5419b5/541b0d/541e36` rotate the center by the PART instance basis and translate by its root. Kind1 skips the force-direction quaternion. `541579..5415d7` normalizes particle-position minus force-center and scales by strength; `5417b2..5417e0` applies1e-4 and global0x58a26c=0.0625, giving radial acceleration magnitude5.4864 toward the moving force center. Zero decay/turbulence in this actual record mean no exponential/noise approximation is required. Existing particles use original-style trapezoidal velocity integration. An exactly coincident particle uses zero force as an explicit finite port policy.

## Original texture residency

Header/source-data inspection, without image viewing or capture:

- maps1.vpp/Electricity01.tga, offset71270400,9116 bytes,64×64,32-bit RLE, SHA-256 `895f0cf201450bc8e0000433db5cb0a205f86083634962ee60a61f9d0a73258c`
- maps1.vpp/NanoCaneFlare.tga, offset71280640,24262 bytes,128×128,24-bit RLE, SHA-256 `175dbedeb47202c16d0599b213fb0140ee6a23169b245a279e55e9edf9b98bc0`
- maps2.vpp/NanoMuzzleFlash.tga, offset72652800,45645 bytes,128×128,24-bit RLE, SHA-256 `7d5086e1fe1e2a5cb034ba208bece09f578baf27449b242a77f404f0fba36c6b`

Existing texture ownership loads these original single-frame images and checks the bound names, dimensions and source formats. Their decoded pixels total147456 bytes. Missing, corrupt or unsupported data refuses the whole optional visual owner; no fallback bitmap is installed.

## Ownership, optional budget and render integration

Accepted-flight publication explicitly resets its visual slot, copies the full source/flight payload and allocates a monotonic64-bit launch ticket. Tickets survive reset and refuse overflow. Each visual uses an isolated RNG seeded1; gameplay RNG is untouched. Guidance is allowed to change velocity/basis. Immutable copied source, weapon, damage profile, liquid flags and radius must still match. No mutable NPC pose or source liveness owns old particles. Source death can cancel a pending windup without erasing an already-launched visual.

Visual tick immediately follows mechanical tick, once per simulation frame. Repeated frame numbers do not age/emit; discontinuities, inconsistent lifetime progression, nonfinite values or invalid room queries discard only that visual. Terminal mechanics clear particles immediately. Ordinary owned-projectile teardown `486670→489fc0→502b10→5028f0→504620→54b570→542b60→5431f0` supports this destruction policy; this slice does not assert a detached draining tail. The mechanical reset calls the visual reset on successful-load/world/life boundaries; failed loads must leave both live states untouched. Existing active-flight save guards cover all surviving visual owners; no visual save row is introduced.

Queue all live PART sprites and both SFXO rectangles into the existing per-room render queue, with sorted=1 and separate callback0x488b03. Reuse existing portal traversal, sphere culling, sorting and particle sink. Per-flight tickets guard queued slot reuse; queue overflow skips presentation only. Every visual error is diagnostic/nonfatal and never changes damage, debit, homing, collision or terminal ordering.

The maximum admission is256KiB, including the shared owner, all16×50 static particle slots and copied-owner state, diagnostics/ticket, complete two-mesh resources, MATL bank, texture ownership/pixels, directory accounting,987-byte loader scratch and an additional8192-byte fixed stack allowance. The streamed TGA loader has a4096-byte reader, not a full encoded-image allocation. No per-frame allocation is introduced. On Xbox, optional admission additionally requires the existing16MiB free gameplay/render reserve plus the entire requested budget. Counters retain accounted resident/peak and pre-admission free pages. This is conservative bounded admission, not measured action-runtime or contiguous-GPU-memory proof.

## Explicit first-pass limits

The visual now consumes all seven original records and all three original textures. It remains a bounded exact-asset consumer, not a general VSFX/PART/WARP engine. Continuous60Hz aging/emission and one force sample at the current root are practical first-pass scheduling. Original outer effect wrap9 versus internal samplecount10, interval subdivision, force-root interpolation, visibility-dependent catch-up, exact global RNG interleaving and x87 intermediate parity are not claimed. The SFXO sample/material evaluation and half-width depth policy are source-grounded; final appearance is unverified. Room-local rendering can hide a missile or trail while it traverses non-room solid space; this does not change its world-skipping gameplay flight. Separate warmup/impact effects, launch/impact sounds and external/shared-model tail policies are outside this slice.

No syntax checks, tests, builds, XEMU, PC runtime, new gameplay fixtures, routes, inventory grants, events or images were performed. Parent must independently review, integrate the documented hooks, maintain TO-DO.MD and run the sole hourly Xbox batch.
