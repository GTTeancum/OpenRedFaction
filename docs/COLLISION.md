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
