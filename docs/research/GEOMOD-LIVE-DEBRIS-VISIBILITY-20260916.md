# Live debris room filtering

`scene_debris_draw` now gathers chunks in reverse prepared visible-room order.
For each occupied room it adjusts the camera side planes to the traversal
rectangle, then tests the original authored room bounds before submitting or
aging that room's chunks. Bounds come from the retained geometry file, not the
mutable collision overlay. A chunk's retained birth room remains authoritative
even when its position moves outside that room. Existing depth sorting is retained
within each room for fading fragments; original within-room linked-list order is
not claimed. Simulation, bounce and audio continue independently of draw admission.

The selector uses bounded 80-entry stack scratch, with no new heap ownership.
Empty rectangles skip submission. Per-frame telemetry hashes hidden ages before
and after the draw loop; a mismatch is an error. Telemetry resets with the debris
pool and is included in the native PC/Xbox comparison.

## Evidence

- Extended original 546f60/5184e0 execution to yaw .7 and yaw -1.2/pitch .4,
  preserving four rectangle shapes and two origins: 24 plane fixtures pass the
  existing explicit float tolerances. Together with 114 admission fixtures,
  these are 138 binary-derived cases, not bit-exact plane reconstruction.
  Captured output: `artifacts/crater-shading-re/debris_room_planes_rotated.json`.
- `debris_scene_visibility` compiles the actual runtime selector against a
  minimal field contract. It proves reverse room order, depth sorting within a
  room, retained room selection for an out-of-bounds fragment, rejection by a
  narrow rectangle, hidden lifetime retention and resumption on return using the
  shared age helper. These are synthetic transitions, not an authored walk.
- All 114 default CTests pass (`artifacts/debris-visibility-ctest.log`).
- The 550-frame ctf06 two-blast run passes 60 stock 64 MiB PC/Xbox comparisons:
  `artifacts/xemu/render-20260916-235008`. Visibility state on both platforms is
  `[0,3877,17,0,2166136261,2166136261,282,0]`. This proves the visible-room path;
  no fragment was hidden in that run. The 3884-byte checkpoint is unchanged from
  the previous run and matches PC exactly. 4159 physical pages remain free
  (16.246 MiB). The harness restored the staged disc and exited successfully.
- Inspected the native endpoint: damaged post, scattered debris, room geometry,
  rocket launcher and HUD remain present. No all-frame or audible-output claim.
- The candidate `artifacts/debris-visibility-live/leave.bin` hits an obstruction
  before leaving the room; all chunks expire normally. It does not count as a
  hidden/return acceptance run.

Remaining: a real room transition while debris survives, native hidden/return
verification, boundary-sensitive plane decisions, original special-room bypass
and alternative view modes. The current reconstructed renderer uses one ordinary
perspective view; multi-view draw-time aging has not been established.
