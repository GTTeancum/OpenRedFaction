# Debris room visibility adapter

The live `scene_debris_draw` currently collects every active chunk, sorts by
camera depth, and advances age before preview clipping. The retained chunk room
is not consulted. This differs from original 4d4708/4d4c60/48fe00, which submits
only matching debris for admitted visible rooms; 48fd70 advances age on submission.
Thus clipping triangles later cannot reproduce the original lifetime behavior.

The existing helper research is now built into the shared C library and registered
as `debris_visibility` in CTest. Its 114 captured room membership/AABB cases pass,
including all eight normal sign combinations and tangency. Invalid input leaves
the output unchanged. Both PC CMake and NXDK source lists include the module.

The new `rf_debris_room_rectangle` adapter replaces four perspective side planes
using the current camera basis, origin, scales and room traversal rectangle.
It preserves near/far slots, masks, plane count and scaled distances. Flat projection,
zero/negative rectangles and invalid inputs are rejected without publication.
There are no allocations. This is a zero-origin viewport contract, not coverage of
the original special-room bypass or alternate camera-origin modes.

Eight embedded fixtures come from execution of original 546f60 and 5184e0 in
`artifacts/crater-shading-re/debris_room_planes.json`, using RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
They cover full, central, left and right rectangles at origins (0,0,0) and
(10,20,30), identity basis, 640x480 and scales (1,4/3,1).
Normals agree within 2e-6 and distances within 4e-5; this is explicitly a
tolerance comparison, not bit-identical plane reconstruction. The 114 admission
fixtures compare exact decisions. Rotated-camera fixtures and boundary-sensitive
adapter decisions still need validation before live integration.

Next: validate rotated rectangle planes, retain original authored room bounds
(not expanded collision overlay bounds), then feed current room rectangles and
reverse visible-room order into debris submission. Verify offscreen retention,
return-to-room aging and native rendering before claiming the live mismatch fixed.
The runtime draw and age policy is unchanged in this commit.
