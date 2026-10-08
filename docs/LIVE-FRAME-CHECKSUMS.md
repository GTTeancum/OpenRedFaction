# Live frame diagnostic checksums

Status: Source-written on 2026-10-08 from commit 436ed14. No compile, test,
benchmark, emulator or UI run by this helper; parent owns the 15:00 batch.
No measured FPS or claim that this alone explains the reported slowdown.

## Scope and evidence

Shared scene code unconditionally traversed large payloads to publish FNV parity
hashes on every frame: projected world geometry, player geometry twice, NPC
playback/bone matrices and skin caches, other projected meshes, mixed audio PCM,
light clocks, ambient/force/switch snapshots, particle vertices, and active
particle records after a full 1,600-slot scan. These destinations are diagnostic
outputs. Counts, timestamps, positions and gameplay flags remain separate.

The new rf_scene_set_diagnostic_checksums(enabled) API defaults to enabled.
An interactive port can disable these payload scans before starting a scene.
The opt-out only changes diagnostic hash fields; simulation, input, rendering,
audio mixing, RNG, allocation, existing validation, save integrity and lifetime
rules remain intact. Timing and gameplay/state rings remain available.
rf_scene_diagnostic_checksums_enabled exposes the mode so disabled payload
hashes cannot be mistaken for complete parity evidence. Small scalar/ring hashes
and fixture/integrity comparisons are deliberately unchanged.

Xbox wiring is a separate integration change: select disabled for normal paced
live controls, enabled for deterministic replay and existing bounded checks.
Do not remove or weaken npc_hash_bytes generally: it also supports integrity and
fixture comparisons, so only identified runtime diagnostic sites use the gate.

## Measurement boundaries

The Xbox retained-world backend already omits static world CPU projection when
it accepts the frame, so the world hash alone can be small there. This change
also removes independent NPC, particle and audio payload scans. The Xbox target
already uses -O2. Actual benefit needs parent-owned comparable scene timings;
source inspection cannot establish the dominant bottleneck or an FPS gain.

The parent batch should preserve enabled-mode checksum outputs and compare live
movement/aim/combat and frame timings with checksums disabled. Preserve useful
state counters and report whether the hardware/emulator path was measured.
