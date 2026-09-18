# Fragment collision work attribution — 2026-09-18

## Scope

The shared collision primitives expose8 process-wide diagnostic counters: segment/box calls, sphere/plane calls, polygon containment, sphere/edge calls, nodes validated, nodes traversed, sweep-face calls, and primary rooms considered. Fragment query stages accumulate unsigned before/after differences into32 words, reset at frame zero and counted after the existing16-frame warm-up. Counter wrap is handled by unsigned subtraction. These counters do not control collision or serialization. Scene collision and its shared scratch are serialized; this is not a thread-safe profiler for concurrent callers.

The PC replay prints `FRAGMENT_STAGE_WORK`; the XEMU harness compares all32 words exactly and adds `fragment_stage_work` to the report. Counts measure operations, not their individual CPU times. The prior stage timers remain the timing evidence.

## PC observations

For the same source108/three-source/600-frame first cut:

| Operation | Sphere stage | Corner stage |
| --- | ---: | ---: |
| Segment/box calls | 2185097 | 995865 |
| Sphere/plane calls | 10260 | 0 |
| Polygon containment calls | 8488 | 30 |
| Sphere/edge calls | 4300 | 0 |
| Nodes validated | 116608 | 29392 |
| Nodes visited | 178715 | 74240 |
| Sweep-face calls | 2529020 | 1190517 |
| Primary rooms considered | 1298944 | 659448 |

The reciprocal world stage adds24984 segment/box calls; the other recorded operations are zero there because it uses thin-face triangle queries directly. This replay has no reciprocal-mover primitive work.

Millions of face/box checks versus only10260 sphere-plane and4300 edge calls make candidate pruning a stronger next hypothesis than rewriting the edge solver. Counts alone do not prove time spent per primitive, nor identify whether tree-leaf or flat-mover faces dominate. Next separate those candidate sources and reduce broad-phase work while retaining source order, equal-time contacts and invalid-input behavior. Do not drop spheres or corner tests to make the benchmark faster.

All123 PC tests pass with instrumentation. Native `artifacts/xemu/render-20260918-031829` confirms all32 counts exactly, passes80 checks and600 frames on stock64MiB, and retains the identical PC/Xbox5300-byte checkpoint and previously inspected framebuffer. The harness restored its disc and closed its emulator. Endpoint free pages3324.

The diagnostic build measured85.582ms average active debris update, versus67.544ms before instrumentation. This is substantial overhead/variation, not an optimization result. The counters were removed from the normal source and retained as an optional reproducible patch at `tools/profiling/fragment-inner-work.patch`, with application/removal instructions. Production collision, scene, PC output and harness files were restored byte-for-byte fromc788c9c2 and rebuilt. Do not count this turn as an FPS improvement.

Checkpoint SHA256: `d27cfa99dae3f1595e695e85f32b777a8f8b856017c85b944cab34c241601aba`. Framebuffer SHA256: `ad18ceadb48c89b8c98548d169a7e9bc51b785197a4ac223c326bcc9bab41ca3`. No new screenshots uploaded.
