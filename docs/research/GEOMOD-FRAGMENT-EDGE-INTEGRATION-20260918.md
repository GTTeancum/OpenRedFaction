# Fragment world/mover edge contacts — 2026-09-18

## Integration

The isolated swept-edge primitive is now part of ordinary fragment queries. Static world and committed-mover traversals test polygon boundary edges after their existing reciprocal vertex tests. Candidate order remains stable and only strictly earlier contacts replace a prior hit. Both routes preserve face/source IDs, textures, material response, mover handles and committed velocity. Material failures preserve the caller's hit. World room skips and existing0x464 face admission still apply.

The piece mesh prepares unique undirected boundary edges at the same five poses used for four corner-sweep intervals. A bounded128-edge shared scratch holds transformed endpoints and four swept AABBs (28704 bytes including the overall bounds/header on Xbox). Overall and interval bounds reject distant stationary edges before the continuous solver. The scratch is rebuilt on each query; oversized meshes use complete uncached traversal, with no truncation. This is serialized scene scratch, not a cache retained across publication. No per-frame allocation.

A contact at the end of one quarter interval is deferred by the primitive's strict limit to the next interval's time0. The supporting-side test therefore also permits the previous interval's front-side extent for that exact boundary case. Initial back-side birth contacts remain excluded. Contact normals must align with the stationary face's one-sided normal.

This adds edge/edge coverage to the existing sphere/corner/reciprocal-vertex composition. It still uses linear endpoint chords over four intervals, not exact rotational CCD. Persistent coplanar/parallel overlap, moving-platform relative motion and wake/carry/crush behavior remain separate work. Mover geometry comes from committed position/input_matrix, not ray public/output pose.

## Verification

All124 PC tests pass. The crossed-beam world fixture now hits through the production scene adapter at0.25, with the expected normal, face17 and material3. Nearer contacts and failed metadata stay unchanged; skipped rooms still miss. The shared startup audit additionally exercises translated and90-degree-rotated committed movers, including preserved velocity and material failure.

The edge scratch overflow test uses129 distinct triangles; only the final uncached triangle can hit. It verifies the hit after capacity128 is exceeded, then edits the same storage and verifies the old contact disappears. Both authored connected destruction histories pass, including protected details, both junctions and exact interrupted/uninterrupted continuation. The source108 first-cut save remains unchanged.

A separate32-word `FRAGMENT_EDGE_AUDIT` ledger records version/count/status/success, three seven-word world/mover hit rows, and a material-failure control. The existing optional fixture flag invokes it without changing the loaded room. The harness compares all32 words to PC and independently checks fractions, normals, identities and materials; the older64-word audit remains.

Native `artifacts/xemu/render-20260918-034636` passes80 checks and600 frames on stock64MiB. All96 contact-audit words match PC, covering existing vertex contacts plus the new world, translated mover, rotated mover and error-control edge rows. Free pages3315 (~12.95MiB); disc inputs were restored and the owned emulator closed.

The added coverage has a measured cost: active debris updates average40.595ms versus34.203ms before edge integration, with peak96ms versus69ms. The reciprocal world stage (now also testing edges) totals517ms versus155ms across the replay. These single-run XEMU subsystem timings are not whole-game FPS or hardware claims. Bounds keep the added cost finite, but wider rotation/coplanar scenarios and performance still need work.

Both first- and second-cut resting-floor audits pass0.005 penetration and clearance gates with exact ordinary continuation saves. The source1085300-byte native checkpoint and endpoint framebuffer remain identical to the prior accepted run: checkpoint SHA256 `d27cfa99dae3f1595e695e85f32b777a8f8b856017c85b944cab34c241601aba`, framebuffer SHA256 `ad18ceadb48c89b8c98548d169a7e9bc51b785197a4ac223c326bcc9bab41ca3`. The unchanged image preserves the previously inspected room/weapon/HUD/destruction endpoint; no new GitHub screenshot.

Next broaden rotating and parallel/coplanar edge scenarios, and implement verified mover-relative response/wake behavior. Campaign remains paused in favor of core DEV gameplay.
