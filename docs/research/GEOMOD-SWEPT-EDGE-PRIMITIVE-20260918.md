# Swept edge primitive — 2026-09-18

## Missing contact and scope

A beam spanningX[-1,1],Z[-0.25,0.25] crosses a floor strip spanningX[-0.25,0.25],Z[-1,1]. None of either rectangle's corners lies inside the other. The unit fixture verifies all four moving-corner casts and all four reciprocal corner casts miss, although the edges meet. This is a concrete gap in vertex/face coverage.

`rf_collision_swept_edge` supplies a shared C primitive for isolated crossings of a linearly moving segment against a stationary segment. It is a port mathematical implementation, not a newly recovered original-game routine. **It is not yet wired into ordinary fragment-world/mover collision.** The next step is to compose it with the existing four fragment pose intervals and conservative candidate bounds, retaining metadata and earliest-hit policy.

## Math

LetA(t)=A0+t*dA, E(t)=B(t)-A(t)=E0+t*dE, F=D-C andR(t)=C-A(t). Coplanarity requires R(t) dot (E(t) cross F)=0. Its coefficients are:

- a=-dA dot(dE cross F)
- b=R0 dot(dE cross F)-dA dot(E0 cross F)
- c=R0 dot(E0 cross F)

Solve the linear or quadratic equation with the cancellation-resistant q form, sort its real roots, then test segment parameters at each root. WithN=E cross F, moving parameteru=((R cross F) dot N)/(N dot N) and stationary parameterv=((R cross E) dot N)/(N dot N). Both must lie in[0,1], and a relative squared-distance residual check rejects inconsistent intersections. The returned normal is normalizedN oriented against dA+u*dE; the point lies on the stationary segment. Zero components are canonicalized so reversing endpoint enumeration does not create signed-zero hash differences.

The limit is strict, matching the fragment candidate policy. Persistent coplanar, parallel, degenerate and zero-normal-speed grazing cases are deliberately left to vertex/face and overlap handling. Linear endpoint chords approximate rotational motion only when composed with pose intervals; this primitive alone does not establish exact rotational CCD or complete collision fidelity.

## Validation

All124 PC tests pass. Cases include the crossed-rectangle miss proof, isolated translation at fraction0.25, miss outside finite edge extents, strict limit0.25 rejection with unchanged output, endpoint-order reversal, reverse travel, stationary motion, persistent coplanarity, malformed-input output preservation, and a quadratic with roots0.25 and0.75 followed by a restricted interval selecting the later physical crossing.

The optional native contact audit remains64 words but advances to version2/eight cases. Its last four words record case7, matched, fraction0.25 and normalY1. The fixture additionally checks all point and normal coordinates before publishing success. This is a numerical primitive fixture; it does not claim a live authored edge-only contact has been integrated.

Native `artifacts/xemu/render-20260918-033609` passes79 checks and600 frames on stock64MiB. All64 audit words match PC, including new tail `[7,1,1048576000,1065353216]` (case7, hit,0.25,+1). Endpoint free pages3324. The harness restores the disc and closes its owned emulator.

The checkpoint and native framebuffer remain byte-identical to the accepted face-block optimization run. This is expected because the edge primitive is used only by the optional startup audit so far. Checkpoint SHA256 `d27cfa99dae3f1595e695e85f32b777a8f8b856017c85b944cab34c241601aba`; framebuffer SHA256 `ad18ceadb48c89b8c98548d169a7e9bc51b785197a4ac223c326bcc9bab41ca3`. Existing inspected visual evidence remains valid; no new screenshots uploaded.

Next: integrate bounded edge candidates into fragment world and committed-mover queries, test the crossed-beam fixture through those scene adapters, and measure native cost before accepting the live path. Moving-surface relative velocity/wake/carry/crush remain separate open work.
