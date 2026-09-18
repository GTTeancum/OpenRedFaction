# Scoped corner-sweep validation — 2026-09-18

## Change

The previous sphere batch reduced active debris-update time to75.532ms in XEMU, leaving2411ms of corner work over79 active ticks. Each corner/quarter-pose cast still began a separate geometry call and repeated invariant world validation.

`rf_geometry_collision_body_sweep_batch` accepts a caller-owned validation batch, while the ordinary and textured APIs retain automatic per-call lifetime. `scene_detached_mesh_sweep` creates a zero-initialized batch on its stack and shares it across all corner/quarter-pose casts for that one mesh sweep. It is discarded before returning. No cache survives fragment-query completion, publication, or a later physics update. Movers are freshly prepared for every call; no contact, material result, alpha result or mover pose is cached.

This uses the existing224-byte bounded batch on Xbox. It does not skip corners, lower physics frequency, change the four angular intervals, reduce triangle coverage, or change narrow-phase calculations. Existing cache-overflow fallback and immutable-owner requirements apply.

## Verification

The geometry overlay test compares32 caller-batched body queries against the ordinary route across different rooms, radii, hit limits and misses, requiring identical hit payloads and metadata callback counts. Existing room-level48-case/17-tree overflow and malformed/reset tests remain. Final complete PC build and all123 tests pass.

An initial harness attempt (`render-20260918-031042`) stopped before Xbox launch because the PC executable was locked by ongoing replay processes while linking. Those processes finished; the final executable was then rebuilt and all tests and connected histories rerun. Earlier replay results are not used as evidence for this revision.

Final native run `artifacts/xemu/render-20260918-031158` passes79 checks at600 frames on stock64MiB, with3325 free pages. All deterministic work counters and64 native contact-fixture words match PC. Both connected histories pass; the first-cut save remains unchanged. The disc was restored and the owned emulator closed.

| Measurement | Per-body batch (`030512`) | Corner batch (`031158`) |
| --- | ---: | ---: |
| Active total,79 ticks | 5967ms | 5336ms |
| Mean active update | 75.532ms | 67.544ms |
| Maximum active update | 159ms | 139ms |
| Corner stage total | 2411ms | 1636ms |
| Sphere stage total | 3338ms | 3478ms |

Corner time decreases32.14%; total active update time decreases10.58%. The three optimizations together reduce the original125.253ms mean by46.07%. Sphere time increased slightly in this single-run comparison; timings vary, and no whole-game FPS or hardware claim is made. Approximately67.5ms per active debris tick remains too expensive. Next isolate remaining sphere narrow-phase cost before choosing another major optimization; edge/edge contact and mover-relative response remain outstanding fidelity work.

Final native checkpoint and framebuffer are byte-identical to the prior accepted run. Checkpoint SHA256: `d27cfa99dae3f1595e695e85f32b777a8f8b856017c85b944cab34c241601aba`. Framebuffer SHA256: `ad18ceadb48c89b8c98548d169a7e9bc51b785197a4ac223c326bcc9bab41ca3`; this matches the previously inspected destruction endpoint, so no new screenshot was uploaded.

First- and second-cut floor audits also pass0.005 penetration and clearance gates, preserving ordinary continuation saves.
