# Retained Xbox static-world rendering

Status: focused stock64MiB XEMU runtime checks have resumed. The first180-frame check renders and matches selected PC gameplay state; full visual/parity checks remain open. See [sustained performance work](XBOX-PACING-POSE-SHARING.md).

## Structural change

The previous Xbox presentation path asked the shared CPU renderer to expand visible static-world faces into triangles, transform vertices, clip polygons against six planes, project to screen/depth, calculate reciprocal texture coordinates, copy the expanded stream, hash it and upload it every frame. The existing step-profile run measured13.029ms in world geometry rebuilding; this is a historical cost for a particular scene, not a promised saving.

The new optional backend caches the static level's unprojected triangles in GPU-accessible write-combined memory once. Small cached face records hold draw ranges, material/lightmap indices, visibility-room data and facing planes. Every frame retains the current portal visibility and face rejection, submits visible contiguous ranges, and updates camera constants. NV2A's vertex program performs camera transformation/projection; vertex W preserves perspective for texture interpolation. This follows the screen-space output convention in the locally installed NXDK samples/mesh shader. Existing texture/lightmap sampling remains in the fragment program.

The scene omits static geometry from the CPU preview stream only when the backend accepts that frame. Movers and the first-person weapon continue through their existing dynamic paths and draw after the retained world. Animated NPCs, rigid clutter, held world weapons and pickups use the optional [retained model backend](XBOX-RETAINED-MODELS.md) when its cache accepts a batch. The projected-vertex shader and its constant are restored before those draws. Shared PC rendering stays on the existing CPU implementation; game logic is unchanged by backend selection.

## Memory and ownership

A4MiB hard cap covers cached vertices plus face descriptors. Oversized levels or allocation failure fall back to the CPU world path for the remainder of that level. There is one vertex allocation and one descriptor allocation, with no per-frame allocation or world vertex upload. GPU writes are sequential; the CPU never reads the cached write-combined vertices. Level stream shutdown waits for outstanding GPU work before releasing the cache. The level geometry and visibility objects are borrowed for frame preparation; descriptors/vertices are copied. Future GeoMod/topology mutation must explicitly invalidate/rebuild this cache before drawing changed static geometry.

`rf_xbox_retained_world[8]` publishes ready state, GPU bytes, descriptor bytes, cached vertex count, visible vertex count, visible face count, draw count and allocation/budget fallback count. This does not increase the stock64MiB target. The prior successful native waypoint run had6606 available pages at completion; that is contextual headroom, not a measurement of this new allocation.

## Outstanding checks and next work

Extend the initial short render check with representative hall performance and visual checks. Inspect near-plane crossings, geometry behind the camera, depth agreement with dynamic geometry, texture perspective, portal transitions, material fallback and level handoff/cache release. The prior CPU path rounded screen coordinates to1/16 pixel; this GPU path relies on hardware rasterization, so byte-identical framebuffers are not an acceptance criterion. Record actual frame costs and memory before claiming a speedup. The old native harness compares the complete CPU-projected world stream against PC; those mesh counts/hashes intentionally differ now. Update that gate to preserve gameplay-state checks while inspecting retained-world metrics and images separately.

The model backend adds retained GPU NPC skinning and rigid-model projection; see [its implementation and validation limits](XBOX-RETAINED-MODELS.md). First-person weapon projection, CPU pose evaluation and high draw-call counts remain. These changes do not solve all rendering or simulation costs.


Build evidence: shared PC executable and NXDK XBE/XISO compile successfully. Cg maps view rows to c0..c2, shading to c3, projection to c4 and its zero literal to c5; the renderer uploads all six explicitly. The initial build preceded resumed testing; subsequent runtime evidence is recorded in [the performance notes](XBOX-PACING-POSE-SHARING.md).
