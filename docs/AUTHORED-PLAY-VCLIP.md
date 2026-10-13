# Authored Play_Vclip first pass

Source-written October 12, 2026; compilation and runtime remain unverified until the parent-owned hourly Xbox batch. No independent build, functional check, emulator run, image capture or saved-HDD access was performed. This branch is based on published `origin/master` `6d1bf09b54625cade292445139cd9da0779634d4`.

## Implemented behavior

Type70 now invokes an ON-only presentation callback through the ordinary event scheduler. Its delay, source/actor references, generic link phase, OFF behavior and activation policy remain with the existing event implementation. Missing resources, unsupported clips or the one-active-presentation cap refuse the visual with diagnostics and return success to preserve event chaining. No damage, physics object or vehicle controller is created.

The campaign-authored `transport_explode` uses every major channel from `shipexp01_anim.VFX`: two explosion billboards, five animated debris meshes, all ten particle tracks, all fifteen animated materials, the diffuse/animated textures and the authored reflection map. The existing Foley service plays `Large Explosion`. This adds no audio cache or voice capacity. The effect advances at 15 effect frames/second through the regular event clock, stops emission/mesh playback at frame90, and lets surviving particles drain. A clock discontinuity exceeding250ms retires the optional presentation instead of accumulating work.

Particles use an independent fixed710-slot owner with each track's authored capacity; they never displace shared gameplay particles. All resources are allocated once at level demand, and playback/drawing allocates nothing. Repeated ON while active is refused; later ON reuses the owner. Opaque debris is submitted first through the bounded Xbox vertex path with exact `RF_VFX_OPAQUE_MODE` admission, depth test/write and blending disabled. Translucent meshes, reflection and particles then use the existing depth-reading alpha/additive modes. The whole effect precedes ordinary scene particles. Main-camera state and simulation ownership are unchanged.

## Original evidence

Evidence is binary/data-derived from the read-only installed PC original, not screenshots. It establishes behavior and export format; it is not proof of Xbox retail raster parity.

- RF.exe type70 factory `0x4b73e3`, constructor `0x4bed20`, vtable `0x589c0c`; setup `0x4b8ab0` resolves the clip through `0x4c1d00`. Parser `0x462839..0x46284d` supplies the authored fields.
- ON `0x4bd0a0` invokes `0x4c16e0` with the event position, null orientation and sound enabled. OFF uses generic `0x4b9f80`. The VFX branch `0x4c17d1..0x4c18b6` builds the identity basis for null orientation and starts playback at speed1 (`0x4c1881..0x4c188e`); the separate bitmap/particle float parameter is not applied as a VFX scale. Foley dispatch is `0x4c177f..0x4c17b5`.
- The68 campaign-level source scan found one authored use: `L3S3.rfl` UID9662, clip `transport_explode`, delay20.8, position `(43.4,66.2,56.9)`. Trigger428 links to Play_Animation532 (`Transport_takeoff.mvf`, delay4.0), whose links include9662. UID9662 has no outgoing links. No development route or campaign playthrough was run.
- `vclip.tbl` specifies `shipexp01_anim.VFX`, radius100 and Foley `Large Explosion`; no code explosion, bitmap clip, damage or auxiliary particle definition. The VFX is385723 bytes, version`0x40006`, with32 chunks: sevenSFXO, tenPART and fifteenMATL, end frame90. All chunks are required before publishing the owner.
- Mesh channels: BoomPlane01/02 plus OBJ_Ship_Debris05/01/02/03/04;320 vertices and936 faces. The export contains91 samples per animated channel. Mesh reconstruction reuses the existing VFX core rather than another scene mesh.
- PART loader `0x5420f0` and interpolation `0x542ba0` establish the13-float sample, linear position/scalars and quaternion interpolation `0x519da0`. Emission credit uses frame integration times320 (`0x540bf0`); negative initial emission begins at`0x542b1f`. Lifetimes convert4800 ticks/second; velocity/spread convert by12. Shape0 is rectangular, shape1 spherical (`0x540e80`). Flag`0x10` supplies random angle (`0x541233`); flag`0x100` is the velocity streak (`0x5575ce`). Size/alpha envelopes are from`0x5574d9..0x5575ab`. This export has no external force records and zero lifetime variance.
- Track capacities are100,100,100,100,60,20,100,100,15,15, totaling710. All ten tracks, including both streak tracks and the two debris-associated trails, are retained.
- `VParticle02` names Debris05 and `VParticle12` names Debris01. All91 samples are already in effect coordinates: maximum emitter/parent-center separation4.399097 versus bound10.969952, and1.470788 versus4.521163, respectively. Root propagation at`0x54ce13`/`0x54cebf` applies the effect basis. The loader resolves each parent, requires the expected morph/timing, and checks the entire sampled bound. It does not apply debris motion a second time.
- Materials request13 unique textures comprising150 decoded frames: eleven TGAs plus96- and43-frame VBMs. MATL13 is flat color. MATL10 also requests `Sub_P05` with reflection strength0.33; that channel is loaded and drawn.

## Stock64 accounting and ownership

Hard total resource/loading ceiling is4MiB, including a128KiB maximum for mutable owner/instances and scratch. Optional admission runs after required effect, lighting, terrain, checkpoint and actor-restore allocations, before startup dispatch, and requires retaining4096 available native pages (16MiB) for later frame owners. Its tables archive is reopened only on demand and closed through the existing read-only VPP cleanup path. All image dimensions/frame counts are preflighted before texture allocation; every native image is charged to a4096-byte page. Resources are admitted as a complete presentation or freed completely. No lower-resolution fallback or partial channel omission is used.

These are **32-bit source-layout estimates**, not measured runtime or compiled `sizeof` results; runtime admission uses the actual compiled sizes and loader allocation reports:

| Charged storage | Bytes |
| --- | ---: |
| Owner, private particles/tracks, globals and callback/context |27,356|
| Allocator/page-fragmentation allowance |16,384|
| Owned mesh data/frame views |407,923|
| Mutable mesh instances |26,584|
| Material bank and tracks |5,914|
| PART sample arrays |47,320|
| Material bindings/texture descriptors |3,960|
| Decoded pixels including white texel |2,854,916|
| Image frame descriptors |3,000|
| Native pixel page padding |4,092|
| Total resident charge |3,397,449|
| Loading peak charge including8KiB shared stack/scratch allowance |3,405,641|
| Mutable/runtime charge including instances, allowances and scratch |78,516|

The loader also charges the directory, largest temporary raw chunk and table scratch while live; their phase peaks are lower than final texture loading. The original full-size textures fit. Generic VPP bitmap readers stream through their existing4KiB reader; no full-file bitmap staging buffer is introduced. Foley borrows the existing bounded sound service and is outside this visual owner.

Ordinary final teardown closes instances, meshes, tracks, textures, materials and the white image. Draw/sink failures propagate unchanged through the renderer particle pass, which ends the batch and waits for GPU idle before returning an error; successful frames finish through the HUD idle fence before owner retirement. Construction failures occur before GPU submission. A retained owner prevents the session-idle gate from succeeding.

Only the final-success `scene_world_load` boundary resets active transient playback, after publication, storage close and staged-owner release. Failed loads preserve it. No save bytes or codec version are added: pending type70 events retain their ordinary timeline; an already playing visual is transient. Type70 now advertises the existing audio/visual external checkpoint requirements.

## Remaining approximations

- The resource reader deliberately admits only the inspected transport export and its format envelope. Other/new clips refuse cleanly instead of silently dropping unsupported channels.
- Velocity streaks preserve authored direction and length through the shared stretched-quad projector; original triangular taper/rasterization remains a refinement.
- Material animation, opacity, brightness, tint and additive modes are used. Lighting uses existing ambient/directional fallback; dynamic point-light/specular matching is not reconstructed here. Reflection uses a first-pass spherical view projection rather than claiming original texture-stage parity.
- Short-step midpoint emission integration, private random draws, particle birth-age distribution, translucency ordering and room-independent effect culling are practical first-pass approximations. No retail visual equivalence is claimed.
- The original fixed-size meshes, callbacks, projection and renderer path are source-integrated only; the parent owns compile fixes and the next startup smoke, while testers own functional coverage.

`rf_scene_play_vclip[24]` exposes demand/admission/status, resident/peak/runtime charges, pixels/padding/descriptors, track/geometry costs, free pages, starts/refusals/births/drops/live count, draw submissions, last UID, retirements, successful-load resets and resource failures. It creates no per-frame output files.
