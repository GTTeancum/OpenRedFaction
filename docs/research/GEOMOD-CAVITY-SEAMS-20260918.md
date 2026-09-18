# Admission across continuous compiled cavity faces

Cavity admission previously required every projected cutter corner to fit one compiled window. This rejected a continuous wall/floor solely because the compiler split it into adjacent faces. Admission now also considers a pair of windows with the same authored source face and an exact reversed shared full edge.

The shared edge is omitted from their boundary tests only if every vertex of both windows lies inside all remaining edge half-planes. That establishes a convex union of the two convex polygons; no arbitrary bounding rectangle, gap filling, area-sum assumption or tolerance increase is used. All eight projected cutter AABB corners must fit that union. The cutter and its complete corridor back to the plane still avoid every other authored brush AABB. No allocation, new persistent owner or large temporary polygon buffer is added. Other unions remain unsupported.

The initial wall candidate5964/5965 is separated by authored pillar12815, whose bounds are approximatelyX[-33,-32],Y[-2,18],Z[-2,2]. It must continue rejecting even though the two air-owned windows share a geometric edge. The test explicitly retains that rejection.

A valid seam is floor6093/6094, authored source394, around(-29,-2,.5). The1.1-unit cutter bounds cross the seam and admit with reference6093. An actual template cut removes positive area from both6093 and6094 while all other132 compiled window areas retain their values within the existing0.001 area bound. The original compiled materials and source-face provenance remain checked. A downward ray through the seam becomes clear while a distantZ4floor ray remains blocked. Bounds beyond the outer union edge reject with the output reference unchanged.

This is tested core admission/publication/collision behavior. The live radius4 developer hardness patch at(-33,4,8) does not cover this floor test, and no live seam-spanning rocket or native framebuffer is claimed here. The same admission API is used by scene publication. Broader multi-window, nonconvex, T-junction and neighboring-brush CSG remains open.

Verification: all123 PC tests pass (`artifacts/cavity-seam-tests.log`), and stock NXDK compile/link/XBE/ISO succeeds (`artifacts/cavity-seam-xbox.log`). No new native runtime verification was performed for this change.


## Live floor-seam fixture

`RF_REPLAY_CAVITY_SEAM_TEST` / `D:\cavity-seam.flag` opts into an additional radius2 hardness65 patch at(-29,-2,.5), only under source66. The existing wall fixture, spawn, original level assets and default hardness stay unchanged. `tools/xemu_render_check.py --cavity-seam-test` supplies the same fixture on PC/Xbox, records the selection and includes the flag in disc restoration. The additional region enters the existing settings digest; saves cannot silently load against different fixture settings.

`tools/check_cavity_seam.py` uses the measured settled walkway/eye pose and analytically aims an ordinary rocket at the floor seam. PC publishes213 faces/989 vertices and writes a3790-byte checkpoint; no edit rejection occurs and the player survives. The native PC framebuffer was inspected and shows the crater below the walkway with retained surrounding floor, walls, crates and launcher/HUD. Repeating identical input without the fixture produces no destruction or replacement faces, preserving retail hardness100. The core tests above separately prove both compiled faces lose area and unaffected faces retain their area.

Native live evidence: `artifacts/xemu/render-20260918-002100`,400frames,77checks passing,4024 free pages (15.71875MiB) at endpoint. Native framebuffer inspected;3790-byte Xbox/PC checkpoint SHA256113d30453cbfbae54e81d2990a319e2a1c9273145d2b0ab27222b7f23aa612b0 agrees. Harness restored disc and closed emulator. Loading this fixture save with the fixture disabled fails with RF_FORMAT and writes no output checkpoint, confirming settings separation. This completes the live seam-shot gate; seam-save continuation, larger/nonconvex unions and actual neighboring-brush CSG remain open.
