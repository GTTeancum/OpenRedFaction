# Movable prop pose publication

`rf_scene_clutter_pose_set(handle, position, basis)` validates the registered owner, resolves its room against the scene world and atomically publishes the solved pose. The shared helper updates rendered position/basis, collision current/pending transforms and bounds, world inertia, query position and room. Identity, health, velocity and gameplay/scheduling flags stay owned by their existing systems.

Focused PC checks pass translation, a90-degree anisotropic-inertia rotation, consistent bounds/room, owner-alias inputs and unchanged output on invalid pose. This supplies a callable scene service for future carried or moved props; it does not implement attachment ownership, hauling animation, standalone physics scheduling or moving-prop saves. No live moved-prop visual check is claimed.

Build verification: shared PC executable and NXDK Xbox image build successfully with these changes. New component tests passed on PC; native save/load and moved-prop runtime behavior remain unverified.
