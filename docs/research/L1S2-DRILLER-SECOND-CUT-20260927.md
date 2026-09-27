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
area only on room-8 faces 5780 and 5784. This particular cut need not mutate
the brush's room-13 or room-123 polygons, although a later cutter may. The
current source-8123-only publication would leave the two room-8 polygons
intact, so the existing rejection is correct. The next implementation must
admit UID8219 as another editable owner, combine its room-8 output with
UID8123 in one collision/render transaction, and preserve its other rooms.

This is geometric evidence of ownership, not a runtime demonstration of the
second cut or its visual quality. The probe uses SciPy for half-space solving;
the shipping C/Xbox build does not depend on SciPy.
