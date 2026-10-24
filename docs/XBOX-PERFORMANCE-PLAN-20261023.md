# Xbox performance plan — October 23, 2026

Scope: stock 64 MiB retail-mode Xbox (debug kit at 10.0.0.231, XBE limited to
64 MiB, confirmed by MmQueryStatistics 16384 pages). Measurements are from the
paced replay harness (`--paced`) with the per-phase profile in the RAM log;
`tools/crash_profile.py` prints them per sample.

## Sources studied

- XDK 5558 `XboxSDK.chm`: *Xbox Graphics Performance Tuning Strategies*
  (Harding), *Xbox Vertex Performance* (Abrash), *Maximizing Xbox File
  Performance* (Dougherty), Begin/End blocks, push-buffer resources,
  allocating resources, fences (`InsertFence`/`BlockOnFence`).
- NXDK `lib/pbkit`: push buffer (default 512 KiB, `pb_size`), triple back
  buffers flipped by the VBlank ISR, `pb_busy`/`pb_wait_until_gr_not_busy`,
  no fence or vertex-buffer abstraction. Anything Direct3D provided (fences,
  dynamic buffers, static push buffers) must be built on raw NV097 methods.

Guidance that applies directly:

1. Find the bottleneck stage (CPU, geometry, raster) before optimising; use
   ms/frame, not fps; experiment by removing work (null GPU, small viewport,
   null textures). Repeat as the bottleneck moves.
2. Never stall the CPU on the GPU: double-buffer dynamic data behind fences
   instead of locking a resource the GPU is still reading (OBJECTLOCK waits).
3. The GPU transforms far faster than the 733 MHz CPU. CPU-generated
   geometry crosses a ~1 GB/s front-side bus and should be occasional, not
   every frame; static meshes belong in GPU memory, drawn indexed/stripped.
4. Textures dominate bandwidth: use mipmaps whenever minified, swizzled
   layouts, and compressed (DXT1 4 bpp) or palettised formats; disable blend
   and depth writes when not needed; draw front to back.
5. Small vertices (compressed formats, packed colour) cut bandwidth and memory.
6. File I/O: large, sector-aligned, non-buffered, asynchronous reads into
   contiguous memory; native `CreateFile`/`ReadFile`, not stdio.

## Current renderer versus that guidance

| Area | Current behaviour | Evidence |
| --- | --- | --- |
| GPU transform | Static world, rigid props and skinned NPCs are retained and transformed by vertex programs (`world_vertex`, `rigid_vertex`, `skinned_vertex`). Movers, GeoMod/terrain, debris, projectiles/effects and other dynamic geometry are projected and clipped on the CPU into screen space every drawn frame (`preview_vertex` is a pass-through). | L2S1 world build 10.7–11 ms/drawn frame, L1S2 16.9 ms, all in `rf_preview_update_world_visible` (`b:project`), for at most 3–5k vertices. |
| Per-mover work | Every mover runs a collision query (`rf_collision_contact_world`) and an object-render dispatch every build, before culling. | L3S2: 27 movers, 21–27 culled, 42–924 vertices emitted. |
| CPU/GPU overlap | One streamed vertex buffer. `preview()` spins on `pb_busy()` before writing it, the skybox pass drains the GPU, and the HUD pass ends with a full fence. The CPU waits for the GPU several times per frame; the two never work in parallel. | `renderer.c` `while(pb_busy())` at stream start, after skybox, after audits/HUD. |
| Vertex format | 56-byte all-float vertex (3 position, 3 colour floats, 3 UV, material, 3 lightmap UV, lightmap tag) for streamed geometry; non-indexed triangle lists, 3 vertices per triangle; the whole streamed mesh is rewritten to write-combined memory each drawn frame. | `rf_preview_vertex`, `preview()` copy loop. |
| Textures | Swizzled A8B8G8R8 (or A1R5G5B5) with `MIPMAP_LEVELS 1`: no mipmaps anywhere, no compression. Minified distant surfaces thrash the texture cache, and 32 bpp storage costs stock-64 memory. | `upload()` format word. |
| Presentation | One frame start per VBlank; paced clock now skips the world build for undrawn ticks (this session) and draws at least 20 Hz when behind. | L2S1 slow stretch 25 → 34 steps/s. |
| Loading | stdio `fopen`/`fread` through pdclib buffering. | First gameplay sample about 80 s after launch on hardware. |

## Plan, in order

Each step is measured on the console before and after with the same paced
recipes (L1S2, L2S1, L3S2 at minimum) and reported as ms per drawn frame and
game speed.

1. **Bottleneck instrumentation.** Add CPU time spent waiting on the GPU per
   frame, GPU-busy share, and flag-file experiments from the XDK method: skip
   GPU submission (null GPU), quarter-size viewport (fill-bound test), 1×1
   textures (bandwidth test). Output: which stage limits each slow view.
2. **CPU/GPU overlap.** Ring-buffer the streamed vertex memory (two or three
   segments) guarded by GPU-written fence values (NV097 semaphore release to a
   notifier word), replacing whole-GPU idle waits. The CPU builds frame N+1
   while the GPU draws frame N. Same treatment for particle/HUD batches.
3. **Retain dynamic geometry on the GPU.** Upload mover geometry once and draw
   it with a per-mover matrix through the existing world/rigid vertex
   programs; move per-mover culling ahead of any collision query and drop the
   render-path collision call. Then GeoMod/terrain meshes (re-upload only when
   an edit publishes), effect/projectile VFX and remaining CPU-projected
   models. Goal: CPU world build near zero; the pass-through path only for
   genuinely per-frame procedural geometry.
4. **Mipmaps and compressed textures.** Build swizzled mip chains at load
   with a box filter, enable mip filtering; evaluate DXT1 for opaque world and
   model textures within the stock-64 texture budget (DXT1 plus mips is still
   well under the current 32 bpp cost).
5. **Leaner vertices and indexed draws.** Packed colour, 16-bit texture
   coordinates where exact, indexed triangle lists or strips for retained
   world and models; keep CPU-generated data minimal.
6. **State and batching.** Sort by material/blend, stop reloading vertex
   programs per particle batch, merge per-face effect draws.
7. **Loading (separate track).** Large non-buffered `ReadFile` into
   contiguous buffers, overlapped where possible, to cut level load time.

The hardware crash sweep (10 of 68 levels done) resumes after the frame-rate
work, as directed.

## Results — October 23 (steps 1–3 partial, 7 partial)

Console, paced exercise replay at the authored start, 1800 inputs; median over
live samples. Before = per-frame CPU world build skip and 20 Hz draw floor
only. After = position-fenced overlap, GPU-retained movers, view-model GPU
path, post-cull model draw limit, archive index cache and large reads.

| Level | Steps/s before → after | Slowest | Drawn/s | ms per drawn | GPU wait ms/drawn |
| --- | --- | --- | --- | --- | --- |
| L1S2 | 26.2 → 43.9 | 16.1 → 30.0 | 16.6 → 16.0 | 60.2 → 62.7 | 4.2 → 0.0 |
| L2S1 | 59.6 → 59.9 | 31.0 → 58.9 | 20.9 → 59.8 | 47.8 → 16.7 | 3.6 → 0.8 |
| L3S2 | 47.0 → 59.9 | 34.4 → 56.2 | 16.9 → 44.4 | 59.0 → 22.5 | 2.7 → 0.0 |

Bottleneck experiments (before build): null draws barely changed L1S2/L3S2
(CPU-bound) but cut L2S1 48 → 19 ms per drawn frame (CPU draw preparation);
a quarter-area raster clip cut L2S1 48 → 32 ms (fill competes for shared
memory bandwidth); 1×1 textures saved ≤4 ms anywhere. Mipmaps are therefore
parked (`local/parked-mipmaps`, needs initializer updates and an LOD-clamp
check on hardware) until fill becomes the limit.

Load to first frame (console): L1S2 60.7 s, L2S1 41.3 s, L3S2 49.9 s, of which
disk wait 7–9 s; the remainder is CPU decode/parse (next loading target).

Hardware findings: an NV097 semaphore DMA context (pbkit context 3) raised a
GPU protection error on real NV2A that XEMU accepted, so fences are command
stream positions retired by DMA get distance or GPU idle. Skipped paced ticks
must restart the retained-model queue (`rf_xbox_frame_skipped`).
Open: the D:\render-overlap.bin bisect switch was not observed on hardware
(all runs reported mode 0); L1S2 remains CPU-bound at ~73% speed.

## Results — October 24 (L1S2 frame cost and loading)

Per-step stage timers (prestep, physics and animation splits) located the
remaining L1S2 costs on the console:

- Parked campaign vehicle ran full rigid support/collision every tick (2.9 ms)
  and never settled (residual angular momentum). Resting chassis now sleep after
  two still steps, clear residual motion, wake on command or force, and run a
  full step every 30 ticks (1738 of 1799 steps skipped on L1S2).
- Undrawn paced ticks ran the whole draw-preparation pass; they now skip it and
  only age debris (48fd70 ages at draw submission).
- The controller commit software-mixed 800 PCM frames per voice per tick and
  discarded them (no software sink on Xbox). rf_audio_advance applies the
  identical position arithmetic in closed form (0/2000 mismatches against
  rf_audio_mix in a host test); full mixing remains with a sink or checksums.

Loading, by timestamped campaign and late-load stages:

- Light regeneration scanned every face per lightmap mapping to size work
  storage (24.7 s); faces are bucketed by mapping once (identical counts).
- entities.tbl re-read per lookup for base motions (~150 MB, 6.9 s): class
  index and per-class records; motions opened once per load.
- clutter.tbl re-scanned per class (81 MB, 6.0 s) and weapons.tbl read 48
  times: class index, scoped table-text cache, faster tokenizer.
- Texture decode per pixel through swizzle addressing and per-byte checksums
  over write-combined memory: row/band decoding, shared first-person hand
  textures, word-read digests, linear alpha classification.
- Authored GeoMod identity SHA-256 (~60 MB): faster identical SHA with a
  start-up self-test (6.0 → 4.1 s; memoisation would remove most of the rest).

| Level | Steps/s median (slowest) | Drawn/s | Load to first frame |
| --- | --- | --- | --- |
| L1S2 | 59.5 (42.9) | 20.7 | 19.2 s |
| L2S1 | 59.9 (59.0) | 59.8 | 14.3 s |
| L3S2 | 60.0 (57.4) | 51.7 | 19.2 s |

Remaining L1S2 per-step costs: simulation ~9 ms (NPC AI/animation ~4.7, physics
~2.8), world ~2.7, draw preparation on drawn ticks. Loading remainder: GeoMod
identity hashing 4 s, renderer first frame 2.4 s, weapon views 1.6 s.

## Results — October 24 afternoon (L1S2 drawn frame rate)

Profile (console, paced exercise replay): per game step about 12 ms of simulation (NPC AI/animation ~4-5, physics ~3, combat ~1.5, events ~1.3) plus about 12 ms CPU per drawn frame, of which world particles 0.7 ms and coronas 2.8 ms. Labels 'medic/masako' and 'w:camera' are mostly NPC animation playback and the combat tick respectively.

- Corona occlusion: 220 glares in L1S2, ~16 in visible rooms per drawn frame, each search ~0.18 ms (world ray plus mover/actor occluders). Xbox build re-traces a quarter of the glares per drawn frame (handle-staggered) instead of half; cached results hold in between. PC/shared path keeps the original alternation. Coronas 2.8 -> 2.2 ms per drawn frame; L1S2 drawn fps median 19.2 -> 21.2.
- NPC animation level of detail (Xbox): NPCs in unseen rooms or beyond 20 units pose on alternate ticks with the skipped time carried; visible nearby NPCs keep every-tick advance. Pose advance 2.0 -> 1.3 ms per step, ~40% of advances deferred; L1S2 drawn fps median 21.2 -> 25.1 (game steps stay at 60/s).
- World particle submission now sends each fan as one pushbuffer block, changes only the texture binding when the mode is unchanged, and loads the particle vertex program once per pass. L1S2 issues only ~7 world fans per frame, so this showed no measurable gain there; it reduces command traffic in particle-heavy scenes.
- New RAM-log diagnostics: renderer phase profile (r:/d: rows in tools/crash_profile.py), particle/corona time split, corona search cycles, NPC pose LOD counters.

Remaining L1S2 costs per step: NPC AI ~1.5, animation ~1.3, physics ~2.8 (crusher/conveyor movers and per-tick audio commit), combat ~1.5, events ~1.3; per drawn frame: corona submission ~1.2 ms beyond searches, world/model draws ~2.5 ms.

- Drawn-frame split (L1S2, per drawn frame): world+movers 1.5 ms (indexed path; draw-arrays fallback measured 2.3 ms), models/stream 1.9 ms, particles+coronas 2.8 ms. Corona parent visibility cost 0.9 ms because its first call built the glare snapshot, copying every clutter prop every frame; clutter is never an occlusion candidate, only a handle lookup, so it now joins the snapshot on first lookup. Snapshot 0.88 -> 0.25 ms, coronas 2.1 -> 1.5 ms per drawn frame, L1S2 drawn fps median 25.9.
- Retained models: 77 model renders per drawn frame on L1S2 (~1.5 ms). Opaque draws within each CPU-boundary run are now stably sorted by shader kind and texture (translucent draws keep their order last) and each draw uses one command block. Models 1.51 -> 1.34 ms per drawn frame; drawn fps median 26.2. Remaining per-draw cost is mostly bone palette upload.
