# Ordered face-block pruning — 2026-09-18

## Evidence and candidate

A PC follow-up to the native work profiler moved its sweep-face counter to the flat-mover traversal loop. The same600-frame source108 replay produced zero flat-mover face attempts in both sphere and corner stages, while all other work counts and the exact5300-byte save remained unchanged. Thus the measured2.53M/1.19M face attempts are static world-tree work for this replay. Evidence: `artifacts/fragment-face-source/replay.log`. This attribution is a PC probe using shared code, not a second native counter capture.

The candidate adds64 direct-mapped entries to the existing query-scoped validation batch. Each entry caches the union bounds of up to16 consecutive faces, after their existing query filter. A segment whose endpoints are outside the expanded union on the same side cannot hit any face in the group; that group is skipped. All potentially intersecting groups retain the original face loop, ordering, narrow-phase arithmetic, tie replacement and metadata behavior. Empty filtered groups are skipped. Cache keys include face pointer/count and query flags; owners remain immutable for the batch lifetime.

Bounds/filter errors mark a group uncacheable without immediately returning an error. The original per-face loop then preserves error timing and first-hit behavior. Unsupported alpha cases fall through as well. Face vertices, planes and contact results are not cached. Cache replacement changes cost only; no geometry is truncated. Xbox batch size rises from224 to2784 stack bytes, with no heap allocation or retained cross-publication state.

## Validation

All123 PC tests pass. Additional64-face fixtures compare32 ordinary versus batched casts across radius, fraction limit, first-hit and equal-time policies; they also check distant malformed filters/bounds cannot disappear behind pruning, and an early first hit still precedes a later malformed face in the same group. Both source92/source108 destruction histories pass, including second cuts and uninterrupted versus reloaded continuation.

Native `artifacts/xemu/render-20260918-032713` passes79 checks over600 frames on stock64MiB, including the shared64-word contact fixture and unchanged semantic work ledger. Endpoint headroom is3324 free pages (one page below the prior run). The owned emulator closed and disc inputs were restored.

| Measurement | Corner-batch baseline (`031158`) | Ordered face blocks (`032713`) |
| --- | ---: | ---: |
| Active update total,79 ticks | 5336ms | 2702ms |
| Mean active update | 67.544ms | 34.203ms |
| Maximum active update | 139ms | 69ms |
| Sphere-stage total | 3478ms | 1663ms |
| Corner-stage total | 1636ms | 831ms |

Measured active update time falls49.36%; the cumulative reduction from the original125.253ms baseline is72.69%. These are single bounded XEMU runs of the destruction subsystem, not whole-game FPS or real-hardware measurements. Current34ms active-debris cost still leaves performance work, but this is sufficient progress to return attention to missing edge/edge and mover-relative contact behavior.

The5300-byte native checkpoint equals PC and prior accepted bytes (SHA256 `d27cfa99dae3f1595e695e85f32b777a8f8b856017c85b944cab34c241601aba`). The native framebuffer is byte-identical to the previously inspected endpoint (SHA256 `ad18ceadb48c89b8c98548d169a7e9bc51b785197a4ac223c326bcc9bab41ca3`). No new screenshot uploaded.

The temporary flat-mover attribution counters were removed before building this candidate. Normal production code contains no expensive inner-work profiler. The optional patch remains available for later investigation.

First- and second-cut floor audits pass0.005 penetration and clearance gates, with audit saves equal to ordinary continuation. The optional profiling patch still passes `git apply --check` against this implementation.
