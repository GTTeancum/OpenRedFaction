# L1S2 Driller second-cut ownership

`python -B tools/probe_driller_second_cut.py` is a read-only, text-only
reproduction of the live frame-296 contact. The 330-frame process-local replay
records cutter center `(124.314514,-2.1283164,-17)` and body basis
`(-0.287775129,0.000001598,-0.957698166; -0.004944316,0.999986291,0.001487379;
0.957684517,0.005163210,-0.287771463)`. Region admission aligns the
template center to `(124.314514,-2.12633848,-17)` and supplies one downward
normal `(0,-1,0)` with depth `0.4` and scale `1`. These inputs reproduce the
runtime cutter bounds to a few micrometers. The single-bit template is packed
from the original `RF.exe` factory at `0x4e6d60` by
`tools/pack_driller_templates.py`; original assets remain untracked.

The installed L1S2 editor section has 389 brushes. UID8219 is an inward
convex 14-face cavity; its 32 compiled faces belong to rooms 8, 13 and 123.
The probe treats each of the template's 26 triangles plus its interior kernel
as a tetrahedron, applies the same shallow-point deformation as the shared C
core, and intersects each tetrahedron with UID8219's convex half-spaces. Six
tetrahedra have positive-volume intersections. The largest normalized
half-space margin is `0.118774864`, with witness
`(126.743775,-1.903039,-17.181067)`.

Clipping UID8219's compiled polygons against those tetrahedra finds positive
area only on room-8 faces 5780 and 5784. This particular cut does not touch
that brush's room-13 or room-123 polygons, although a later cutter may.
Extending the same read-only sweep to other overlapping brushes reveals four
positive-area compiled contacts on detail brush UID9996 in room 121: faces
4972, 4984, 4985 and 4998. Their compiled flags are 8. Other nearby brush
bounds overlap, but their compiled polygons do not intersect this cutter.
The original source-8123-only publication would leave both UID8219's room-8
geometry and UID9996's room-121 detail intact, so the live rejection remains
correct.

The C core now decodes and hashes UID8219 independently and can compose both
cavity outputs into one bounded room-8 candidate. A focused collision probe
replaces the 83 UID8123 and eight UID8219 room-8 face IDs together: a ray that
previously hit UID8219 face 5780 at x=126.625 no longer hits that face, and an
opposite ray meets the recessed crater at x=126.863. This proves the room-8
geometry path, not a safe live second cut. Room 121 contains all 30 compiled
faces of UID9996, each with serialized face flag 8; its room owner has kind 1
and active state 1. The shared core can now import that compiled room while
preserving face IDs, source tokens, UVs and texture indices. Applying the
second star through the existing polygon cutter produces 74 retained fragments
with 297 vertices. A collision-composition probe replaces the 30 original
faces and confirms a ray that hit face 4972 now misses. Runtime publication
must bind this room together with the room-8 candidate, then integrate paired
admission, checkpoint history and renderer staging without regressing the
first cut's save/load behavior.

The exact UID8123/UID8219 pair now opens through the scene source collection,
and a one-unit first cut commits through the real grouped scene transaction:
177 room-8 publication faces, 731 vertices and 91 replaced compiled faces.
The existing single-source checkpoint stage still passes. A second focused
scene check applies the recorded frame-296 center, basis, unit template scale
and 0.4 downward shallow limit after that paired first cut. Both source
bounds are selected, but UID8123's cavity obstacle admission rejects the
second cutter with `RF_NOT_FOUND`; the transaction retains serial 1 and no
pending publication. This is an intentional guard while the intersected
UID9996 detail in room 121 has no live replacement. The pair is diagnostic
only; the normal L1S2 scene remains the single source and keeps its working
first-cut save/load path. The pair's 16 MiB subsystem budget has passed this
PC transaction but has not been validated against whole-game stock Xbox RAM.

The paired scene publication owner now also opens room 121 as a bounded detail
source: 30 compiled faces and 120 vertices, all flags-8, with their compiled
face IDs retained. Its inactive candidate bank clips the recorded second star
to 74 faces and 297 vertices, prepares a composed collision tree, and changes
the face-4972 ray as predicted. Aborting the candidate restores the original
face-4972 hit and leaves room 8 at serial 1. The focused PC stage peak is
1,741,302 bytes within the 4 MiB publication budget; the shared scene code
also compiles for Xbox. This candidate is not yet bound to the live collision
world or renderer, and the exact second edit remains guarded.

The paired first-cut scene now writes RFDS3 without detached-piece trailers,
stages both cavity histories, commits the private collection restore and
re-encodes byte-identically in the focused PC harness. The original
single-source RFDS2 stage and the exact second-contact rollback checks pass.
Generated chart names for L1S2 use lowercase `l1s2.rfl`, as required by the
shared digest format. This qualifies the pair's scene-level first-cut history;
ordinary campaign RFDS3 dispatch and the room-121 second-cut publication remain
separate work. The Xbox build compiles, but this new collection route has not
run under XEMU or against the stock whole-game memory limit.

Ordinary world snapshot dispatch now recognizes the exact L1S2
UID8123/UID8219 collection, writes RFDS3 and selects the collection restore
stage when that profile is loaded. The default campaign still opens only
UID8123, so its existing RFDS2 saves continue on the previous route. Focused
PC stage checks and Xbox compilation pass; a paired whole-world save/load has
not yet run, and room 121 still lacks a scene publication owner.

This is geometric and focused C collision evidence, not a runtime
demonstration of the second cut or its visual quality. The Python probe uses
SciPy for half-space solving; the shipping C/Xbox build does not depend on
SciPy.

The paired scene now owns a room-121 overlay layered over room 8. The focused
PC harness prepares the second room-121 candidate, verifies rollback to
compiled face 4972, then prepares it again and publishes the 74-face tree
into the second overlay. The room-8 view is synchronized after its first-cut
bind, and a world query no longer reports face 4972 at the recorded ray. The
stage peak including overlay storage is 1,761,806 bytes, below the 4 MiB
publication cap. The three focused L1S2 checkpoint/paired checks pass, and
the NXDK build completes. The live edit still fails its existing admission
guard, deliberately: rendering, coordinated commit and second-cut save state
are not ready for player-visible use. No Xbox runtime check was made here.

Room-121 fragment preparation now remaps the original texture indices to the
shared renderer slots and replaces each editor source-face word with its
compiled face ID for inherited lightmap lookup. A source-identity check admits
only retained UID9996 fragments whose IDs and materials match the 30 imported
compiled faces. The focused PC projection emits 120 visible vertices from the
candidate and all 120 carry authored lightmap bindings. Its enlarged owner
raises the measured stage peak to 1,810,958 bytes, still below 4 MiB. The
focused L1S2 checks and NXDK build pass; this is draw data only, not a rendered
or visually reviewed gameplay frame. The normal second edit remains guarded.

The paired scene transaction now recognizes only the measured frame-296
second cutter bounds (0.01-unit tolerance), stages the UID9996 room-121
fragment composition, and lets the UID8123/UID8219 room-8 group reach lighting
preparation. `scene_terrain_publication_finish` rejects while a room-121
candidate is pending, before binding room 8, so the group discards both cloned
cores and returns to serial 1. The focused PC rollback check and NXDK build
pass. This narrow bound is a staging guard, not proof that arbitrary cutters
with the same box avoid other compiled surfaces; live admission still needs a
general geometric scope check and a coordinated two-room commit.
