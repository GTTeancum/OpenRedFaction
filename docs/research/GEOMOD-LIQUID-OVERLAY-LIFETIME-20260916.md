# GeoMod liquid overlay lifetime review

## Finding

No definite stale liquid-face pointer or incorrect face-index ownership was found in the current successful cut/reset path. No geometry.c change was justified. This is a source review plus a new regression pending the primary agent's coordinated build; it is not a new live Xbox or native water-flow result.

The earlier statement that the current DEV room contains a water pool was unsupported and is withdrawn. The primary's glass_house.rfl inventory has no authored water/liquid faces. The regression below creates a synthetic water fixture explicitly; it does not claim the current DEV level exercises water.

## Source ownership evidence

- geometry.c: overlay_open owns copied room/view/liquid descriptors and its source-index map. Each copied view.tree is rewired to the overlay's copied room.tree. Other room trees and common room lists remain borrowed under the documented base-outlives-overlay contract.
- overlay_bind validates before mutation, remaps caller source IDs through the replacement tree's source_indices, assigns the copied room.tree, and updates BOTH liquids[room].faces and face_count. The existing contains_liquid marker remains copied. No old liquid face-array pointer is retained after successful bind.
- The liquid sweep returns a face index in the same tree-ordered face array; geometry_world_sweep_liquid resolves that through the copied room.tree.source_indices. Generated surfaces use the explicit scene fallback source ID; preserved source faces retain source IDs. The liquid and solid paths therefore share one authoritative published array/map.
- geomod.c terrain_bind starts each new face with generated_filter, but restores original_filters by source_face for preserved faces. Existing liquid face bits are not silently replaced with generated flags when an original face survives a cut. Geometry is generated into inactive position/face banks and a fresh collision tree; terrain_publish frees the previous tree only after successful mesh commit.
- scene.c scene_terrain_bind obtains the committed tree and replaces overlay bindings. Every successful current cut_box, rocket cut_template, DEV reset and checkpoint import calls this before resuming frame collision queries. DEV reset clears lightmap caches separately; it does not substitute those caches for collision ownership.
- Stream destruction closes the shallow overlay before terrain storage; the platform caller retains the base world throughout the scene call. Overlay close does not free borrowed trees.

## Failure boundary

A successful terrain_publish frees the old collision tree before scene_terrain_bind performs draw subdivision and overlay rebind. Thus there is a short interval where the overlay is invalid. Current paths return a scene error and unwind on a subsequent binding/subdivision failure, rather than resuming queries. Do not introduce a recover-and-continue frame path after these failures without making terrain and overlay publication atomic or immediately restoring a valid tree. This is a concrete future gameplay risk, not a demonstrated current use-after-free.

Similarly, overlay_bind's rejected-input preservation contract only protects a still-live previous replacement. Callers cannot free that previous tree first and expect a rejected bind to resurrect it. The new regression keeps the previous tree alive until successful bind.

## New focused regression

`tests/geometry_liquid_overlay_tests.c` constructs two independent rooms, each with a solid floor and a liquid surface. It exercises eight changing replacement trees and four restores to original geometry; intervening replacements occur while a prior cut tree is still active. It frees the previous tree after successful bind and then queries the replacement.

Assertions cover actual liquid sweep fractions and resolved source IDs, tree-order remapping, liquid descriptor face ownership/count, copied view.tree ownership, unchanged borrowed room, unchanged base geometry, and continued queries after rejected invalid-source-ID/count/null-face replacements. Closing the overlay twice must leave the base world queryable. Existing geomod_interior_tests cover eight cuts/reset and solid queries; collision_liquid_room_tests cover liquid ordering separately. This new test covers their previously untested binding/lifetime intersection.

Build registration and execution are owned by the primary agent; no build was run by this reviewer. A meaningful next live validation is a water-bearing fixture with a solid boundary behind it, then cut/reset/re-cut and water-to-solid projectile continuation. The current dry DEV level cannot establish that behavior. Optional per-face liquid texture metadata is currently absent from geometry_world_open; future textured-water overlays will need explicit texture-table remapping along with face replacement.
