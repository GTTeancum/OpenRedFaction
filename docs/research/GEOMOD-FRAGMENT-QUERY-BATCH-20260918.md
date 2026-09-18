# Scoped sphere-query validation — 2026-09-18

## Problem and change

Native stage profiling in `render-20260918-025903` measured5245ms of sphere-sweep work and2415ms of corner-sweep work over79 active debris ticks. The sweep path rescanned all room/list descriptors and every node in candidate trees separately for each sphere. This change shares successful validation across the dry spheres of one `rf_geometry_collision_body_sweep` call.

`rf_collision_sweep_batch` holds one room/list identity and16 node-array/count records. The geometry body context zero-initializes it on the stack for every call and discards it on return. On32-bit Xbox it occupies224 bytes; there is no allocation, persistent memory growth or retained contact. A full cache falls back to normal validation for additional trees. Room/list identity changes clear the cache. The caller must keep geometry immutable for the batch, including metadata callbacks; terrain edits, replacement and destruction publication require a new batch.

Public ordinary room/tree sweep APIs still validate normally. Alpha/textured and liquid queries retain their existing uncached paths. Every query still checks numeric inputs, face data, traversal capacity and cycle/visit limits; only successfully checked invariant room/list/node fields are reused. Sphere order, candidate order, equal-time replacement, narrow-phase math and material lookup remain unchanged. This is a port optimization, not newly recovered original-game behavior.

## Verification

The existing collision-room test now compares48 batched versus ordinary sweeps across17 trees, varying radius, fraction limit, first-hit policy and hit/miss. It checks equal-time output identity,16-entry overflow, invalid radius rejection, invalid uncached seventeenth-node rejection, invalid edited node/room after reset, and invalid replacement primary-list rejection. All123 PC tests pass.

Both source92/source108 full connected destruction histories pass, including protected-trim rejection, both junctions, uninterrupted versus reloaded continuation. First- and second-cut floor audits pass0.005 penetration/clearance limits with identical continuation saves.

Native `artifacts/xemu/render-20260918-030512` passes79 checks and600 frames on stock64MiB, including64 numerical contact-fixture words and the unchanged deterministic work ledger. Available memory remains3325 pages. The owned emulator closed and disc inputs were restored.

| Measurement | Before (`025903`) | Batched (`030512`) |
| --- | ---: | ---: |
| Active update total,79 ticks | 7856ms | 5967ms |
| Mean active update | 99.443ms | 75.532ms |
| Maximum active update | 218ms | 159ms |
| Sphere stage total | 5245ms | 3338ms |
| Corner stage total | 2415ms | 2411ms |
| Reciprocal world stage total | 129ms | 149ms |

Measured active update time decreases24.05%; sphere time decreases36.36%. The original pre-cache baseline was125.253ms average, so the combined two optimizations reduce that measured average by39.70%. Timings are guest milliseconds in XEMU, not real-hardware or whole-game FPS claims; comparisons are single bounded runs. Remaining75.5ms average is still too high for final gameplay. The next target is shared validation across a fragment's many corner sweeps, followed by remaining sphere narrow-phase cost.

The5300-byte Xbox checkpoint equals PC and baseline (SHA256 `d27cfa99dae3f1595e695e85f32b777a8f8b856017c85b944cab34c241601aba`). The framebuffer equals the previously inspected baseline byte-for-byte (SHA256 `ad18ceadb48c89b8c98548d169a7e9bc51b785197a4ac223c326bcc9bab41ca3`), preserving the room, weapon/HUD and destruction endpoint. No new GitHub screenshots.
