# Admission across continuous compiled cavity faces

Cavity admission previously required every projected cutter corner to fit one compiled window. This rejected a continuous wall/floor solely because the compiler split it into adjacent faces. Admission now also considers a pair of windows with the same authored source face and an exact reversed shared full edge.

The shared edge is omitted from their boundary tests only if every vertex of both windows lies inside all remaining edge half-planes. That establishes a convex union of the two convex polygons; no arbitrary bounding rectangle, gap filling, area-sum assumption or tolerance increase is used. All eight projected cutter AABB corners must fit that union. The cutter and its complete corridor back to the plane still avoid every other authored brush AABB. No allocation, new persistent owner or large temporary polygon buffer is added. Other unions remain unsupported.

The initial wall candidate5964/5965 is separated by authored pillar12815, whose bounds are approximatelyX[-33,-32],Y[-2,18],Z[-2,2]. It must continue rejecting even though the two air-owned windows share a geometric edge. The test explicitly retains that rejection.

A valid seam is floor6093/6094, authored source394, around(-29,-2,.5). The1.1-unit cutter bounds cross the seam and admit with reference6093. An actual template cut removes positive area from both6093 and6094 while all other132 compiled window areas retain their values within the existing0.001 area bound. The original compiled materials and source-face provenance remain checked. A downward ray through the seam becomes clear while a distantZ4floor ray remains blocked. Bounds beyond the outer union edge reject with the output reference unchanged.

This is tested core admission/publication/collision behavior. The live radius4 developer hardness patch at(-33,4,8) does not cover this floor test, and no live seam-spanning rocket or native framebuffer is claimed here. The same admission API is used by scene publication. Broader multi-window, nonconvex, T-junction and neighboring-brush CSG remains open.

Verification: all123 PC tests pass (`artifacts/cavity-seam-tests.log`), and stock NXDK compile/link/XBE/ISO succeeds (`artifacts/cavity-seam-xbox.log`). No new native runtime verification was performed for this change.
