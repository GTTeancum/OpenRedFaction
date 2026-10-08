# NPC frame-cost reduction

Source written 2026-10-08 against c36f6cd; no helper build/test/emulator run.
Parent owns the 16:00 consolidated check. Runtime gains remain unmeasured.

## Recorded baseline, not a new measurement

Parent-provided L1S1 optimized timing in
artifacts/hourly/20261008-1500-optimized/performance.json records NPC
playback/pose/collision at 8.946 ms, including pose advance/evaluation at
5.475 ms and collision preparation/hash at 2.139 ms. Later parent observation
at 15:20 reports 11.5 presented XEMU FPS on the original neutral L1S1 spawn,
stock 64 MiB. These phase timings identify work to remove; they are not
promised gains and are not a post-change benchmark.

## Changes

1. Live NPC/corpse skinning uses the existing owned collision-prepared matrices
   for rendering as well as collision. Previously each pose advance eagerly
   composed every collision matrix, and each visible draw composed the same
   bind-by-pose matrices again in a fresh stack array.

   With diagnostic checksums disabled, publishing an evaluated pose now marks
   the prepared cache stale rather than composing it eagerly. The first actual
   render or collision consumer prepares it; later consumers reuse its exact
   generation-tagged matrices. Every publication invalidates the stamps to
   generation minus one, so 16-bit wrap cannot resurrect an ancient cache.
   This adds no heap allocation and preserves per-owner skeleton/count checks.

   Fully current evaluated poses alone use this path. A partial/restored pose
   retains the original scratch-render fallback rather than adding a new
   rejection or changing when animation advances. Normal restore already
   invalidates the prepared stamps; model retirement frees the cache; corpse
   transfer retains the same skeleton/pose identity. Diagnostic mode retains
   eager cache hashing and the original render scratch path.

2. Motion sampling keeps its validated bone-track descriptor for the duration
   of one sample. Up to four rotation/position key reads no longer each reread
   and revalidate the same track metadata. Public indexed accessors still
   validate independently; key bounds, byte bounds, decoding, interpolation,
   finite-value checks and output-on-failure behavior remain. The existing
   immutable-file contract is unchanged. There is no persistent file cache.

3. The existing 16-entry per-tick shared-pose cache compares only exact inputs
   consumed by skeletal sampling: skeleton, bone count, active count, each live
   motion/tick/weight tuple, primary slot and root displacement. Unused slot
   bytes and freeze/dominant selector history no longer cause false misses.
   Overrides and partially current poses still bypass sharing. Every actor
   still advances its own controller, events, references and playback first.
   The cache is still reset per tick; no cross-tick lifetime is introduced.

## Preserved behavior and pending validation

No change to NPC visibility, active actor count, simulation frequency, animation
cadence, motion interpolation, AI, collision shapes, damage, save payloads,
resolution, materials or submitted geometry. This is redundant-work removal,
not a reduced-fidelity mode.

No tests were run by this helper. Existing shared_pose_evaluation and motion
sampler/model probes can check arithmetic/parity in the parent batch. Compare
the same live 64 MiB L1S1 scene and inspect actual presented FPS plus phase
costs. Include ordinary combat/body contacts and save/load if practical because
the live prepared-cache path deliberately differs from diagnostic eager work.
