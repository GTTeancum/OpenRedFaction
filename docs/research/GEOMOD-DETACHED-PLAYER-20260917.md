# Player movement against detached chunks

The new registry body query composes the recovered body-sphere transform/order
with current-pose chunk polygons. It retains batch/piece/face/sphere identity,
resolved material/texture and body velocity. Static-world face tokens and
object-registry handles are deliberately UINT32_MAX: a chunk face is not a
world face and a batch index is not an entity handle. No allocation or invented
bounding sphere is added. Empty sphere lists stay empty.

Player movement sweeps and support/ground queries now arbitrate this result
against their existing world/mover result. The nearer contact wins; world wins
ties. NPC queries and fragment simulation do not go through this player-only
adapter, preventing fragment self-collision. A non-owning scene pointer is
bound for the existing single scene lifetime, guarded by the queried world
identity, and cleared before scene destruction. Restore resolves the registry
through the live scene, so it does not retain the replaced registry pointer.

This first port treats the chunk as an unregistered dynamic surface: current
pose and linear velocity participate, but no attachment handle or rotational
carry is provided. It does not push the chunk, apply crush damage, or implement
NPC locomotion against rubble. Physical material1 matches the scene's detached
body construction. Broader material owners and out-of-room support need work.

Core tests query a real extracted face with two offset spheres, select the
nearer sphere at fraction.225, preserve material/velocity/identity, and verify
identical contact after rotating the query basis and corresponding local sphere
offset. A shorter limit misses, NULL registry misses, malformed input preserves
outputs. All121 tests pass; the expanded basis/empty cases pass additionally.
NXDK builds.

`tools/check_detached_player.py` replays ordinary input: rocket separation,
walking forward into settled rubble, then retreat. PC records34 accepted chunk
contacts. Its angled face deflects the actor sideways: the650-frame route ends
at(-10.585028,-1.118479,3.699112), past the post in the side passage. The820-frame
retreat ends at(-6.603238,-1.118479,3.699112). Both captured viewpoints were
inspected and show valid room/floor/weapon rendering. A fixed stopping-point
assertion was rejected because ordinary sliding around a slanted obstacle is
expected; this is not a claim that the chunk forms an impassable wall.

Remaining acceptance: moving chunks as supports, repeated pushes, standing on
varied fragments, save-placement rejection against restored body geometry and
broader terrain/mover/room combinations. Existing full-player save safety gates
remain unchanged. No new audio or visual-parity claim.

Native evidence: stock64MiB XEMU run render-20260917-065703 executes the full
820-frame contact/slide/retreat sequence. All73 comparisons pass, including
1555 fragment body queries,34 selected contacts and exact contact hash514430318.
The native framebuffer was inspected and shows the same side-passage retreat
view as PC. The harness restored staged disc contents and closed its emulator.
