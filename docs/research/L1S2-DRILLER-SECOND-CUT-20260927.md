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

This is geometric and focused C collision evidence, not a runtime
demonstration of the second cut or its visual quality. The Python probe uses
SciPy for half-space solving; the shipping C/Xbox build does not depend on
SciPy.
