# Collision reconstruction

`src/core/collision.c` starts the shared collision subsystem with the complete
segment/axis-aligned-box routine at RF.exe `0x508b70` and its outcode helper
`0x508dc0`. Evidence uses the installed binary fingerprint recorded by the
verifier. This primitive does not implement world collision or actor physics.

The routine checks the start, then the end, for inclusion before testing the
start's outside planes in X/Y/Z order. Bounds are inclusive. An endpoint inside
the box wins before any boundary intersection; callers must not interpret the
output as a nearest intersection. Shared outside bits reject immediately.
Otherwise failed plane attempts overwrite the output point. The C API retains
those writes while separately returning a hit flag. Invalid nonfinite input or
inverted bounds is a port error that preserves outputs. Inputs and outputs must
not overlap. The x86 implementations preserve extended intermediates, round at
the original float stores and restore the caller's x87 control word.

`python tools/verify_collision_box.py` runs 15,678 complete original-code cases:
5,711 hits and 9,967 misses, including 1,749 misses that write the point. It also
checks 322 invalid-input guards. Every hit value and output float byte matches
the PC implementation. All 16,000 fixtures additionally execute the actual
NXDK-linked function from `build/xbox/main.exe` in Unicorn, with unchanged
callees and the symbol address resolved from its map. The report fingerprints
that linked image. This checks compiled CPU behavior, not XEMU world behavior.
Fixtures include boundary endpoints, zero-length segments, zero-width boxes and
coordinate scales from 2^-15 through 2^15; they do not prove every float input.
Report: `artifacts/collision-box-verification.json`.

## Call graph and remaining work

- `0x4dec10` filters candidate faces, dispatches thin rays through bounding-box
  test `0x508b70` then plane test `0x506550`, and checks polygon containment with
  `0x4e1f50`. Nonzero-radius sweeps use `0x5071b0` and edge work instead; that
  swept path remains open. The initial candidate filter and projected polygon
  containment are verified below; later texture rejection remains open.
- Crouch eligibility `0x429ae0` resolves target position via `0x48a8d0`, builds
  a ray origin from the entity position and transformed class offset, then calls
  `0x498e80` with mask `0x27`. The target helper uses a valid type-zero entity's
  stored eye at +0x7d4; other objects use position +0x3c.
- `0x498e80` iterates the moving-geometry list rooted at `0x64e96c` until sentinel
  `0x64e6e0`, testing each box with `0x508b70`, and invokes `0x4df1c0` in local
  coordinates. It also queries static world geometry. Its flag conversion is
  `0x499190`; transforms, traversal and hit records still require reconstruction.
- Stand-up `0x428a60` tests from entity position toward a point whose Y is
  position Y plus info +0xf74 plus constant `0x5893c4`, using `0x499ed0` and
  the entity's physics object +0x88. A collision prevents standing. Successful
  standing clears bit 0x400, updates an associated record, refreshes collider
  vectors, then calls `0x4a0840`.
- Enter-crouch `0x4289d0` sets bit 0x400, refreshes collider vectors, calls
  `0x4a0840`, then stamps entity +0x7b4 from global `0x6460f0`. The refresh uses
  40-byte class entries selected by `0x42da40`, copying their +0x18 vector.
- `0x4a0840` performs a ground/support query through `0x499ed0`, then may change
  vertical position, support-object state, bounds and related actor state. It
  cannot be replaced with only a collider-height change.

These call-graph observations come from Ghidra and original instructions;
the segment/box, segment/plane and projected containment routines are verified.
The export script now retains the named collision functions for further work.

## One-sided plane crossing

`rf_collision_segment_plane` reconstructs complete `0x506550` including the
numeric behavior of distance helper `0x4163a0` and dot product `0x40a0b0`.
Input is a start position, displacement (not an endpoint), and four plane
coefficients. It rejects negative signed start distance before rounding that
comparison, then compares the negated extended normal/displacement dot against
the stored float distance. Accepted fraction is stored float distance divided
by that extended dot. Dot products accumulate Z then Y then X. Misses preserve
the caller's fraction. The x86 paths restore the caller's x87 control word.

Coplanar parallel inputs reproduce the original hit byte one and negative NaN
fraction from zero/zero; this must not be consumed as a valid point by future
world-query code without checking the higher-level original behavior. Nonfinite
inputs are rejected by the port with unchanged outputs. Zero normals remain
accepted inputs to preserve the original arithmetic behavior. Non-x86 fallback
uses long double and has not been proved equivalent; the supported PC/Xbox x86
paths are the ones verified.

`python tools/verify_collision_plane.py` matches all output bits for 11,876
complete original executions: 2,901 hit results including 1,235 NaN fractions.
Another 124 invalid-input guards pass. All 12,000 fixtures also execute the
actual NXDK-linked routine in Unicorn. The verifier fingerprints the linked
image and checks terminal return addresses. Report:
`artifacts/collision-plane-verification.json`. Both builds, all four CTest
checks, and the 16,000-case box regression pass. This proves the primitive,
not polygon containment, world traversal or gameplay collision.

## Projected polygon containment

`rf_collision_polygon_contains` reconstructs `0x4e1f50` with axis selection
`0x4fa6d0`. A bounded, ordered vertex array replaces the original circular edge
list; the last vertex seeds the previous edge endpoint. No allocation is needed.
Dominant-axis selection uses strict comparisons: X wins only if larger than Y
and Z, otherwise Y wins only if larger than Z, otherwise Z wins. The sign of
that normal component selects the ordering of the remaining axes from the
original table at `0x5a3ee0`. A zero normal follows the original Z/nonpositive
path; containment is projected and does not establish coplanarity.

The crossing test uses half-open vertical intervals and a strict horizontal
comparison against the extended intersection. It applies no epsilon or general
on-edge override. The x86 implementations restore the x87 control word after
each comparison. Port guards reject nonfinite coordinates and counts outside
1..65536 without changing the output; even one/two-vertex loops follow the
original computation. Input arrays must contain the declared count.

`python tools/verify_collision_polygon.py` executes 6,858 complete original
calls, including the original axis helper and circular edge-list traversal,
and matches 821 inside results. Another 142 guard cases preserve output.
All 7,000 fixtures also execute the NXDK-linked function in Unicorn. Fixtures
cover concave star loops, arbitrary vertex orders, reversed winding, normal
axis/sign ties, zero normals, degenerate loops and exact vertex/edge points.
Report: `artifacts/collision-polygon-verification.json`, including the linked
Xbox image fingerprint. This is primitive CPU validation, not an XEMU gameplay
test; world traversal, later texture rejection and swept collision remain open.

## Candidate face filter

`rf_collision_face_accept` reconstructs the prefix `0x4dec10..0x4deced`, using
compact views of query +0x50, face flags +0x28, signed 16-bit face +0x34 and the
optional face owner +0x44. All seven predicate callees are accounted for:

| Query condition | Face/owner condition that rejects |
| --- | --- |
| bit 0x20 set | face bit 0x40 set (`0x4d4db0`) |
| bit 0x40 set | face bit 0x80 set (`0x4d7d60`) |
| bit 0x400 set | owner exists, byte +0 equals 1 and byte +0x98 equals 0 (`0x4e36a0`, `0x494a50`) |
| bit 0x2 clear | signed face +0x34 greater than zero (`0x49cc80`) |
| bit 0x1000 clear | face bit 0x4 set (`0x416260`) |
| bits 0x8 and 0x2000 clear | face bit 0x2000 set (`0x4ce450`) |
| bits 0x8 and 0x800 clear | face bit 0x1 set |

The original order is retained. Accept means proceed to geometry, not that a
collision occurred. Names for unresolved flags and the signed field have not
been invented. The caller resolves the optional owner into this view. Port
guards reject out-of-range byte, signed-word or presence fields and preserve
output. No allocation, geometry calls or entity mutation occurs here.

`python tools/verify_collision_filter.py` compares 28,356 original executions
against PC and the actual NXDK-linked function: 7,551 accepted. Its original
execution stops at `0x4deced` before geometric work and runs all predicate
callees unchanged. Fixtures cover every combination of eight relevant query
bits and five relevant face bits at signed-field values -1/0/1, with varied
owner fields, plus random full words and signed limits. Another 316 guards
pass, totaling 28,672 PC/NXDK fixtures. Report:
`artifacts/collision-filter-verification.json`. This does not cover the later
texture-based rejection or nearest-hit/result-record updates in `0x4dec10`.

## Combined thin-face query

`rf_collision_thin_face` now joins the filter, bounding-box, one-sided plane,
nearest-fraction and projected containment tests into a caller-owned face view.
It constructs the endpoint with float addition, and constructs the intersection
with separate float multiply/add stores, matching the original vector helpers.
Equal fractions pass the nearest-fraction gate. Accepted results publish fraction,
point and plane normal; misses and errors preserve the prior result. There is
no allocation, world traversal or original global counter mutation.

The crouch-visibility external mask `0x27` converts through original `0x499190`
to internal `0x461`; this supported mode does not invoke later texture sampling.
Other texture-check flags `0x80/0x100` return RF_NOT_FOUND after filtering until
that branch is recovered. Swept radii are outside this API. Nonfinite or
out-of-range fraction limits and a coplanar NaN fraction return RF_FORMAT;
these are explicit port guards, not claims of original degenerate-ray behavior.

`python tools/verify_collision_thin.py` compares 6,000 complete original
`0x4dec10` calls with zero radius and flags `0x461`: 126 accepted hits, matching
every fraction/point/normal byte. All original geometric and filter callees run
unchanged; the fixture starts with original static scratch initialization marked
complete to avoid process-exit registration. The original output count starts
at zero and the fraction limit varies among 0.25, 0.5 and 1. Faces use rectangles
on each signed axis with varied filters and rays. This does not prove arbitrary
polygon integration beyond the separately tested containment primitive.
Three port guards check unsupported flags, a NaN limit and coplanar input with
unchanged output. All 6,003 cases also run the NXDK-linked routine in Unicorn.
Report: `artifacts/collision-thin-verification.json`. PC/NXDK builds and four
CTest checks pass. Real level-face binding and traversal are the next integration
steps; this function is not yet used by the running diagnostic scene.

## Loaded level-face binding

`rf_geometry_collision_face` connects an opened level geometry object to a
collision face view. It copies the file plane, resolves corners to vertices in
file order and derives axis-aligned extrema with the original expansion. The caller owns and sizes the
vertex scratch buffer; the function allocates nothing. The returned view borrows
that buffer and remains valid until it is reused. Capacity is measured in
vertices. Errors preserve the output view, while scratch may be partially
written. Successfully opened geometry must remain unmodified during the call.

Runtime filter metadata is an explicit argument. The adapter does not assume
that the file's one-byte flags equal the runtime face flags, synthesize owner
records or guess the signed face field. This boundary remains necessary until
the runtime face construction/loading path is recovered.

`python tools/verify_collision_level.py` binds every one of Live Mines
`L1S1.rfl`'s 7,418 faces. The largest contains 16 corners. Each face is tested
with a ray starting at the vertex average plus its normal and displaced by
minus twice that normal. All 7,418 results hit and match complete original
`0x4dec10` calls exactly for fraction, point and normal; the same bound inputs
also match the actual NXDK-linked thin-face function in Unicorn. Every face
additionally checks an undersized capacity and unchanged output view. The
probe uses a fixed 3,072-byte scratch buffer and the existing 8 MiB geometry
budget. It loads geometry on PC; the Xbox comparison validates the compiled
query on its bound faces, not loading the level inside XEMU.

Report: `artifacts/collision-level-verification.json`. All calls deliberately
use internal query flags `0x461` and clear diagnostic face/owner fields. This
is evidence for geometric binding and intersection, not equivalence of runtime
filter metadata, world traversal, nearest world hit or gameplay collision.
PC/NXDK builds and the four registered CTest checks pass. No rendering changed.

## Corrected file flag width

The prior geometry accessor exposed only byte +40 as face flags. Original
solid loader `0x4ed520`, identified using the pinned Dash Faction solid_read
patch addresses, establishes that version-180 flags are a full 32-bit word at
file face +40. At `0x4edeb9..0x4edec8`, unchanged reader `0x515420` consumes
four bytes and stores the word into the temporary face attributes. The face
creation chain `0x4cfab0 -> 0x4dfbd0` copies six attribute words into runtime
face +0x28, so this flags word initially survives intact. This observation is
not proof that later setup leaves every flag unchanged.

`rf_geometry_get_face` now returns the complete word. The older pinned Open
Faction layout lead described separate bytes here; original executable evidence
supersedes that interpretation. No third-party implementation was copied.
The apparent byte at +41 must not be treated as an independent lightmap
resolution field based on that older layout description.

`python tools/verify_geometry_flags.py` checks every accessor against the raw
word across 460,720 faces in 94 inventoried levels; 271,394 have flags above
0xff that were previously truncated. It executes the original loader read
block and unchanged binary reader on every observed flag word plus random
32-bit words: 1,047 cases, each preserving the full value and advancing four
bytes. Report: `artifacts/geometry-flags-verification.json`. All 94 bounded
geometry loads, accessor/budget checks, PC/NXDK builds and four CTest checks
pass. The existing preview tests only flag bit 0, so this correction does not
introduce a new visible result.

The loader also narrows the file portal field to runtime face +0x34 and assigns
room ownership through `0x4ccec0`; these are trace leads requiring further
verification before creating authoritative runtime filter views. Post-load
texture/room-derived flag changes remain open.

## Original bound finalization

The face finalizer `0x4dfe20` revealed a missing step in the level adapter:
after accumulating vertex extrema, `0x4e002b..0x4e0045` subtracts binary32
`0x38d1b717` (0.0001) from each minimum and adds it to each maximum through
`0x465ee0` and `0x465ec0`. Exact vertex extrema alone were narrower than the
original broad-phase bounds. `rf_geometry_collision_face` now applies this
expansion, preserving each final float store.

`verify_collision_level.py` now additionally executes the original complete
extrema loop and expansion at `0x4dff92..0x4e0045`, using actual circular face
edge lists. All six bound floats match bit-for-bit for all 7,418 Live Mines
faces. Their original and NXDK thin-ray comparisons still pass, as do both
builds and all four CTest checks. This does not prove the remaining plane
recalculation and degeneracy handling elsewhere in the finalizer.

Further setup traces: `0x4ce160` derives room byte +2 by scanning its linked
faces; it becomes one only when every face has flag 0x2000 (also one for an
empty list). `0x4ccf50` rebuilds the room's bounding structure through
`0x4f9340`. These are Ghidra/instruction observations, not yet reconstructed
runtime room behavior. They do not establish texture-derived flag semantics.
