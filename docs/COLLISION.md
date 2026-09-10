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

## Initial file/room collision filters

`rf_geometry_initial_collision_filter` now produces initial filter views from
loaded geometry: full file flags, the low signed 16 bits of the portal field,
and the face's owning room. It uses room file byte +34 for the initial owner
kind and room life at +36 to initialize owner state to zero when life is
positive, otherwise one. Output is unchanged on failure, and no allocation is
performed. Geometry must be a successfully opened, unmodified object.

Source evidence: the loader narrows portal to a word at `0x4ede8b`; attribute
creation copies it to runtime face +0x34. Room constructor `0x4ccd06` initializes
room +0x98 to one; the life read/store/comparison at `0x4eda87..0x4edaa6` clears
it for positive life. The preceding byte reads place detail in `0x4ce110`,
which copies the byte to room +0 and updates the solid's room lists. This API
reconstructs the initial scalar values only, not those list mutations or later
damage, destruction, liquid or texture-derived state changes.

`python tools/verify_collision_level.py --initial` runs every Live Mines face
using these initial filter views. Of 7,418 centered face rays, 7,087 hit and
331 are rejected, matching complete original `0x4dec10` calls and the actual
NXDK-linked query for every output byte. Original bound-finalizer comparisons
and capacity guards also pass. The test maps the produced initial views into
original runtime structs; it is not an execution of the complete original
level loader. Report: `artifacts/collision-level-initial-verification.json`.
The clear-metadata diagnostic remains separately available and still passes;
both builds and all four CTest checks pass. World traversal and later mutable
room/face state remain open.

## Bounded thin-ray tree traversal

`rf_collision_thin_tree` reconstructs the zero-radius traversal at `0x4deab0`
over caller-owned node and face arrays. Each node's ordered face range is tested
before its descendants. The original pushes left (+0x20), then right (+0x24),
so the LIFO traversal visits right first. Query bit 0 exits on the first accepted
hit. Otherwise accepted hits update the fraction limit; equal fractions replace
the prior result, and the output records the number of accepted updates and
the final face index. The original node-list pointers are represented as indices.

The caller supplies one stack entry per node; no allocation or fixed 4 KiB
automatic stack is introduced. Root is node zero and UINT32_MAX denotes absent
children. Bounds and index ranges are checked before traversal. A visit-count
limit catches traversed cycles; this API requires a tree, not a shared-node DAG.
Errors preserve result and matched outputs, although scratch can change. Empty
node input is a miss. This does not construct the hierarchy or select world
rooms and retains the thin-face API's unsupported texture/sweep modes.

`python tools/verify_collision_tree.py` compares 1,800 complete original calls,
including its stack probe, box tests, face-list iteration and full face queries.
Three-node fixtures vary child ordering, empty face lists, bounds rejection,
equal depths and first/nearest modes: 907 hits, with 144 cases accepting multiple
updates. All output fraction/point/normal, face indices and hit counts match PC
and the actual NXDK-linked function. Three port guards additionally check an
invalid child, NaN fraction limit and traversed cycle with preserved outputs.
Report: `artifacts/collision-tree-verification.json`. Both builds, the Live Mines
initial-metadata regression and four CTest checks pass. This CPU fixture is not
an XEMU gameplay traversal test; level hierarchy construction is still open.

## Tree partition decision

The builder chain is now traced: `0x4f9340` appends room faces in linked-list
order, `0x4f8fd0` computes their union bounds and `0x4f9050` partitions. The
recovered `rf_collision_partition` performs the decision before child allocation.
It selects the longest axis using strict comparisons, retaining X on ties before
Y and Z. Original Y-span rounding is preserved in the x86 comparison helper.
Faces wholly in the upper half receive label 1; that test has precedence over
the lower half (label 2). Faces crossing the split remain in the parent (0).
Bounds must be finite, ordered and contained in the node. Errors preserve all
outputs. The caller supplies the labels; there is no allocation or face mutation.

The original only creates children if both labels 1 and 2 have nonzero counts.
It moves faces in original order with append helper `0x4d30e0`, updates their
node owner pointers, recomputes child union bounds, then builds left and right
subtrees. That redistribution and allocation are not yet implemented by this
decision helper. The resulting upper child occupies the original left slot,
which traversal visits after the lower/right child.

`python tools/verify_collision_partition.py` compares 2,000 original executions
through the leaf/allocation boundary, then uses unchanged `0x4f8f90` for every
face label. Axis, labels and group counts match; 1,917 cases can split. Fixtures
use varied extents, exact split-plane ties, degenerate boxes and parent-spanning
faces. Three port guards check NaN, inverted bounds and a face outside its node.
All 2,003 fixtures also pass against the actual NXDK-linked helper in Unicorn.
Report: `artifacts/collision-partition-verification.json`. PC/NXDK builds and
four CTest checks pass. Full recursive trees and world queries remain open.

## Complete bounded tree construction

`rf_collision_tree_open` now reconstructs union bounds (`0x4f8fd0`) and the
complete partition builder (`0x4f9050`). Stable redistribution retains the
original face order within parent, upper/left and lower/right groups. It creates
both children before building the left subtree and then the right, preserving
original node allocation order. Child bounds are recomputed from their faces.
An explicit pending-node stack replaces recursive C stack growth. Each split
requires two nonempty groups, bounding node capacity at `2 * face_count - 1`.

The tree owns copied face views, source-index mappings, nodes and traversal
scratch; vertex arrays remain borrowed and must stay alive. Source indices map
reordered query results back to input faces. The budget includes the tree struct,
retained storage and temporary redistribution scratch, excluding allocator
metadata and borrowed vertices. On 32-bit targets, nonempty trees reserve
`164 * face_count - 4` retained bytes and peak at `241 * face_count - 4` bytes.
An empty tree needs 40 bytes. Invalid bounds, insufficient budgets and allocation
failure preserve the output. `rf_collision_tree_close` frees storage and clears
the struct. Existing trees must be closed before reusing their output object.

`python tools/verify_collision_builder.py` compares 600 complete original
constructions with PC and actual NXDK-linked code. Fixtures contain 1–16 faces,
degenerate bounds, overlapping boxes and varied extents; the largest resulting
tree has 19 nodes. Bounds match byte-for-byte, and ordered face identities and
child topology match. Six additional checks cover exact/insufficient budgets,
empty input and malformed bounds. Two NXDK allocation-failure checks verify
cleanup and preserved output. The original pool allocator at `0x4f97b0` alone
is substituted with bounded fixture storage; its list movement and child
initialization execute unchanged. NXDK malloc/free are also fixture hooks, so
this does not validate the guest heap in XEMU. Report:
`artifacts/collision-builder-verification.json`. PC/NXDK builds and all four
CTest checks pass. Stable loaded-face storage, per-room ownership, world room
selection and gameplay integration remain open.

## Owned initial room geometry

`rf_geometry_collision_room_open` binds all file faces whose room index matches
the requested room, in file order, using initial collision metadata. It copies
each face's ordered vertices into owned storage and builds the recovered tree.
Tree source indices are translated back to level face indices. The resulting
object does not borrow the input geometry; close it with
`rf_geometry_collision_room_close`. Budget accounting covers the object, owned
vertices, retained tree, temporary input views/index mapping and tree construction
scratch. The input geometry and allocator metadata are excluded. Failures preserve
the output; a previously open output must be closed before reuse.

This is a new integration layer around recovered primitives. It does not claim
that file order is the final original runtime room-list order. Detail-room
attachment, post-load face changes, world room selection, moving geometry and
mutable damage/Geo-Mod state remain unresolved. No world-query facade substitutes
an all-room scan for that missing behavior.

`python tools/verify_collision_rooms.py` passes 94 levels and 460,720 faces on PC.
Every room is built with a generous budget, then rebuilt at its reported exact
peak budget; one byte less must fail with preserved output. Checks verify exact
source vertices, room membership, unique source face identities, total face
coverage, and nearest-ray hit/fraction agreement with exhaustive per-room face
queries. Rays are slightly skewed from face normals to avoid the already-known
coplanar/parallel NaN primitive result on adjacent faces; this is a synthetic
integration test, not an original-world-query comparison. Live Mines has 54
rooms, 7,418 faces and 1,272 tree nodes. Its largest single-room peak is 194,095
bytes; the maximum across all levels is 826,739 bytes. Rooms are tested sequentially,
so these figures do not describe total resident world memory. Report:
`artifacts/collision-rooms-verification.json`. Both builds pass, as do the 600-case
original builder regression on PC/NXDK and four CTest checks. Guest heap and full
room integration have not yet been tested in XEMU.

## Original room attachment and selection trace

`0x4edfeb..0x4edff2` attaches a face only after successful `0x4dfe20`
finalization and a non-null resolved room. `0x4ccec0` removes any existing owner
membership through `0x4ce240`, writes face +0x44, and appends through `0x4ce200`
to room +0x28. That helper increments the list count and walks to its tail;
face +0x5c is the next link. Thus initial accepted faces retain input order.
Reattachment removes then appends, which can change that order. The routine also
expands room +8/+0x14 bounds by face +0x10/+0x1c, with strict min/max comparisons
retaining the room value on ties. The owned-room integration now exposes these
bounds, starting with the file room bounds. Full original finalizer rejection,
reattachment and later changes are still absent from the integration.

`python tools/verify_collision_room_attachment.py` runs unchanged original
`0x4ccec0`, including its list and min/max helpers, on 460,720 initial face views
across 14,694 rooms in 94 levels. Owner pointers and append order match, and
final bounds match the PC room adapter byte-for-byte. The supplied room bounds
already contain the face bounds in all these rooms. A further 54 Live Mines
fixtures start with zero-size room bounds to exercise expansion: all 54 expand
and match. Total: 14,748 room cases and 468,138 attachments. No original helpers
are hooked. Report: `artifacts/collision-room-attachment-verification.json`.
This verifies attachment given accepted faces, not the complete original loader.

Instruction-level observations for implementing world selection at `0x4df1c0`:

- The solid is ECX; query, result and reset-limit flag are three stack arguments
  (`ret 12`). Query +0x50 contains internal flags. Bit 4 copies start/displacement
  directly into +0x54/+0x60; the other branch applies the query transform.
- An existing query face (+0) can short-circuit first-hit queries. Global
  cache count `0xca06e0` and state byte `0xca06e4` select the cached-face path;
  its pointer array begins at `0xc9f690`.
- Without the cache path, solid +0x90 controls the hierarchy/fallback decision.
  The fallback scans solid +0x70 face links via `0x45ec30`. Its exact gate
  ownership and cache invalidation still need recovery.
- The hierarchical path builds a segment AABB and expands it by query radius
  (+0x4c), then iterates the solid +0x9c room array in index order. It skips a
  room when its +1 byte is nonzero unless query bit 8 is set (`0x45ebb0`).
  `0x507990` performs inclusive AABB overlap against room +8/+0x14.
- It queries the primary room tree (+0x3c), then its +0x6c child array in index
  order, testing each child's bounds. Child checks do not repeat the +1-byte
  predicate. Positive hit count with query bit 1 exits this hierarchy path.
- `0x4ce110` changes the room detail byte (+0) and moves its pointer between
  solid +0x9c and +0xa8 arrays. The +0x6c attachment population is still open.
- Query bit 0x1000 has additional handling for room +0x184 and selected room
  faces. Cached and uncached paths differ, so neither is replaced by a generic
  scan. This branch, cache lifetime and original finalizer behavior remain open.

These observations are backed by the pinned executable instructions and exported
Ghidra functions; world selection itself is not implemented or tested yet. PC
and NXDK builds pass. The owned-room budget increases by 24 bytes for retained
bounds; current single-room peaks are 194,119 bytes for Live Mines and 826,763
bytes across all levels.

## Uncached local-space thin room query

`rf_collision_thin_rooms` reconstructs the hierarchy branch of `0x4df1c0`
for zero-radius queries already expressed in solid-local coordinates. Callers
provide room views and ordered primary/child index lists. Each primary room
passes its +1-byte gate (unless mask 8 bypasses it) and inclusive segment-AABB
overlap before its tree is queried. Its children follow in array order, each
with an overlap check but without the primary skip-byte gate. Children are not
recursively expanded. A rejected primary also suppresses its children. Nearest
hits carry the reduced fraction into subsequent trees; equal-depth hits replace
earlier hits. Mask 1 returns on the first hit. Hit counts aggregate accepted
updates across all queried trees. The output records the room index and tree
face index; the tree's source-index mapping resolves the original level face.

The helper allocates nothing and reuses each tree's stack, so simultaneous queries
need separate scratch. It validates room bounds, byte/range views and list indices;
errors preserve the result and matched outputs. A zero displacement is a miss.
It requires prepared runtime lists and does not select the original cached versus
hierarchical versus fallback path. Preferred faces, transforms, radius sweeps,
texture modes 0x80/0x100 and special room-face mode 0x1000 remain outside this
helper; the unsupported masks return RF_NOT_FOUND. Runtime list construction and
mutable room state still prevent using this as a complete gameplay world query.

`python tools/verify_collision_room_query.py` passes 2,400 complete, unmodified
original `0x4df1c0` calls with cache/preferred faces disabled, hierarchy enabled,
radius zero, direct-coordinate mask 4 and special mode disabled. Every original
callee, including tree traversal and face queries, executes unchanged. Four
single-face room trees vary primary and child order, skip bytes, overlap rejection,
zero displacement, equal depths, fraction limits and first/nearest modes. All
fraction/point/normal, room/face identities and hit counts match PC and the actual
NXDK-linked helper: 464 hits, including 152 cases with multiple accepted updates.
Six port guards cover invalid primary indices, skip bytes and child ranges, NaN
bounds/limits and the unsupported special mode, with preserved outputs. Report:
`artifacts/collision-room-query-verification.json`. Both builds, the 1,800-call
tree regression and all four CTest checks pass. This CPU fixture does not establish
XEMU gameplay correctness or correctness of the unrecovered world-query branches.

## Explicit file child-room lists

The records immediately after the variable-size room records are now identified
as initial child-room lists. `0x4edc44..0x4edc8a` reads a parent index into the
solid's +0x90 all-room array, then a child count and child indices into that same
array. Each resolved child pointer is appended to parent +0x6c with `0x45ec40`.
This step reads supplied relationships rather than computing geometric overlap.
Repeated parent records append more entries; duplicate child entries are retained.

The geometry parser now retains the record count and offset, validates parent
and child indices against the room count, and rejects truncated lists.
`rf_geometry_room_children` returns a room's children in that original append
order without allocating memory. Insufficient output capacity preserves both the
indices and count. Like the other geometry accessors, it requires an unmodified,
successfully opened geometry object. There are no new payload/index allocations;
the geometry object grows by two uint32 fields. Primary-room ordering and later
relationship rebuilding remain separate open work.

`python tools/verify_room_links.py` compares all 13,929 links across 14,694 rooms
in 94 levels with the original loader block and the PC/NXDK accessor. Original
binary reads, index lookup and append helpers run unchanged; arrays are sized
in advance so the fixture does not exercise original allocation growth. It
also compares a repeated-parent/duplicate-child/empty-list fixture against the
NXDK accessor and checks preserved outputs at insufficient capacity. Three PC
parser guards use temporary standalone VPP/RFL fixtures to reject an out-of-range
parent, an out-of-range child and an oversized truncated child list, after first
proving the base fixture loads successfully. Report:
`artifacts/room-links-verification.json`. Both builds, all 94 resident geometry
load regressions and four CTest checks pass. Full gameplay room-list binding and
XEMU validation remain open.

## Initial primary-room ordering

The room constructor tail `0x4ccdbf..0x4ccdd6` stores its all-room index and
appends the room to solid +0x90 and +0x9c. The loader then supplies the file
detail byte to `0x4ce110`. A nonzero detail value removes that room from +0x9c
and appends it to +0xa8. Removal (`0x4bf550` -> `0x4ce390`) shifts remaining
entries left, preserving their order. Thus the initial primary array contains
file-order rooms with a zero detail byte; the detail array contains the nonzero
ones in file order. Later transitions append to the destination and can change
that ordering, so re-filtering file data does not reconstruct later state.

`rf_geometry_primary_rooms` exposes this initial ordering with no allocation and
preserves indices/count on insufficient capacity. It complements the explicit
child-list accessor; neither accessor builds the world object or maintains later
state. `python tools/verify_primary_rooms.py` executes the original constructor
tail and complete detail setter, including unmodified append/removal helpers,
with preallocated arrays. All-room, primary and detail indices are checked.
Across 94 levels and 14,694 rooms, the PC and actual NXDK-linked primary accessor
match all 2,738 primary entries. A further 500 synthetic cases exercise empty
lists and detail bytes 0, 1, 2 and 255 against the original and NXDK; insufficient
capacity preserves outputs. Report: `artifacts/primary-rooms-verification.json`.
Both builds, the child-list regression and four CTest checks pass. Loaded-world
assembly, later list mutations, original face-finalizer rejection and XEMU query
validation remain open.

## Owned initial collision world

`rf_geometry_collision_world_open` assembles owned room trees, room query views,
the verified initial primary list, and explicit child lists under one budget.
Each room view points to its stable owned tree and expanded bounds. Its initial
skip byte is zero, matching constructor `0x4cccc6`; later changes to that byte
are not inferred. Geometry can be closed after construction. The world retains
all source-index mappings, so `rf_geometry_collision_world_ray` returns a level
face index alongside the room, hit geometry and accepted-update count.

Budget accounting includes the world object, room/view/list storage, owned
vertices and trees, and the largest simultaneous construction scratch. Previously
constructed rooms remain counted while subsequent rooms build. Embedded room
and tree objects are counted once. The input geometry, allocator overhead and
unrelated renderer/animation allocations are excluded. Failure closes partially
constructed rooms and preserves output. Close existing objects before reuse;
`rf_geometry_collision_world_close` releases storage and zeros the object.
Queries reuse owned tree scratch and must be serialized per world.

`python tools/verify_collision_world.py` builds all 94 worlds with an 8 MiB
collision budget, repeats at each exact measured peak, and checks that one byte
less fails without changing output. Every source face maps to its owning room
and the world covers all loaded faces. One synthetic ray per nonempty room gives
14,694 successful queries and 11,520 hits. Output bytes repeat exactly after the
input geometry is closed and another allocation is filled with poison bytes,
testing the independent lifetime of world storage. Across the levels, maximum
retained storage is 2,069,416 bytes and maximum construction peak is 2,093,376
bytes. Report: `artifacts/collision-world-verification.json`. These are collision
storage figures, not total game memory. Both builds, the original 2,400-call room
query regression on PC/NXDK and four CTest checks pass.

This is integration of the recovered initial-data paths, not a complete original
loader or gameplay world. Original face-finalizer rejection, later room/face
mutations, preferred/cached queries, transforms, special face modes and sweeps
remain open. Loaded-world guest heap and ray execution still require XEMU tests.

## First loaded-world XEMU validation

The Xbox diagnostic now constructs and retains the initial Live Mines collision
world after loading geometry. It executes the same one-ray-per-room fixture as
the PC probe and exposes separate `rf_collision_diagnostic` telemetry. The smoke
runner reads it through QMP and checks the allocation totals, query/hit/error
counts and FNV-1a checksum over all status/matched/hit output bytes against a
fresh PC run. No original executable or allocation substitution is used in the
guest: this runs the NXDK build and guest heap.

`python tools/xemu_smoke.py --scene-states --no-capture` passes on exactly
64 MiB XEMU. The world retains 1,570,360 requested bytes with a construction
peak of 1,592,392 bytes; 54 queries produce 54 hits and zero errors. The complete
output checksum is `0x2052a365`, matching PC. Available guest memory immediately
after world construction is 61,517,824 bytes. The world remains resident while
the existing authored-state scene renders all 64 frames; available memory after
CPU mesh release is 45,424,640 bytes. These are observed diagnostic memory points,
not a full-game peak or performance result. The frame itself is unchanged and
no framebuffer was captured. Evidence:
`artifacts/xemu/20260909-011409-412936/report.json`.

Both builds, all 94 PC world-load/replay cases and four CTest checks pass. This
establishes guest construction and the selected static ray fixture alongside
rendering/animation. Finalizer rejection, mutable room/face state, the other query
branches, swept actor collision and gameplay integration are still open.

## Complete supplied-plane finalizer audit

Version-180 loading supplies a non-null plane at `0x4edfbc..0x4edfca`, taking
the copy-plane branch of `0x4dfe20`. That branch preserves the supplied plane,
computes the expanded face bounds, then checks polygon area. For three or more
vertices it fans triangles from vertex zero, accumulating cross products of
`vertex[i] - vertex[0]` and `vertex[i+1] - vertex[0]`. Each cross component and
accumulated vector component is stored as binary32. The final dot product with
the supplied normal is compared to zero in x87 extended precision. Zero rejects;
either positive or negative nonzero area accepts. There is no area epsilon here.
Fewer than three vertices reject. The no-plane branch additionally computes and
normalizes a Newell-style normal and has a separate zero-normal failure; that
branch remains unreconstructed for generated or modified geometry.

`python tools/verify_face_finalizer.py` now executes the complete original
supplied-plane finalizer and every callee unchanged for all 460,720 faces across
94 levels. Every face is accepted. All resulting plane and expanded bound bytes
match the PC collision adapter. Thus the current initial world does not retain
any face the original would reject for these shipped inputs; this resolves that
specific uncertainty without claiming a general-purpose reconstructed finalizer.
Ten further original-code fixtures check a triangle, opposite/perpendicular/zero
normals, collinearity, one/two vertices, a cancelling bow-tie polygon, a nonzero
subnormal area, and an area that underflows to zero. All outcomes agree with the
observed rule. Report: `artifacts/face-finalizer-verification.json`.

This audit does not test arbitrary modified levels, reconstruct the rejection
gate in C, execute the no-plane branch, or validate runtime Geo-Mod mutations.
It adds original-executable evidence for the existing initial-data adapter; no
rendering or compiled gameplay behavior changed. Swept actor collision and
mutable/query state remain the next gameplay dependencies.

## Swept sphere against a plane

`rf_collision_sphere_plane` reconstructs complete `0x5071b0`. It first tests
strictly positive approach toward the plane front, then rejects a center behind
the plane. The original tests these dot products in extended precision but stores
approach speed and start distance as binary32 for subsequent operations. A
front-side center closer than the radius returns fraction zero and projects the
center onto the plane. Otherwise the gap must fit within the displacement's
approach component; the returned fraction is `(distance - radius) / approach`.
Contact is formed by subtracting the radius-scaled normal from the start, then
adding the fraction-scaled displacement, with the original float stores retained.

Motion away from or parallel to the plane is a miss even during initial overlap.
The helper does not normalize supplied normals. Its radius convention expects
the caller's plane convention, as in the original. Misses preserve fraction and
point; nonfinite input or negative radius returns RF_FORMAT without changing any
output. The x86 helper preserves the caller's x87 control word. This primitive
does not test whether contact lies inside a polygon, consider polygon edges or
walk room trees; it cannot yet replace the complete actor sweep.

`python tools/verify_sphere_plane.py` passes 9,000 complete unmodified original
calls, including all vector helpers, against PC and the actual NXDK-linked code.
Axial overlap/boundary cases and randomized plane normals, offsets, positions,
displacements and radii produce 2,490 hits, including 1,038 zero-fraction hits.
Every fraction/contact output byte and miss-preservation result matches. Four
port guards check negative/NaN radius, infinite start and NaN plane values.
Report: `artifacts/sphere-plane-verification.json`. Both builds and four CTest
checks pass. Sphere/edge helper `0x5072e0`, finite-polygon sweep composition,
actor response and guest execution of this new primitive remain open.

## Sphere/edge reconstruction evidence

Original `0x5072e0` takes `(contact_out, start, displacement, radius, edge_a,
edge_b, fraction_out, fraction_limit)`. It first forms edge `e = b-a` and
offset `o = start-a`, storing vector differences and six dot/length values as
binary32. The line-cylinder quadratic uses coefficients
`A = (e·d)^2 - (d·d)(e·e)` and
`B = 2((e·d)(o·e) - (e·e)(o·d))`, both stored as binary32.
The discriminant retains extended intermediates for
`B² - 4A((e·e)r² + (o·e)² - (e·e)(o·o))`; nonpositive discriminants reject,
including exact tangencies. Square root and division remain extended until root
stores. The earlier root is selected. A root in `[-0.05f, 0)` is replaced by
binary32 `0x358637bd` (0.000001). Accepted line times must be at most 1 and
strictly less than the supplied fraction limit. The projected position along
the finite edge is then checked before contact is committed.

Parallel/degenerate line cases, deeper initial overlap and projections outside
the finite edge can enter a sphere-versus-point quadratic fallback. Crucially,
the point is **edge_a only**. This routine is asymmetric under edge reversal;
the caller's ordered polygon-edge iteration is needed to cover all endpoints.
Endpoint times require nonnegative time, at most 1 and strictly below the limit.
Initial overlap behavior differs from the plane helper. The function also uses
two static scratch vectors, which the port should replace with caller-local
storage. Misses leave contact and fraction outputs unchanged.

`python tools/probe_sphere_edge.py` passes 13 analytic original-code fixtures:
interior contact, exact tangency, start-endpoint contact, omitted far-endpoint
contact and its reversed counterpart, equal/larger fraction limits, small-negative
entry, deep initial overlap, parallel travel, zero movement, zero-length edge and
zero radius. Exact output bytes and selected branch traces are recorded in
`artifacts/sphere-edge-boundaries.json`. Static initialization flags are seeded
to represent steady-state execution and avoid unrelated CRT atexit registration;
all geometric callees run unchanged. Ghidra export now includes this function.
These fixtures establish behavior for the upcoming reconstruction, not a C/NXDK
implementation comparison. No compiled gameplay or rendering behavior changed.

## Reconstructed sphere/edge helper

`rf_collision_sphere_edge` now implements the steady-state geometric helper in
shared `src/core/collision.c`, replacing original static scratch with local
arrays. Both x86 builds retain the original x87 operation order and float stores
through coefficients, discriminants, roots and projected contact. The lower
projection bound compares the extended result before rounding; the upper bound
uses its stored float. This preserves the distinction between exact negative
zero and a negative value that underflows when stored. Each x87 helper restores
the caller's control word. The non-x86 long-double fallback remains unverified.

The public wrapper requires finite vectors, a finite nonnegative radius and a
fraction limit in [0,1]; nonfinite derived dot/length terms return RF_FORMAT.
Invalid pointers return RF_RANGE. Errors preserve all outputs, while misses set
hit to zero and preserve fraction/contact. The helper performs no allocation.

`python tools/verify_sphere_edge.py` compares 9,013 complete original calls with
both the PC build and actual NXDK-linked code: the 13 analytic boundary fixtures
plus 9,000 deterministic randomized cases, including degenerate edges, stationary
spheres, axial contacts, endpoint boundaries and varying fraction limits. All
480 hit results and all miss outputs match byte-for-byte. Seven additional port
guards pass. Report: `artifacts/sphere-edge-verification.json`.

Full PC and NXDK builds, four CTest checks and the 9,000-case sphere/plane
regression pass. This is CPU-level verification, not guest execution of the new
primitive. Finite-face composition, ordered edge traversal, actor movement
response and XEMU sweep integration remain open; rendering is unchanged.

## Finite-face sphere sweep composition

`rf_collision_sweep_face` now reconstructs the geometric paths of `0x4dec10`
with a fresh per-face hit count. It reuses the existing face filter and rejects
unsupported texture-check modes 0x80/0x100 after filtering. Radius below the
original binary32 threshold at `0x5894a0` (0.0001f) uses the thin-face path.
Larger radii follow the original stages:

1. Expand face bounds by radius (`0x436db0`, `0x436d70`), then test the center
   segment against those bounds.
2. Query the one-sided sphere/plane helper. A miss returns immediately. Reject a
   plane fraction greater than the current limit; equality remains eligible.
3. Test projected polygon containment. An interior contact commits the plane
   normal, edge=0 and hits=1 immediately, without traversing edges.
4. Otherwise form componentwise start/end bounds (`0x539460`), expand them by
   radius (`0x465ee0`, `0x465ec0`), and walk ordered edges including the closing
   last-to-first edge. Each edge must pass the segment/expanded-bounds test before
   the asymmetric sphere/edge helper is called.
5. Each improving edge contact tightens the fraction limit, increments hits and
   replaces the contact. Edge fractions must be strictly below the current limit;
   equal-time edges do not replace one another. The final edge flag is 1.

Original query +0x60 supplies the sweep displacement, while +0x40 supplies the
vector used to form the edge-response normal: normalize
`start + normal_displacement * fraction - contact`. The port accepts these as
separate arguments instead of assuming they are equal. Vector multiply, add and
subtract retain their individual float stores. Normalization (`0x4faaf0`) sums
X/Y/Z squares, takes the square root, and keeps the reciprocal length extended
through each component store. The x86 helper restores the caller's x87 control
word; the non-x86 long-double fallback remains unverified.

The result contains fraction, contact, normal, edge classification and the number
of improving contacts within this face. The wrapper does not reproduce an
already-populated original output counter's return-value behavior: callers must
accumulate counts across faces and preserve world identity themselves. Misses
preserve the result and set matched=0; errors preserve both. As with other port
geometry guards, malformed data returns an error. A degenerate edge-response
normal producing nonfinite components returns RF_FORMAT rather than publishing
the original unchecked NaNs. No allocation, original static scratch or runtime
texture sampling is introduced.

`python tools/verify_collision_sweep.py` passes 12,000 complete original calls
against both PC and NXDK-linked CPU execution, with original geometric callees
unchanged and static initialization already complete. Fixtures cover axial and
rotated quadrilaterals, both windings, filters, below-threshold radii, independent
normal displacement and six explicit plane/edge limit, direction and tangency
cases. Exact output comparisons include 395 hits, 119 final edge contacts and
11 cases with multiple improving edge hits. Five malformed-input guards pass.
Report: `artifacts/collision-sweep-verification.json`.

Both builds and four CTest checks pass. The 6,000-case thin-face, 9,000-case
sphere/plane and 9,013-case sphere/edge comparisons remain green. Swept room-tree
and world traversal, transformed-query composition, texture modes, actor response
and guest execution of this new sweep remain open. There is no visual change.

## Swept tree and local room traversal

`rf_collision_sweep_tree` extends the reconstructed `0x4deab0` path to finite
radii. Every node's bounds are expanded by radius before the center segment is
tested, including radii below the face helper's thin-path threshold. Parent
faces remain first, followed by the right child before the left child through
the explicit stack. Accepted contacts tighten the fraction limit. The aggregate
count adds every improving contact returned by a face, including multiple edges,
while the result retains the latest face index and plane/edge classification.
Query flag 1 returns after the first face that accepts a contact; it does not
interrupt that face's internal edge loop. The separate normal displacement is
passed through for original query +0x40 semantics.

`rf_collision_sweep_rooms` covers the uncached, solid-local hierarchy branch of
`0x4df1c0`. It forms start/end component bounds and expands them by radius before
inclusive room-overlap checks. A primary room's skip byte and overlap gate still
control access to its child list; children ignore the skip byte and are visited
in their supplied order, without recursion. The primary tree is queried first.
Counts accumulate across room trees, nearest limits carry forward, and first-hit
mode exits after the first accepting room. Zero displacement remains a miss.
The local-coordinate branch uses the same displacement for sweep and normal
formation. Unsupported flags 0x1180 remain RF_NOT_FOUND. Preferred faces, cached
face lists, fallback solid-face lists and coordinate transforms are not yet part
of this API.

Neither query allocates memory. Existing tree scratch must be used serially;
input bounds, indices, finite radius and capacity are validated. Errors preserve
result/matched, and misses preserve result while setting matched to zero. The
reported face index is still an index in the room's reordered tree; loaded-world
integration must map it through source_indices to the level face ID.

`python tools/verify_collision_sweep_tree.py` passes 5,000 complete original
`0x4deab0` calls against PC and NXDK-linked code, retaining all original geometric
callees and the original stack probe. Three-node fixtures exercise empty face
lists, swapped child order, equal-depth faces, first/nearest modes, edge hits,
independent normal displacement and radii on both sides of the thin threshold.
There are 2,732 hits, 546 final edge contacts and 415 queries with multiple
updates. Five malformed index, cycle, limit and radius guards pass.
Report: `artifacts/collision-sweep-tree-verification.json`.

`python tools/verify_collision_sweep_rooms.py` passes 5,000 complete original
`0x4df1c0` calls in the selected local, uncached hierarchy configuration, with
all tree/face callees unchanged. Four single-face room trees exercise ordered
primary/child lists, duplicates, skip flags, overlap rejection, radius changes,
stationary queries and first/nearest modes. There are 1,632 hits, 672 final edge
contacts and 388 queries with multiple updates. Eight malformed-input and
unsupported-mode guards pass. Report:
`artifacts/collision-sweep-rooms-verification.json`.

Full PC/NXDK builds and four CTest checks pass. The 1,800-case thin-tree,
2,400-case thin-room and 12,000-case finite-face regressions also pass. These are
synthetic original/port CPU comparisons; loaded-level sweep validation, XEMU
execution, transformed/moving solids and actor collision response remain open.
Rendering is unchanged, so no screenshot was captured.

## Loaded-world sweeps and 64 MiB guest execution

`rf_geometry_collision_world_sweep` now binds the local swept-room query to the
owned collision world and translates reordered tree indices through source_indices
to level face IDs. Results retain room ID, aggregate contact count and plane/edge
classification. It shares the existing world storage and serialized tree scratch;
there is no extra per-query allocation or separate swept geometry copy.

`python tools/verify_collision_world_sweep.py` passes all 94 installed levels.
For each nonempty room, three queries use its first tree face's centroid, first
vertex and first-edge midpoint as anchors, with small coordinate perturbations
and radii 0.25, 0.5 and 0.75. Across 44,082 queries there are 39,263 hits, including
13,568 edge contacts, and zero errors. Returned face IDs are checked against the
returned room's source-index mapping. Complete output bytes replay identically
after source geometry is closed and replacement storage is filled with poison.
Exact peak budgets succeed and peak-minus-one fails without changing the output.
Maximum retained/peak world bytes remain 2,069,416 / 2,093,376. This checks owned
storage and real-data execution, not a new full original-world differential test.
Report: `artifacts/collision-world-sweep-verification.json`.

The NXDK diagnostic now runs the same three query fixtures per nonempty Live
Mines room. A separate eight-word `rf_sweep_diagnostic` publishes status, query,
hit and edge counts, errors, full-output FNV-1a checksum and available pages.
The XEMU harness resolves it from the matching linker map, reads it through QMP,
and checks a freshly computed PC reference before accepting the run. The existing
ray and animation checks remain active.

`python tools/xemu_smoke.py --scene-states --no-capture` passed with exactly
67,108,864 bytes of guest RAM and no additional memory in
`artifacts/xemu/20260909-014622-698953/report.json`. All 162 sweep queries hit,
32 ended on edges, and zero errors occurred. Output checksum `0x56cf1a44` matches
PC, including contact coordinates, normals, level face identities and counts.
The existing 54 room rays still match `0x2052a365`. Available memory was
61,509,632 bytes both before and after the sweep batch. Collision storage remained
resident throughout the existing 64-frame authored miner-state scene, with
45,416,448 bytes available after CPU mesh release. These are observed diagnostic
memory values, not a full-game peak or performance guarantee.

Both builds and four CTest checks pass. Transformed and moving-solid queries,
runtime room/face mutations, support/slide response and actual actor movement are
still open. The sweeps do not move the diagnostic miner; rendering is unchanged,
and the run explicitly performed no framebuffer capture.

## Solid-local input preparation

`rf_collision_query_local` reconstructs `0x4df1c0` entry through `0x4df302`,
including the original zero-motion branch at `0x4df227`. Query fields are origin
+0x04, matrix +0x10, original start +0x34, original displacement +0x40, flags +0x50,
local start +0x54 and local displacement +0x60. Original displacement is tested
before coordinate conversion. For finite x86 inputs, the extended squared-length
comparison with zero is equivalent to all three displacement components being
zero. An inactive query preserves the local vector outputs.

Flag 4 copies the supplied start/displacement. Otherwise, the original computes
and stores these intermediate vectors as binary32:

1. Offset = original start minus origin.
2. Local start = matrix times offset.
3. Endpoint = original start plus original displacement, then minus origin.
4. Local endpoint = matrix times endpoint.
5. Local displacement = local endpoint minus local start.

Matrix helper `0x4faa30` dots each contiguous matrix row against the vector, with
Z/Y/X products accumulated in extended precision before the float store. The
port uses the existing x87 dot helper to preserve that order and control-word
handling. It deliberately does not replace the endpoint sequence with a rotated
original displacement. Origin and matrix are ignored on the direct-copy path;
nonfinite relevant data or overflowing intermediates return RF_FORMAT and
preserve all outputs. There is no allocation or collision-world mutation.

`python tools/verify_collision_query_local.py` compares 12,000 original input
preparations with PC and actual NXDK-linked code. Original execution begins at
`0x4df1c0` and stops immediately before preferred-face selection at `0x4df302`, or
at `0x4df681` for zero original displacement. All intervening vector/matrix callees
run unchanged. Randomized translated rotations and arbitrary matrices, direct
local flags, large coordinates, tiny displacements and signed zero inputs match
byte-for-byte. There are 706 inactive queries and 1,131 active queries whose
transformed displacement rounds completely to zero. Four malformed-input guards
also pass. Report: `artifacts/collision-query-local-verification.json`.

The latter cases matter for integration: the original proceeds to face/room
selection even when the transformed displacement is zero. A future transformed
room wrapper must carry the original active decision and +0x40 normal displacement;
it cannot simply call the current local-only room wrapper, which rejects zero
local displacement and uses it for normal construction. Nonfinite contact cases
still require the port's explicit error policy. Full transformed room/contact
comparison, conversion of contacts back to world space, moving-solid list order,
and actor movement response remain open. Both builds and four CTest checks pass.
This change has no visible rendering effect.

## Transformed room-query composition

`rf_collision_transformed_rooms` now combines verified input preparation with
the uncached hierarchy path of `0x4df1c0`. It shares an internal prepared-room
query with the existing local wrapper. That internal path receives local start,
local displacement, original displacement for edge normals, and a separate
original-motion active flag. Thus it preserves the original decision to continue
when endpoint rounding collapses the local displacement to zero. The local
wrapper retains its previous zero-input behavior and passes the same vector for
both displacement roles.

The matrix/origin conversion occurs only when flag 4 is clear. Results remain
in the contact convention returned by `0x4df1c0`, including its original +0x40
edge-normal calculation; this API does not yet convert contacts back to world
space. Face indices still refer to reordered room-tree faces. Preferred-face
shortcuts, cached face lists, fallback solid-face lists and special mode 0x1000
remain outside the supported hierarchy path. Errors/misses retain the existing
output-preservation rules, and no allocation is introduced.

`python tools/verify_collision_transformed_rooms.py` passes 5,003 original calls
against PC and actual NXDK-linked code. These include 5,000 randomized translated
rotations/direct-local queries and three finite-output degenerate/inactive
fixtures. Complete original `0x4df1c0` and every transform, room, tree and face
callee run unchanged. The randomized cases yield 1,666 hits, 586 final edge
contacts and 460 queries with multiple updates; fractions, points, normals,
face/room identity, counts and classification match byte-for-byte.

One additional original fixture collapses local displacement on a coplanar
zero-radius face. The port correctly reaches the thin-plane helper and returns
its existing RF_FORMAT policy for a nonfinite candidate fraction, preserving all
outputs; it is classified as a port guard rather than a bit-identical original
result. Original call-entry tracing confirms face queries are reached for all
three collapsed-local fixtures and skipped for zero original displacement. Ten
other malformed/unsupported input checks pass, for 11 guards total. Report:
`artifacts/collision-transformed-rooms-verification.json`.

Both builds and four CTest checks pass. Regressions also pass for 5,000 direct-local
room queries, 12,000 input transformations and 44,082 loaded-world sweeps across
94 levels, including byte-identical replay after source geometry is freed. This
new transformed path has not yet run in XEMU. World-space output conversion,
moving-solid iteration and actor response remain open. Rendering is unchanged.

## Moving-solid geometric output conversion

`rf_collision_contact_world` reconstructs the geometric result block
`0x498fb4..0x499011` within the original ray-query wrapper `0x498e80`. It copies
the fraction, applies `0x4faa90` to both local normal and local point, then adds
the output origin to the already-stored transformed point using `0x40a350`.
The normal is not normalized again. `0x4faa90` dots matrix columns against the
vector with Z/Y/X extended intermediates, whereas input conversion `0x4faa30`
uses contiguous rows. The shared x87 dot helper preserves that accumulation and
restores the caller's control word. Errors from nonfinite inputs/results preserve
the output; a temporary result also permits input/output aliasing.

The original moving-solid wrapper prepares the query using object position
+0x3c and matrix +0x48, but converts the returned contact using position +0xe4
and matrix +0xfc. These field roles are established by instructions at
`0x498f5f..0x498f96` and `0x498fb4..0x499011`; their temporal meaning is still
unrecovered. The port therefore accepts an explicit output pose and does not
assume it is the same pose used for the input query. It does not infer an inverse
matrix or silently normalize a supplied matrix/normal.

`python tools/verify_collision_contact_world.py` passes 12,000 original output
blocks against PC and actual NXDK-linked code. Execution begins at `0x498fb4`
with original local fraction/point/normal stack fields and object output-pose
fields populated, then stops at `0x499011` before metadata lookup. Every vector
and matrix callee runs unchanged. Random rotations, arbitrary matrices, large
origins and tiny coordinates produce exact fraction, point and normal bytes.
Five nonfinite-input guards pass. Report:
`artifacts/collision-contact-world-verification.json`.

Both builds and four CTest checks pass; the 5,003-case transformed-room comparison
and its 11 guards remain green. Ghidra exports now include both `0x4faa90` and
`0x40a350`. The helper has not yet been exercised in XEMU. Moving-solid list
iteration, output material/object metadata, segment shortening between candidate
solids, the final static-world query and actor response remain open. No rendering
or gameplay movement behavior changes yet.

## Ordered moving/static ray composition

`rf_collision_ray_solids` now reconstructs the geometric composition in
`0x498e80`, using supplied moving-solid views in list order followed by a supplied
static world view. Input is start/end, with displacement formed by a float
subtraction. External flags are translated exactly as `0x499190`. Each moving
solid's world bounds gate its transformed, zero-radius room query; accepted
contacts use the separate output pose and retain the object's supplied ID.
External flag 1 returns on the first accepted moving solid. The static world is
queried afterward in direct-local mode, with UINT32_MAX object and solid indices.
Results include geometry, object/solid indices and room/tree-face identity;
material lookup and original ancillary output fields are not yet reconstructed.
No allocation is introduced, and all supplied collision scratch remains shared
and serialized. A NULL result is supported for visibility-only callers, skipping
output conversion; errors preserve matched/result and misses preserve result.

The shortening rule must not be replaced with an ordinary nearest-ray algorithm.
After a moving hit, the original computes a stored endpoint
`start + current_displacement * hit_fraction`, then replaces displacement with
`endpoint - start`. It retains that same hit_fraction as the next query's upper
limit rather than resetting the limit to 1 or accumulating a global fraction.
This can reject a later, geometrically closer surface, and the returned fraction
need not be relative to the initial full segment. For a ray from z=8 to z=-8:

- A mover at z=0 hits at 0.5 and shortens displacement from -16 to -8.
- A later mover at z=2 would hit the shortened ray at 0.75, so the retained 0.5
  limit rejects it and keeps the z=0 result.
- A later mover at z=4 hits at 0.5 and is accepted; shortening again yields -4.
- A subsequent static face at z=6 hits at 0.5 and is accepted. The reported
  fraction remains 0.5 even though its distance is 0.125 of the original ray.

`python tools/probe_moving_ray.py` establishes ten analytic cases through complete
unmodified `0x498e80`: retained-limit rejection/equality for movers and static
world, successive shortening, first-hit exit, reversed order, static-only hit,
and total miss. A read-only entry hook records each `0x4df1c0` displacement and
limit; all actual callees run unchanged. Original texture index -1 selects the
real no-material branch, with no substituted resource helper. Report:
`artifacts/moving-ray-boundaries.json`.

`python tools/verify_collision_solid_ray.py` compares those ten fixtures plus
2,500 deterministic randomized cases against PC and actual NXDK-linked code.
Two moving list entries and a static hierarchy use identity poses, randomized
face presence/heights and ray endpoints, and first/nearest external flags. All
2,419 hits and remaining miss outputs match, including point, normal, retained
fraction and object/solid/room/face identity. Report:
`artifacts/collision-solid-ray-verification.json`.

Both builds and four CTest checks pass, with the transformed-room regression
also passing. This combined wrapper still needs distinct-pose and optional-output
coverage; component transform checks alone do not prove those full-wrapper cases.
Runtime mover extraction/order, live poses, material metadata, caches/fallbacks,
XEMU execution and actor response remain open. This is the original ray/visibility
wrapper, not the separate swept actor movement routine. Rendering is unchanged.

## Distinct-pose wrapper verification and mover creation lead

`python tools/verify_collision_solid_poses.py` now compares 4,010 complete original
`0x498e80` calls against PC and NXDK-linked code. The ten analytic identity-pose
cases remain, followed by 4,000 queries with independently translated/rotated
input and output poses for both moving solids. The static hierarchy remains in
world coordinates. There are 3,728 hits. All result-producing cases match exact
fraction, point, normal and object/solid/room/face identity.

The set includes 1,333 visibility-only calls with a NULL result pointer, of which
1,234 hit. Both original and port leave the sentinel output buffer untouched.
Forty-four successful visibility-only calls deliberately contain an unused NaN
in an output-pose matrix, proving that the result-conversion branch is skipped
rather than needlessly validating or using that matrix. This does not relax
finite-data requirements for poses actually used to produce results. Report:
`artifacts/collision-solid-poses-verification.json`. No core change was needed;
the PC probe was extended to supply the full poses and optional output. Its build
and four CTest checks pass. The combined wrapper still has no XEMU execution or
runtime moving-solid binding.

New original global-reference exports in `moving-solid-xrefs.tsv` identify
`0x469160` as the sentinel-list initializer and `0x46b020` as a creation lead.
Ghidra output for `0x46b020` calls object factory `0x486da0` with type 9, stores
its second argument as the object's collision-solid pointer at +0x294, and
appends the new object at the tail of sentinel `0x64e6e0`: next +0x28c points to
the sentinel, previous +0x290 points to the old tail, the old tail's next points
to the new object, and global tail `0x64e970` is updated. Initial head/tail both
point to the sentinel. Therefore initial query order follows creation order,
subject to later removals/reinsertions that still need recovery.

The creator also allocates a 20-byte linked record referencing the supplied solid
and the object's +0x3c/+0x48 input pose, linking it through global `0x64e3a8`.
These are original-code leads, not a reconstructed object factory or proof of the
on-disk mover format. Next work is to trace creator callers into level loading,
recover object lifetime and pose updates, and bind those solids to the verified
query APIs. Material/output metadata and actor response remain open. Rendering
is unchanged; no screen was captured.

## Level mover records and the missing flat-face path

Original creator `0x46b020` has a direct caller at `0x463c60`. This loader reads a
count, then for each record reads an integer UID, a position through `0x514f90`,
and orientation through `0x515520`; calls solid reader `0x4ed520(stream,0,1)`;
consumes three trailing words in v180; and invokes the creator with UID, solid,
position and orientation. If object creation fails, it destroys the loaded
solid. The middle trailer read is version-gated at 0x7a (122), so it is present
for the installed v180 levels. Trailer meanings are not yet reconstructed.

`python tools/inspect_movers.py` inventories section 0x2000 directly in the
installed VPPs. All 68 levels containing that section exhaust it exactly, totaling
1,406 mover solids and 27,216 faces. Every mover has **zero rooms**, and every
face therefore uses room index UINT32_MAX. All legacy tail counts are zero.
The existing geometry inspection helper gained an explicit allow_unowned option;
static geometry keeps its previous strict room-index validation and still parses
all 94 static payloads unchanged. This option only affects the Python inventory,
not the C geometry parser. Report: `artifacts/movers.json`.

Live Mines has five records in serialized order: UIDs 8544, 8543, 8524, 8523 and
21, with respectively 22, 18, 22, 18 and 14 faces. The complete mover section is
19,897 bytes. Geometry spans, counts, raw pose values and the three unknown
trailer words are recorded, without extracting assets into tracked source.

`python tools/verify_mover_headers.py` compares all 1,406 headers with original
instructions `0x463c8e..0x463cbd`, executing the actual UID/vector/matrix readers
against memory-backed streams. Serialized orientation rows become runtime rows
2,0,1, as in the other level poses. UID and all 48 pose bytes match, and the
reader cursor reaches each inventoried geometry start. Separate original trailer
instructions `0x463cce..0x463ce5` consume exactly the inventoried final 12 bytes.
Report: `artifacts/mover-header-verification.json`. Embedded geometry reading,
resource allocation and object construction are deliberately not substituted or
claimed as executed by this bounded check.

This evidence changes the integration order: the existing moving-ray comparisons
used synthetic room hierarchies, whereas actual installed mover solids have no
rooms. The current room-only views would return no contact for them. Original
`0x4df1c0` has a separate solid-face-list fallback when its room hierarchy is
absent. That path must be reconstructed and checked before binding real movers;
creating artificial rooms would need separate equivalence evidence and is not
the current plan. Then add a bounded C mover reader, owned unassigned-face
geometry and runtime type-9 creation in file order. Pose lifetime, movement-group
records, materials and actor response remain open. No rendered result changed.

## Reconstructed zero-room fallback

`rf_collision_flat_faces` now implements the uncached no-room branch of
`0x4df1c0`. Original `0x4df45d..0x4df49a` selects the solid's face list at +0x70
when the room count at +0x90 is zero, visits its head, and follows each face's
+0x54 link via `0x45ec30`. The port receives an array in that exact list order.
It prepares local query coordinates, passes the original displacement for edge
normal construction, carries the current fraction limit between faces, and sums
all improving contacts. No synthetic room or spatial tree is created.

Unlike room-tree traversal, this branch does not inspect the first-hit flag
between faces. All eligible faces are visited even with query bit 0 set. Later
plane contacts at an equal fraction replace earlier contacts; edge contacts
retain the primitive's strict limit. The result includes final ordered face
index, aggregate count and edge classification. Empty lists/inactive original
movement miss; errors preserve result/matched. Texture-dependent modes remain
unsupported through the shared face helper. Flag 0x1000 can pass through the
ordinary flat face filter here; there is no extra room-based special list.

`rf_collision_solid_view` now includes flat_faces/flat_count. The moving/static
ray wrapper selects the existing hierarchy path when room_count is nonzero and
otherwise calls the flat fallback. A returned flat contact reports room
UINT32_MAX and a face index in the supplied flat array. The owning loader must
preserve original list order and lifetime. Both routes retain the original
outer mover-list first-hit behavior and segment-shortening rules. Neither route
allocates during queries.

`python tools/verify_collision_flat.py` passes 5,000 complete original
`0x4df1c0` calls on PC and NXDK, with all geometric and list callees unchanged.
Cases include transformed/direct inputs, thin and finite radii, empty lists,
ordinary 0x1000 filtering, ties and the lack of first-face early exit. Two explicit
four-face fixtures with bit 0 set both report four improving contacts and retain
face index 3: one visits increasingly close planes and one visits equal planes.
Overall there are 1,697 hits, 583 multi-update queries and 516 final edge contacts.
Six malformed-input guards pass. Report: `artifacts/collision-flat-verification.json`.

`python tools/verify_collision_solid_flat.py` additionally passes 2,510 complete
original `0x498e80` calls using two moving solids and a static solid with flat
face lists and identity poses. All 2,419 hit results and remaining misses match
PC/NXDK, including object/solid/face identity and room UINT32_MAX. Report:
`artifacts/collision-solid-flat-verification.json`. The previous 2,510 hierarchy
ray cases and 4,010 distinct-pose/visibility cases remain green; both builds and
four CTest checks pass.

The actual 1,406 installed mover solids are still only inventoried. Next is a
bounded C reader for their embedded geometry and owned zero-room face storage,
then runtime creation/poses and integration into the running scene. The new flat
path has not yet run in XEMU, and actor movement/material metadata remain open.
No visible scene change occurred.


### Bounded installed mover loader

`rf_geometry_movers_open` now reads v180 section 0x2000 using the recovered
0x463c60 sequence. One owned payload backs all embedded geometries; texture,
room and face indices are separately budgeted. The collection owns these
geometries: individual geometry close calls are prohibited. The budget includes
the collection, record array, payload and indices, excluding allocator overhead.
No payload duplication or unbudgeted build scratch is needed. Failures preserve
output and free partial state. Geometry parsing retains strict static room
ownership and explicitly allows UINT32_MAX ownership in mover geometry.
The legacy count/12-byte records and three trailer words are bounded; trailer
semantics remain opaque. Orientation is reordered from serialized rows to match
515520. This loads file data, not runtime type-9 objects.

`python tools/verify_mover_loader.py` passes all 68 installed sections / 1,406
movers / 27,216 faces against the independent Python inventory. It compares
record and geometry spans, counts, poses and trailers; accesses every corner and
vertex after archive closure; checks exact budgets, one-byte-short budgets,
and 5,624 truncated header/geometry/trailer boundaries with unchanged failure
outputs. Largest budget: 307,120 bytes. Report:
`artifacts/mover-loader-verification.json`. PC and NXDK builds and four CTest
checks pass. This is not execution of the original embedded geometry parser or
XEMU validation. Owned flat collision views, original face-list ordering,
runtime object creation, pose evolution and destruction still need recovery
and integration. No visible result changed.


### Owned zero-room collision geometry

`rf_geometry_collision_flat_open` copies the zero-room solid's faces and corner
vertices into owned, budgeted storage. Input may be closed after success;
failures preserve output. Retained and peak requested allocation are identical:
object + face array + corner vertices. Face indices remain file indices. This
uses supplied planes and expanded bounds, with no generated-face acceptance
implementation or texture/material mutation.

Original creation `0x4cfab0` calls `0x4dfbd0`, then appends through `0x4d3160`.
The constructor at `0x4dfbdc..0x4dfbfa` copies the six-word face metadata and
zeros owner +0x44 and other attachment pointers. The loader only invokes room
attachment for a resolved non-null room; all installed mover faces use -1.
Append `0x4d3160..0x4d3195` walks +0x54 to the tail and increments the count.
Initial metadata now supports the absent owner alongside existing room owners.

`python tools/verify_mover_flat.py` passes 68 levels, 1,406 movers and 27,216
faces. It independently reads installed planes, corner indices/vertices and
filter fields, compares the PC owned copies after source closure, and runs
complete original `0x4dfe20` supplied-plane finalization: every face is accepted
and all plane/bounds bytes match. Original append helper execution confirms
file ordering for each list. Exact and one-byte-short budgets pass; maximum
single flat allocation is 46,396 bytes. This is not the sum of all level mover
storage. Report: `artifacts/mover-flat-verification.json`.

PC/NXDK builds and four CTest checks pass. Actual loaded-solid ray/sweep
comparison, runtime creation and changing poses, lifetime, full original-loader
execution and XEMU integration remain open. No visible result changed.


### Actual loaded-mover ray and sphere queries

`python tools/verify_mover_queries.py` passes 81,648 queries across all 68
installed mover-bearing levels / 1,406 movers / 27,216 faces. Each face provides
three anchors: centroid for a thin ray, first vertex for radius .25, and first
edge midpoint for radius .75. Small deterministic offsets avoid exact coplanar
fixtures. Query flags are 0x464, 0x465 and 0x1464 respectively. Each query traverses
the complete solid's file-order face list with initial file metadata.

The complete unmodified original `0x4df1c0` zero-room path and all callees run
against linked copies of these faces. Results are byte-exact against the PC
owned mover copies after closing source geometry/archive, and the NXDK-compiled
`rf_collision_flat_faces` run under Unicorn. There are 75,723 hits, 30,758
multi-update queries and 31,865 final edge contacts. Fraction, point, normal,
face index, hit count, edge flag and preserved miss output all match. No core
correction was required. Report: `artifacts/mover-query-verification.json`.
This exercises direct/local queries, not actual changing runtime poses,
material checks, preferred-face caches, original object creation or XEMU.

The Ghidra export now includes `0x486da0` (type-9 caller's shared object factory)
and `0x48a230` with instruction-confirmed thiscall convention. The latter's
`0x48a26c..0x48a2a3` assigns the supplied position to object +0x3c, +0xe4 and
+0xf0. Its remaining path sets +0x190/+0x19c bounds from +0x180 radius (or copies
the position for nonpositive radius), then sets flag 0x04000000. These are
instruction/decompiler leads, not a ported object initializer. Factory calls
`0x49ec90(object+0x88, parameters)`; follow this physics constructor for output
orientation initialization before assuming input and output poses are equal.


### Initial mover pose and position-assignment evidence

`python tools/verify_mover_pose_initialization.py` executes original
`0x49f051..0x49f0ab`, including real vector and matrix callees, for all 1,406
installed mover poses. With physics destination object+0x88, file position
copies to object +0xe4/+0xf0; file orientation copies unchanged to +0xfc/+0x120.
The block also copies the zero initial linear vector to +0x144 and clears
+0x168. The test compares every byte of the 0x298-byte object, including
untouched sentinel fields. It verifies this bounded block, not all physics
constructor behavior.

The same tool executes complete original `0x48a230` for 8,030 cases, with the
optional debug-name lookup disabled through its normal global flags. Cases
include all file positions with radii -1, 0, .25, 1 and 10, plus 1,000 seeded
positions/radii. Exactly 4,015 calls use object+0xf0 as the source, matching
creation's alias. Position copies to +0x3c/+0xe4/+0xf0; positive radius yields
position-minus-radius bounds at +0x190 and position-plus-radius at +0x19c,
while nonpositive radius copies position to both bounds. Flag 0x04000000 is
set at +0x7c. All object bytes match; report:
`artifacts/mover-pose-initialization.json`.

Ghidra export includes physics preparation `0x49ec90` and initializer
`0x49f010`. Factory `0x486da0` supplies object+0x88 to the preparation routine,
which ends by calling the initializer. This evidence establishes the initial
output pose; it does not justify keeping the two poses synchronized during
simulation. No new C/NXDK object initializer or XEMU validation is claimed.

Next radius lead: original mover creation `0x46b075..0x46b091` takes the length
of solid+0x64, adds the float at solid+0x60 and stores the result in factory
parameter +0x84. Follow solid bound finalization `0x4cf9a0` / `0x4cf500` to
recover those quantities and their exact float stores before runtime binding.


### Original mover bounding sphere and creation radius

`python tools/probe_mover_bounds.py` executes complete `0x4cf9a0` with its
unmodified vertex-array access and `0x4cf500` sphere callee for all 1,406 mover
vertex lists. The AABB uses every serialized vertex (not only face corners),
strict comparisons and +/- float .0001 expansion. All AABB bytes and unchanged
object fields pass. Original sphere/center and creation-radius bytes are retained
in ignored `artifacts/mover-bounds-original.json` as test evidence, not runtime
data. Sphere containment is checked with a stated rounding tolerance; this
is not an exact C implementation comparison.

There are also 1,010 synthetic calls (505 sets in forward and reverse order),
including empty, singleton and duplicate vertices. Reversal changes 453 result
sets. Empty `0x4cf9a0` leaves all bounds/sphere bytes alone; standalone empty
`0x4cf500` clears radius and center only. Do not merge those contracts.

Recovered sphere algorithm for the next C implementation:

- Keep the first full vertex attaining each coordinate minimum/maximum, using
  strict comparisons. Compute squared distances between each opposing pair from
  float-stored vector differences. Select the greatest span; ties retain X,
  then Y over Z if Y strictly displaced X.
- Center starts from float-stored endpoint addition, then multiplication by .5.
  Radius starts from the length of the float-stored endpoint-minus-center vector.
- Visit every vertex in original order. Compare its extended squared distance
  to the stored float radius-squared. If outside, compute extended distance and
  new radius `(distance + stored_radius) * .5`. Store radius as float, but square
  the still-extended new radius times that stored float for radius-squared.
- Center updates use `(old_center * stored_new_radius +
  (extended_distance - stored_new_radius) * vertex) / extended_distance`,
  storing each component as float. Preserve the distinct extended and stored
  operands; replacing them with a conventional float-only sphere changes results.

Complete original creator block `0x46b075..0x46b098` confirms factory radius
parameter +0x84 is `length(solid.center at +0x64) + solid.radius at +0x60`,
with the length retained in x87 until the sum is float-stored. This conservatively
centers the object's radius at its local origin. Ghidra export now includes
`0x4cf500` and `0x4cf9a0`. C/NXDK sphere reconstruction, initial runtime binding,
object lifetime and XEMU integration remain open.


### Shared reconstructed vertex bounds

`rf_collision_vertex_bounds` implements the nonempty 4cf9a0/4cf500 path and
46b075 creation radius in shared C, with explicit x87 arithmetic for MSVC x86
and NXDK. It preserves full vertex order, axis-extreme ties, float vector stores
and extended sphere updates. No allocation is required. Empty input returns
NOT_FOUND with output unchanged; invalid/nonfinite or overflowing calculations
return an error with output unchanged. Other architectures use an unverified
long-double fallback.

`python tools/verify_vertex_bounds.py` passes exact output bytes for all 1,406
installed movers and 1,010 synthetic forward/reverse cases against original
execution. Four nonfinite/overflow guards pass. PC and NXDK agree, including
AABB, sphere radius/center and origin radius; the NXDK test checks balanced x87
stack after every call. Report: `artifacts/vertex-bounds-verification.json`.
Full PC/NXDK builds and four CTest checks pass. Initial mover binding and XEMU
execution of this new path remain open.


### Owned initial mover collision bindings

`rf_geometry_collision_movers_open` constructs file-order solid views with
owned flat collision faces, copied file UIDs and caller-provided runtime object
handles. Handles correspond to original object+0x2c, which 498e80 returns;
they are deliberately not inferred from the file UID at object+0x20. This API
does not implement the original handle registry or object factory.

Each solid computes its bounds from the full serialized vertex array, using
budgeted temporary typed storage freed before allocating collision faces.
Recovered origin radius produces position +/- radius bounds; initial input and
output positions/matrices copy the file pose. Views borrow only the collection's
owned faces. Source geometry/archive may close after success. All retained
arrays, objects and temporary vertex storage count against one peak budget;
allocator overhead and source data are excluded. Failures free partial storage
and preserve output. Zero-vertex or room-bearing solids are rejected explicitly.

`python tools/verify_mover_binding.py` passes all 68 levels / 1,406 movers on PC.
It compares exact poses and bounds against original radius/pose evidence,
verifies separately supplied diagnostic handles and file UIDs after source
closure, and checks exact and one-byte-short peak budgets. Maximum retained
and peak requests are both 201,460 bytes across the installed levels. Report:
`artifacts/mover-binding-verification.json`. Both builds and four CTest checks
pass. Combined loaded world/mover query verification, XEMU, runtime registration,
changing poses and lifetime remain open. No visible result changed.


### Combined loaded world and mover ray path

`rf_geometry_collision_ray` combines owned mover views with the resident static
world through the recovered 498e80 wrapper. Static tree face indices map back to
file face IDs; mover face indices retain file order. Runtime handles and nullable
visibility semantics are preserved. Queries allocate nothing and share world
tree scratch. This does not implement actor movement or mutable mover state.

`python tools/verify_combined_world.py` passes 41,910 queries across all 94 levels:
37,348 hits, including 25,134 mover hits and 12,214 static hits. Queries use the
first face of each nonempty static room and every mover face with its initial
pose. Tests check static-only agreement with the existing world query, ownership
of every hit identity, visibility-only agreement, and exact output replay after
closing source geometry/archive and poisoning the released storage. Largest
combined retained request is 2,132,876 bytes. Report:
`artifacts/combined-world-verification.json`. This is integration evidence over
previously compared primitives/wrappers, not a complete original loaded-world
execution comparison.

The Xbox diagnostic now loads and retains Live Mines' five mover collision
views. Their initial bounds and poses use the shared reconstructed routines;
source mover geometry is freed before queries. Diagnostic runtime handles are
explicitly supplied and remain separate from file UIDs. A native memory telemetry
block reports combined results; the smoke harness compares a fresh PC run.

`python tools/xemu_smoke.py --scene-states --no-capture` passes in exactly 64 MiB:
148 queries / 148 hits, 90 mover hits and 58 static hits, checksum e561d46f.
Mover retained and peak requests are both 12,284 bytes, alongside 1,570,360
retained static collision bytes. Available memory after mover setup is 61,493,248
bytes. Previous static ray/sweep checks and the 64-frame authored animation
sequence also pass. Report:
`artifacts/xemu/20260909-062341-789122/report.json`. Both builds and four CTest
checks pass. No screenshot was captured because the visible scene is unchanged.
Runtime handle allocation, moving-group pose updates/destruction and actor
movement response remain open.


### Moving-group section dispatch and inventory

The original level dispatch compares section 0x3000 at 0x460f5c and calls
0x463820 at 0x460f9e. The Ghidra export now includes that routine. Its v180
sequence reads a group name, two bytes, key count and key records; a legacy
UID/position/orientation array; six group flags; two integer fields; four
string/float sound pairs; and two counted ID arrays. Each key contains UID,
position, orientation, string, byte, five floats, three IDs and a final float.
The original byte reader 52c780 requests one byte (and normalizes nonzero);
52c910 requests four bytes. Version thresholds below 180 admit these fields;
the older 105..141 two-vector branch is skipped for the installed version.
Field names in the inventory are provisional, not gameplay semantics.

`python tools/inspect_moving_groups.py` parses all 68 section-bearing levels,
1,223 groups and 2,441 keyframes with bounded reads, finite float checks and
exact section exhaustion. Output: `artifacts/moving-groups.json`. Cross-checking
the second ID array finds all 1,421 entries in the corresponding level's mover
UID set. The first ID array has 212 entries, none matching movers; its identity
class remains unresolved. Repeated membership must not be rejected merely
because references exceed the 1,406 loaded mover count.

Live Mines has five groups: Door Out 01a/01b reference movers 8544/8543;
Door Out 02a/02b reference 8524/8523; big_crate_lid references 21. The four doors
have two keys each, while the lid has one. This identifies authored links, not
the interpolation or trigger semantics. Original loader calls 469250 with the
assembled group at 463bf3, which is the next registration/initialization lead.
This milestone is Python inventory and instruction tracing only; original group
parser execution, bounded C loading, runtime registration and animation remain
open. No visible scene change occurred.


### Bounded C moving-group reader

The shared level API now exposes sequential `rf_level_groups_begin` /
`rf_level_group_next`, key access and indexed access to both ID lists. Reads
stay within section 0x3000 and use no heap allocation. Group/key strings have
256-byte capacities with explicit range failure; unknown raw flags, timings,
rotation and sound values remain available. Key orientation rows are reordered
consistently with existing v180 pose readers. No boolean normalization, mode
clamp or degree conversion is silently applied to these raw file records.
The reader validates legacy poses, keeps their spans, and requires exact section
exhaustion. Failed next calls preserve both output and reader cursor.

`python tools/verify_group_reader.py` matches the independent inventory across
68 levels, 1,223 groups, 2,441 keys and 1,633 IDs, including string fields and
float bytes. All 1,223 group-final-byte truncation checks return FORMAT without
changing output or reader. Report: `artifacts/group-reader-verification.json`.
Both builds and four CTest checks pass. Original reader execution and XEMU
validation of this API remain open.

Registration export `0x469250` provides the next control-flow evidence: a group
with no keys returns null. Otherwise it creates a type-8 object through 486da0
using the first key's pose, copies three ordered arrays, configures flags/sounds,
and records the first key UID as controller object+0x20. It calls 46b320; on the
false branch it selects parameter+0x34 as the initial key index and invokes
48a230 for position assignment. The controller appends to a separate sentinel
list at 64e3b0 (tail 64e640), distinct from the type-9 solid list. These are
unverified decompiler leads pending instruction/differential checks; do not
collapse controller and mover identities or infer timing semantics from them.


### Reconstructed initial controller flags

`rf_level_group_initial_flags` maps the six raw group bytes and the first key's
last two timing floats to the initial controller flags. It normalizes nonzero
file bytes as the original 52c780 reader does. The third boolean selects
80000100 versus 80002000; first/second set bits 2/4; a set second boolean and
nonzero timing[3] or timing[4] additionally sets 40. The last three booleans
control 400/800/1000. Semantic labels remain unresolved. Empty key sets return
NOT_FOUND; nonfinite timing inputs are FORMAT; errors preserve output.

`python tools/verify_group_flags.py` executes original registration blocks
4693c9..469404 and 46949a..469570 with unchanged first-key access, then the
complete 46b320 gate. Exact PC and NXDK results match for all 1,223 installed
groups plus 4,096 combinations of raw 0/1/2/255 flag bytes and varying timing
values. Three port guards pass. Report: `artifacts/group-flags-verification.json`.
The gate simply returns bit 4 from object+318; 3,183 tested cases set it. When
clear, registration's later branch selects parameter+34 as the initial key.
This is not a full registration/playback implementation, and does not establish
sound, attachment, trigger or interpolation semantics. Both builds and four
CTest checks pass. No visual change occurred.


### Original controller-to-mover attachment loop

Expanded Ghidra xrefs now include the type-8 controller sentinel/head and the
object attachment helper. The post-load pass 46b620 walks controllers in list
order. Its second membership loop, 46b6e8..46b79c, resolves each UID through
48a4a0 and requires object type 9. Valid runtime handles append to controller
+2cc; mover+30 receives controller+2c. Controller flag 1000 additionally ORs
40000 into mover+7c. Missing or wrong-type references are removed in-place from
controller+2b4 without skipping the next entry. Duplicate references are kept.
Later controllers overwrite parent handles on multiply referenced movers.

A conditional key mutation occurs for EACH accepted mover: controller bit 4
must be set, global byte 64e97c must be zero, and controller mask 2100 must be
clear. The first key's +54 float is then multiplied by the original constant
at 589510, which is -1. Thus two accepted references reverse the sign twice.
Do not move this operation outside the loop or deduplicate references.

`python tools/probe_group_attachment.py` runs this original loop with unchanged
UID lookup, append, removal and gate helpers for all 68 levels / 1,421 mover
references. It checks ordered handles, surviving references, final parent/flag
state across groups and per-reference rotation effects. Ten synthetic cases
cover missing UIDs, wrong object type, duplicate references, flag gates and
both global-mode values. All pass. Report:
`artifacts/group-attachment-original.json`. Arrays are preallocated to avoid
original allocator growth; this is not the full attachment pass, first ID-list
handling, saved-state continuation or playback. The C port is described below.

UID resolver 48a4a0 scans the global object list at 73d890, comparing object+20,
and returns the first match. UID -1 is absent; UID -999 has an additional object
flag-bit-2 exclusion. A future registry adapter must preserve that lookup
contract instead of assuming unique IDs without evidence. The generic relative
attachment helper 48a330 has a separate caller at 47fc74; it is not called in
the verified controller mover loop, so it must not be substituted here.

### Shared C mover attachment

`rf_group_attach_movers` reconstructs this membership loop over a caller-owned
ordered object snapshot. It compacts surviving UIDs, appends runtime handles,
updates mover parents/flags and flips first-key rotation per accepted reference
under the original gate. Duplicate references and first-match lookup remain
observable, including a wrong-type first match blocking a later type-9 match.
The helper allocates nothing. It validates capacity before mutation and rejects
nonfinite rotation; errors preserve all caller state. Arrays must not overlap.

`python tools/verify_group_attachment.py` compares PC and compiled NXDK code
with the unchanged original loop for 3,000 seeded cases covering ordered
duplicate UIDs, -1/-999 lookup, missing and wrong-type objects, duplicate
references, parent/flag updates and both global-mode values. Three additional
capacity/nonfinite guards verify unchanged state on failure. Active list
contents are compared; the unused compacted tail is unspecified. All pass:
`artifacts/group-attachment-verification.json`. PC and NXDK builds and all four
CTest checks pass. This helper is not yet connected to scene controller
creation, the first ID-list pass, saved-state continuation or motion playback.
There is no new visual result.

### Controller activation state transition

`rf_group_motion_activate` reconstructs 46ac43..46acb2 and its 46adab branch
inside activation function 46aba0. It is called after activation eligibility,
not as a substitute for those checks. If next-key +2fc differs from -1,
the block leaves state alone. Otherwise bit 4 selects key zero; when bit 40 is
also set it zeros +300, clears bit 20 and sets bit 10. Without bit 4, mask 2000
selects forward stepping; otherwise it steps backward, wrapping at either end.
Mode 1 sets +30c to the corresponding final endpoint (last or zero); other
modes set -1. The bit-4 branch preserves +30c. All idle activations set bit 8,
and mode 1 clears bit 1. Field +300 is provisionally named phase; its playback
units and accumulation remain unverified.

`python tools/verify_group_activation.py` executes the original block and its
unchanged gate/count/handle helpers for 4,096 seeded states, including single
key bit-4 groups, endpoint wrapping, all six mode values, phase reset and
already-active no-ops. It checks the entire original object for unexpected
mutations and compares exact state bytes with PC and compiled NXDK C. Five
port-only malformed-input guards preserve state. All pass; report:
`artifacts/group-activation-verification.json`. Both builds and four CTest
checks pass. This does not yet recover eligibility, sound/event calls, object
wakeup, interpolation or pose propagation. No scene behavior changes yet.

New Ghidra exports distinguish debug rendering from playback: controller-list
routine 46ab70 calls 46a9c0, which draws key markers, connecting lines and text.
Activation helper 46a120 starts controller sound handles through 5056a0; it is
not the interpolator. These routines must not be used as per-frame motion
updates merely because they traverse controllers or run during activation.

### Pending-position commit and playback entry points

Complete original function 46a8f0, called from 487e00, commits pending positions
when controller flags contain either bit 8 or 80000000. It first passes its own
+f0 position to 48a230, then does the same for valid handles in +2cc followed by
+2c0. Each handle uses 40a0e0: low 16 bits index the 1,024-entry object table,
and the resolved object's full handle must match. Missing and stale handles
are skipped. Finally it clears controller mask 80000008. Other controller bits
remain. No orientation is changed by this routine; it does not compute attached
objects' pending positions or derive them from the controller position.

`python tools/probe_group_pose_commit.py` runs the entire unchanged function,
array helpers, handle lookup and position assignment with debug lookup disabled.
All 2,000 controller fixtures pass, checking complete 1,024-byte snapshots of
18,000 objects and 10,627 committed objects. Coverage includes all four dirty
mask combinations, both handle lists, duplicates, absent/stale handles and
positive/nonpositive radius bounds. Report:
`artifacts/group-pose-commit-original.json`. This is original-code evidence,
not a C port or a scene playback test.

Expanded Ghidra exports identify translation update 469800, which delegates to
46a3d0 when controller bit 4 is set. The following are decompiler-backed leads
that still require original execution before reconstruction:

- Translation uses current/next keys, +2f4 speed, +300 elapsed accumulation and
  +304 traveled distance. Direction selects different key timing fields; flag
  400 changes the timing field's use from duration to speed. Acceleration and
  deceleration affect the speed, with a clamp helper at 40a4c0.
- Timer and trigger checks occur after speed/distance accumulation. Intermediate
  position increments start from +e4 and write +f0; arrival can snap to the next
  key before key-transition logic runs on a later tick. Do not replace this
  ordering with a normalized key-position lerp.
- The bit-4 path at 46a3d0 updates +2f0 angle using first-key rotation and timing,
  direction and acceleration/deceleration envelope flags. Mode 5 has a distinct
  elapsed-time wrap. Matrix/axis helpers and terminal transitions remain open.
- 46a060 changes bit 1 in objects resolved from the first key's +4c/+50 links;
  these must not be confused with the mover membership arrays.

Next work is executing these update paths and recovering propagation into
attached objects before wiring scene motion. Existing render output is unchanged.

### Complete original translation trajectories

`python tools/probe_group_translation.py` now executes complete unchanged
469800 ticks and 46a8f0 commits with real helpers. The fixture uses two keys
eight units apart, 0.25 time steps, mode 1, disabled sound handles and absent
external key links. Four trajectories cover both directions and both duration
and speed interpretations of the timing fields. All 56 ticks match exact
expected positions and key states, including idle ticks after completion.

The pending position snaps to the endpoint on the crossing tick, while current
and next key indices remain unchanged until the following tick. Arrival logic
uses the previous accumulated distance, not just the newly computed distance.
This ordering is now verified by full execution rather than only decompilation.

Eight additional held-timer ticks (negative timer and future timer) prove that
speed, elapsed time and traveled distance accumulate while pending position
stays unchanged. Twenty-two ticks verify acceleration to target speed and
deceleration starting at the braking threshold in both directions. Two more
ticks verify a directional asymmetry: a zero braking threshold is ignored in
the forward path but accepted in the backward path. In that backward fixture
the speed reaches the clamp's 0.4 lower bound. Do not normalize these branches
into identical conditions.

All 88 ticks pass; report `artifacts/group-translation-original.json` retains
trajectory states and the additional terminal snapshots. This is a controlled
original-code baseline, not a reconstructed C translation implementation.
General float-rounding cases, other mode transitions, trigger/event effects,
rotation and attached-object pose propagation remain open. No render change.

### Shared C translation arrival transition

`rf_group_translation_arrive` reconstructs 469da4..46a02a after the arrival
position, current-key assignment, event/link effects and dwell decision. It
returns sound-start/end requests for caller dispatch rather than emitting
audio. Input must already have current_key equal to the arrived next_key.
The rotation branch is excluded, and errors preserve state and requests.

Reaching terminal_key stops next_key at -1 and resets elapsed phase. An end
sound is requested only when flag 1 is clear; reaching either path endpoint
also selects the direction for subsequent activation. Nonterminal transitions
step in the existing direction. At the path boundary, mode 1 stops, modes 2/3
reverse direction, and modes 4/5 wrap to the opposite end. Modes 2/4 additionally
set a terminal endpoint, mode 3 preserves it, and mode 5 clears it. Mode 0
preserves the original default branch, including an out-of-range next key at
a boundary; callers must not silently assume every mode loops. Nonterminal
transitions request a start sound and clear flag 1 when it was set, even if
the resulting mode transition stops. Elapsed phase resets on all these paths.

`python tools/verify_group_arrival.py` compares 10,080 original transitions
against PC and compiled NXDK code: every key in 2/3/7/128-key paths, all six
modes, both directions, both flag-1 values and terminal/nonterminal cases.
Original sound helpers execute with disabled handles; read-only observers
count 3,360 start requests and 1,680 end requests, with no request in 5,040
cases. Whole original objects are checked for unexpected mutation. All state
bytes and requests match. Seven port-only guards preserve outputs. Report:
`artifacts/group-arrival-verification.json`. This is not yet full translation,
dwell/event handling, audio playback or integration into the running scene.
PC and NXDK builds and all four CTest checks pass. No visual change occurred.

### Shared C translation numeric integration

`rf_group_translation_integrate` reconstructs the numeric work in
4698ad..469b16, before timer/trigger tests. Its caller supplies the current and
next positions, the direction-selected timing field and the current key's
acceleration/deceleration times. It returns speed, elapsed time, accumulated
distance and segment length; it does not yet change a pose or advance keys.

Position differences round to float before length calculation. The duration
mode derives target speed from length/timing; flag 400 uses timing as speed.
Acceleration applies through the equality boundary on elapsed time. Braking
uses a float-rounded deceleration rate and preserves the forward-only positive
threshold check. When acceleration and elapsed time are both zero, speed is
assigned directly. Otherwise speed is integrated, then clamped with the
original ordered tests: below 0.4 is handled before above-target. This ordering
matters when the target speed is below 0.4. Elapsed time and distance update
before the caller checks timers. The helper uses double intermediates and
explicit float stores; universal equivalence to original x87 extended
precision is not established and rounding-boundary coverage remains open.

`python tools/verify_group_integration.py` executes the unchanged original
block, vector-distance helper and clamp for 10,100 cases. All 10,085 finite
results match PC and compiled NXDK C bit-for-bit across all four output floats.
Whole original object writes are checked. Cases cover both directions/timing
modes, acceleration/braking, varied positions and tick/state values, plus zero
length and zero/negative/extreme timing. Fifteen nonfinite original outcomes
and three nonfinite-input guards instead produce port errors with unchanged
output; this difference is explicit, not a claim to reproduce those original
outcomes. Full-tick handling for those paths must be recovered before scene
integration. Report: `artifacts/group-integration-verification.json`.

### Shared C translation position step

`rf_group_translation_position` reconstructs the position work between
469b59 and 469cf7 after timer, trigger and obstruction gates allow movement.
It receives the integration input/output and committed position, returning
pending position and an arrival-processing request. Zero timing, flag 1 or
previous distance at/past segment length requests arrival and snaps to the
target. A newly crossed endpoint snaps without requesting arrival yet.
Otherwise it normalizes the float-rounded key-position difference, rounds
the normalized components, multiplies by speed and then by dt with separate
float stores, and adds the committed position. There is no direct lerp from
the first key. It allocates nothing and preserves outputs on errors.

`python tools/verify_group_position.py` uses the finite integration fixtures
and executes unchanged original vector helpers, with mode 1 excluding the
obstruction-reversal path and a read-only stop before arrival effects. All
10,085 cases match pending-position bytes and arrival requests on PC and
compiled NXDK: 3,737 intermediate moves, 42 crossing snaps and 6,306 arrival
cases. Whole original object mutations are checked, and three nonfinite-input
guards preserve outputs. Report: `artifacts/group-position-verification.json`.
Both builds and all four CTest checks pass. Double-intermediate rounding limits
remain as documented for integration; this is not full controller playback,
trigger/dwell handling or attached-object propagation. No new visual result.

### Composed translation trajectory replay

`python tools/verify_group_translation_replay.py` now compares the reconstructed
stages together against complete unchanged original 469800 ticks and 46a8f0
commits. It runs 40 trajectories of 40 ticks each: modes 1 through 5, both
directions, duration/speed timing forms and zero/half-second dwell. Motion
stages execute on both PC and compiled NXDK; the harness also executes compiled
NXDK timer expiration and deadline-setting helpers. It compares speed, elapsed
time, distance, pending/committed positions, key indices, terminal key, flags
and deadline on successive ticks. All 1,600 ticks pass, including repeated
turns/wraps and idle ticks after stopping. Report:
`artifacts/group-translation-replay.json`.

The fixture's arrival ordering is now exercised across dwell: assign the
arrived current key, clear speed/distance, and, when flag 1 is clear and dwell
is positive, set the deadline, set flag 1 and request the end sound without
running the mode transition. Later ticks still integrate while the timer is
waiting. At expiry, flag 1 requests arrival again; then the existing C mode
transition runs and can clear flag 1/request a start sound. Elapsed phase is
not reset when dwell first begins. This is an observed full-tick baseline for
these fixtures, not an inferred pause model.

The replay is diagnostic composition: Python still provides the verified dwell
decision, dirty-flag updates and pending-position commit; there is no production
full-tick wrapper yet. Keys have no external links, acceleration is zero and
sound handles are disabled. Trigger/event callbacks, dwell-rounding edge cases,
rotation, rounding limits and attached-object pose propagation remain open.
No runtime source changed in this verification step and no visual result changed.

### Staged C translation runtime

`rf_group_translation_runtime` now retains the controller motion state,
speed/distance, deadline, object flags, committed/pending positions and linear
velocity (76 bytes on x86). The caller owns an 84-byte frame snapshot. Three
allocation-free C stages replace the Python orchestration:

- `rf_group_translation_tick_begin` clears linear velocity, handles idle/flag-80
  states, selects authored key fields, applies dirty flags, integrates and
  checks the deadline. It returns IDLE, WAIT or GATES. Integration remains
  committed to runtime state on a successful WAIT result.
- At GATES, the caller must execute the original trigger/obstruction decisions
  before `rf_group_translation_tick_move`. Move updates pending position and,
  on arrival, clears speed/distance and assigns current_key before returning
  ARRIVAL. A crossing snap alone returns DONE.
- At ARRIVAL, the caller must execute key event/link effects before
  `rf_group_translation_tick_finish`. Finish performs the dwell deadline/flag
  decision or the arrival mode transition, returning sound requests and DONE.
  Dwell uses truncation of seconds*1000+0.5 under the timer's bounded domain.

Failed stages preserve runtime/frame/output state; successful earlier stages
remain committed. Frames must remain stable between calls, and outputs must
not overlap. Portal/loop-sound callbacks, trigger hold/reversal, key events and
links, and attached-object commit are caller responsibilities still awaiting
integration. Rotation is explicitly excluded. Dwell conversion boundaries and
the previously documented numeric/nonfinite limits still need broader recovery.

`python tools/verify_group_runtime.py` compares the C stages directly on PC and
compiled NXDK against 40 complete original trajectories / 1,600 ticks. All
mapped runtime bytes match, including velocity and object flags, plus deadline
and terminal key. Read-only original call observers match 48 end and 32 start
sound requests. The fixtures contain 400 idle and 40 timer-wait ticks, with
1,160 completed movement/arrival ticks. Reports retain final states:
`artifacts/group-runtime-verification.json`. Original translated-code caches
are flushed after attaching observers, so previously executed helper blocks
also receive their call observations. Both builds and all four CTest checks
pass. These fixtures allow all external gates and have absent event links and
disabled sound handles; this is not yet scene gameplay or attached-mover motion.

### Complete original attached translation propagation

46bbe0 is the attachment propagation pass. Its 0x612c stack allocation uses the
unchanged stack-probe helper 5754d0. The force argument is read at adjusted
stack+6140. Current raw Ghidra output incorrectly removes the propagation
portion as unreachable; direct instructions and complete-function execution
below establish that it runs. Do not use that truncated decompilation as
evidence that this function only collects memberships.

Collector 46c150 groups handles into 24-byte records containing the handle,
byte contribution count, byte dirty-contribution count and controller pointers.
The record has space for four pointers; the observed implementation has no
capacity check. The pass visits controllers in list order, taking +2cc handles
before +2c0 handles. Both lists use exact runtime-handle lookup; +2c0 additionally
excludes object flag 08000000. Duplicate contributions are retained. An object
updates when force is set or at least one contributor has mask 80000008 set;
once updating, all collected controllers contribute, including clean ones.

For translation without controller flag 800, the pass starts from object base
position +238 and base matrix +244. Each controller contributes its pending
+f0 minus first-key position, with float stores on subtraction and each sum.
The resulting orientation copies to +48, +fc and +120. Ordinary updates derive
velocity as float((target-committed)/dt), then pending as committed plus the
float-rounded velocity*dt. Forced updates instead assign target directly to
committed/pending positions and clear velocity. Object dirty bit 04000000 is
set. Bounds enclose committed and pending positions, then expand by +180
radius, including the original nonpositive-radius behavior.

`python tools/probe_group_propagation.py` executes complete unchanged 46bbe0,
including stack probe, collection, lookup, pose and bounds helpers. All 2,000
fixtures pass whole-mover-byte comparisons: 633 updates, including 354 forced
updates and 171 updates with multiple contributions. In 200 ordinary updates,
the velocity round trip produces a pending position different from the direct
target. Inputs cover both membership lists, absent/stale/duplicate handles,
dirty gating and positive/nonpositive radii. The fixture stays within four
contributions. Report: `artifacts/group-propagation-original.json`.

This establishes the translation path, not rotation, flag-800 orientation
alignment, collision response, production C propagation or initialization of
the base-pose fields from real level objects. Those remain open. No render change.

### Shared C attached translation propagation

`rf_group_translation_propagate` now applies the verified translation path to
a caller-owned 236-byte pose snapshot and up to four ordered contributions.
It includes every accepted contribution when forced or any contributor is
dirty, preserves duplicates, copies base orientation to all three pose matrices,
and reproduces both direct forced assignment and the normal velocity round
trip. Bounds and dirty flags update with the original ordering. No heap storage
is used; no contributors or no force/dirty state leaves the pose untouched.

The caller must collect valid handles in the original order. This helper does
not replace registry lookup, membership filtering, base-pose initialization,
rotation or flag-800 orientation alignment. Unsupported active contributions,
invalid numeric input and zero dt on normal updates return errors without
changing the pose. Forced updates do not require dt because that branch does
not divide by it.

`python tools/verify_group_propagation.py` compares all mapped pose bytes against
the 2,000 complete-original-function fixtures on PC and compiled NXDK. All pass,
including positions, velocity, three matrices, bounds, dirty/no-op behavior and
the 200 velocity-rounding differences from direct targets. Four additional
count/unsupported/nonfinite/zero-dt guards preserve state. Report:
`artifacts/group-propagation-verification.json`. The isolated NXDK instruction
budget includes its library's byte-wise copies of the pose snapshot. Both builds
and four CTest checks pass. This is not connected to scene rendering yet.

### Owned authored mover base/runtime poses

The original factory block 486ee6..486f0f copies its supplied matrix into
object+48/+244 and supplied position into +238. The expanded
`verify_mover_pose_initialization.py` executes that block with unchanged helpers
for all 1,406 installed movers, checking every object byte, in addition to the
previous physics pose/position-assignment checks. All pass. This establishes
the base poses needed by the propagation code from the authored geometry data.

`rf_geometry_collision_movers_open` now retains one 236-byte attached-pose
snapshot per mover in the existing owned allocation. It initializes authored
base/current/pending poses, all three matrices, zero velocity and the original
origin-radius bounds. Initial factory flags are 06000000: the type-9 factory
call at 46b0ae passes flag argument zero, and 486f52 ORs this mask. Later
attachment flags and state changes must still be applied by the runtime binder.
The collection's storage accounting includes these snapshots and its new pointer;
source geometry and archives may close after construction.

Updated `verify_mover_binding.py` compares every snapshot field for all 68
levels / 1,406 movers after source closure, alongside collision views, exact
and one-byte-short budgets and failure preservation. All pass; maximum retained
and peak allocation is 210,668 bytes. The 94-level combined collision replay
still passes all 41,910 queries after source closure/poisoning.

Both builds and four CTest checks pass. A fresh native-telemetry XEMU run with
`--scene-states --no-capture` passes on exactly 67,108,864 bytes of guest RAM:
`artifacts/xemu/20260909-090601-292806/report.json`. Live Mines retains five
movers in 13,468 bytes, including their new pose storage. Its combined 148-ray
fixture still returns 90 mover hits and 58 static hits, checksum e561d46f.
The snapshots are retained in the guest but not yet updated by authored
controllers or consumed by rendering; no new visible result occurred.


### Ordered controller bindings into owned mover geometry

`rf_group_translation_bind_pose` scans controller mover/general handle lists in
original 46bbe0/46c150 order, keeps duplicate references, compares full handles,
and applies the general-list 08000000 exclusion. The target must already be a
known live registered object. Dirty gating and translation propagation reuse the
reconstructed pose path; more than four contributions fail without mutation.
`tools/verify_group_binding.py` compares complete pose bytes for 2,000 fixtures
against the unchanged original function on PC and compiled NXDK code.

`rf_geometry_collision_movers_propagate` validates every target before changing
owned poses and synchronizes collision bounds, matrices and committed origins.
It allocates nothing; controller inputs must remain stable and separate from mover
storage. Normal propagation retains committed origins until the later commit pass.
Forced propagation moves those origins immediately. The extended real-level
`tools/verify_mover_binding.py` checks all 1,406 movers across 68 levels after
source closure, with unchanged budgets, exact shifted poses/views and rejection
of a rotation contribution to the final mover without partial changes.

This is shared core integration, not scene animation: authored controller
ownership, runtime handle allocation, normal position commit, rendering updates,
rotation and gameplay trigger/event/response boundaries remain open. These new
forced real-level checks run on PC; NXDK binding is checked through Unicorn, not
a new XEMU animation run.


### Position assignment for the normal commit pass

`rf_group_pose_set_position` reconstructs 48a230's pose writes: copy the supplied
position to public/current/pending, rebuild bounds as position +/- positive
radius (or a point for nonpositive radius), and set object flag 04000000.
Velocity, base pose and all matrices remain unchanged. The source may be the
pose's own pending vector, as used by original controller commit 46a8f0.
NaN/infinite inputs and overflowing bounds return RF_FORMAT without mutation;
these guards are the port's finite-data contract, not original error behavior.

`tools/verify_pose_position.py` compares all 236 mapped pose bytes against 8,030
complete original 48a230 executions on PC and compiled NXDK code, including
4,015 aliased sources, authored positions and random positions, plus three
failure-preservation guards. The original diagnostic name-lookup flags are off.
This primitive does not yet implement 46a8f0's controller dirty gating, handle
list traversal, controller pose storage or dirty-bit clearing; those and running
scene integration remain open.


### Controller position commit

`rf_group_commit_positions` now reconstructs 46a8f0 over caller-owned poses and
indexed handle slots. It checks controller flag mask 80000008, commits the
controller pose, then each valid mover/general-list reference through 48a230,
and clears the controller mask. Full handle equality rejects stale generations;
missing slots are skipped. Duplicates are retained, and general objects are
committed regardless of flag 08000000 (unlike the propagation collector).

Lists and slots are stable, separate caller storage; flags cannot overlap poses.
A two-pass finite-data validation prevents partial mutation on an invalid later
pose without heap allocation. This error policy is a port guard. Only attachment
fields of the supplied controller view are read; runtime mirrors and collision
views must be synchronized by their owner. Clean controllers return before
inspecting any pose or list. The slot table is bounded to 1,024 entries.

`tools/verify_group_commit.py` passes 2,000 complete original controller calls,
covering all 18,000 mapped object poses on PC and compiled NXDK code. It also
checks late invalid-target atomicity, clean invalid data and a controller listed
in its own attachments. The runtime still needs authored controller ownership,
position mirror synchronization, collision-view synchronization and rendering
integration before this becomes animated scene behavior.


### Synchronizing committed poses into collision views

`rf_geometry_collision_movers_sync` copies bounds and the distinct input/output
origins and matrices from owned poses into existing collision views, preserving
all face pointers, IDs and ordering. Propagation now uses this same helper, and
callers can invoke it after controller commits without allocating new geometry.
The caller supplies valid poses produced by the pose operations.

The real-level `--committed-movers` probe now performs normal propagation,
checks that collision origins have not advanced early, commits through controller
handle lists, synchronizes runtime position/flags and collision views, and retains
velocity. `tools/verify_mover_binding.py` checks the resulting full poses/views
for all 1,406 movers across 68 levels, after source closure and without changing
retained geometry budgets. It separately runs the compiled NXDK synchronization
helper over those committed poses, checking complete view bytes including fields
that must remain untouched. The test controller and handle table are diagnostic;
persistent authored controller ownership and running scene/render integration
remain open. No new XEMU or visible animation result is claimed by these checks.


### Translation controller initialization

The second integer following the file mode (`rf_level_group.unknown`) is the
selected start-key index: 463820 stores it at constructor parameter +34, and
469604/469659 read it for the translation starting position/current key. The
factory receives the first key position/orientation earlier in 469250, so the
base pose must survive selection of a different starting key.

`rf_group_translation_initialize` initializes the translation runtime after
factory/base pose creation. Caller-supplied initial flags/mode are retained;
current key is selected, next and terminal keys are -1, phase/speed/distance
and velocity are zero, and the initial deadline is current game time. It assigns
the selected position via 48a230 while preserving base pose, radius and matrices,
then synchronizes runtime positions and object flags. Invalid indices, rotation,
mode, clock or nonfinite position fail without changing either output.

`tools/verify_group_initialize.py` executes original 469570..469593 including
4fa360(0), and 4695f5..469662 including unchanged vector helpers and 48a230.
All 2,441 authored key positions, varied selected indices, radii, clocks, modes
and nonrotation flags match the complete mapped runtime/pose bytes on PC and
compiled NXDK. Six port guards also pass. This verifies the constructor state
blocks, not the complete constructor: allocation, base-pose factory setup,
registration, sound/event links, rotation and persistent scene ownership remain
open. The verifier deliberately selects every key, not only authored starts.


### Owned controller input storage

`rf_level_owned_groups_open` retains controller records, all keys and legacy
poses, and both raw UID lists in file order. It validates/sizes the section,
then uses one zeroed allocation with sequential key reads. The budget includes
the owner structure and all retained bytes; allocator overhead and bounded stack
scratch are excluded. Inputs stay stable during loading and may close afterward.
Errors leave the caller output unchanged. Closing the owner is repeatable.

All raw values are retained, including rotation, sound labels/values, key links,
flags, selected start index, and legacy poses in serialized order. This storage
performs no registration, UID-to-handle conversion, or rotation conversion.
Section offsets in records are provenance, not live archive dependencies.

`tools/verify_owned_groups.py` validates all 68 sections / 1,223 groups / 2,441
keys / 1,633 IDs / 1,641 legacy poses. Record/key/ID bytes match the independently
verified reader and legacy bytes match the independent inventory. Serialization
occurs after archive closure and overwriting the level object. Exact budget,
one-byte-short budget, truncated section, output preservation and repeated close
checks pass. Maximum retained allocation is 114,176 bytes on the 32-bit PC build.
NXDK builds successfully; persistent scene integration and XEMU lifetime checks
remain open.


### Owned controller storage in 64 MiB XEMU

The Xbox diagnostic now loads and retains `resident_groups` before constructing
mover collision views, using a 256 KiB storage budget. A separate telemetry block
hashes record/key/legacy/UID bytes in the same order as the PC owned-data probe.
It rechecks after each streamed render frame and after the level archive closes.
`tools/xemu_smoke.py` compares the complete-data checksum, counts, retained bytes
and lifetime-check count against PC and the independent group inventory.

`artifacts/xemu/20260909-093229-770905/report.json` passes with exactly 67,108,864
base RAM bytes and zero plugged memory. Live Mines retains five groups, nine
keys, five membership IDs and five legacy poses in 10,340 bytes. All 66 lifetime
checks preserve checksum 7d74d5cd (load, 64 rendered frames, archive closure).
Existing combined collision checks retain 148 hits from 148 queries, including
90 moving-solid and 58 static hits, checksum e561d46f. Observed available memory
is 42,881,024 bytes after renderer upload and 45,387,776 after CPU mesh release;
these are diagnostic observations, not full-game peak memory proof.

No framebuffer was captured because the rendered result is unchanged. Controller
storage is now resident alongside rendering, but registration, persistent runtime
poses/attachments, controller playback and mover rendering remain open.


### Controller factory pose and zero radius

Type 8 dispatch at 487330 selects 4871f8, allocating 32c bytes and calling
46c280. The 469250 factory call passes flags=1; 486f45/486f59 produce object
flags 06000001. The first key supplies base/public/physics positions and matrices.
Controller physics parameters have no collision-sphere mode. Although 49f010
skips its conditional radius assignment in that case, its final 4a0cb0 call
explicitly resets physics +f8 (object +180) to zero and rebuilds it from sphere
entries. With the controller's empty list, radius is zero and bounds are a point.
This avoids depending on unspecified allocator contents.

`rf_group_controller_pose` constructs these mapped fields from a first key,
with zero velocity, retaining the first-key base pose for later selected-key
initialization. It validates finite position/matrix inputs and preserves output
on error. `tools/verify_controller_pose.py` compares all 1,223 first keys on PC
and compiled NXDK against original 486ee6..486f63 and 49f051..49f0ab blocks,
complete 4a0cb0 with a poisoned initial radius and empty list, and unchanged
public-position vector copy. Complete mapped pose bytes and two nonfinite guards
pass. This is factory-pose reconstruction, not complete allocation, registration,
rotation playback or persistent scene controller binding.


### Persistent controller runtime collection

`rf_group_runtime_open` allocates persistent entries in authored order, borrowing
stable `rf_level_owned_groups` inputs. Each nonempty entry retains its base pose
and original initial flags. Translation entries compose the verified factory
pose and selected-key initialization at the supplied game time; file modes above
5 normalize to 1 as in 463820. Empty records retain an EMPTY marker. Rotation
entries retain source/base/flags with ROTATION_PENDING, and their translation
state is explicitly invalid. Callers must check kind before motion. This does
not implement rotation by treating it as translation or discard its authored data.

The budget covers the collection header and entries, excluding allocator overhead.
Failure preserves the caller output, and close is repeatable. No handles,
attachments or activation are created by this layer. The owned source must
outlive the runtime collection. `tools/verify_runtime_groups.py` checks all 68
levels / 1,223 entries: 1,112 translations and 111 pending rotations. Full pose
and runtime fields match the independent inventory plus original-verified
initialization semantics. Exact/short budgets, late invalid starting key and
output preservation pass; maximum runtime storage is 17,184 bytes.

The Xbox diagnostic retains this collection beside the controller inputs and
checks its pointer-free serialized fields after each streamed frame and archive
closure. `artifacts/xemu/20260909-094041-857774/report.json` passes on stock
64 MiB: Live Mines has four initialized inactive translations and one pending
rotation, 1,632 runtime bytes, checksum 84f77ab0 and 66 lifetime checks. Available
memory after renderer upload is 42,876,928 bytes, and after CPU mesh release is
45,383,680 bytes. Existing collision checks remain unchanged. No screenshot was
captured because controllers are not yet attached, activated or rendered in motion.


### Owned initial mover membership lists

`rf_group_mover_memberships_open` joins persistent runtime entries to an ordered
caller-supplied object table using the original-verified `rf_group_attach_movers`
pass. It retains accepted handles (including duplicates) in authored controller
order and commits final object parent/flag writes after all bindings succeed.
Controller handles must refer to valid caller-registered slots below 1,024; the
function does not allocate or validate the external registry itself.

Memberships have a separate bounded owner: retained entry/capacity bytes plus
peak temporary object-table and largest UID-list copies are accounted for before
allocation. Authored source lists remain unchanged. Rotation flip parity is stored
as a +/-1 factor, not as a recovered runtime angle; no rotation playback is implied.
General-object lists remain available in the source but are not bound by this
mover-only pass. Empty controller records have empty lists.

`tools/verify_member_groups.py` passes all 68 levels, 1,223 controllers, 1,406
movers and 1,421 accepted references with diagnostic handles. It checks full
ordered lists, final parent/flag state, duplicate references and flip parity
against the independent authored inventories. The archive is closed before
binding. Exact/one-byte-short budget and unchanged-output/object checks pass;
maximum retained membership bytes are 928 and maximum accounted peak is 2,352.
NXDK builds, but these owned memberships are not yet connected to the resident
Xbox object table or active scene propagation/rendering. Real registry allocation
and general-object attachment remain open.


### Resident Xbox mover membership integration

The Xbox diagnostic now retains a mover object table, controller handles and
owned membership lists beside its existing poses/collision views. It populates
the object table from loaded mover UIDs and diagnostic handles, binds authored
memberships through the shared owner, and synchronizes resulting object flags
back into the resident poses. Handle slots are disjoint between diagnostic
movers and controllers; this is not a reconstructed gameplay registry allocator.

A new telemetry block hashes the complete ordered object table and membership
count/sign/handle fields, using the same serialization as PC `--member-groups`.
Lifetime checks also verify that mover pose flags and collision IDs still agree
with the object table. Checks run at attachment, after 64 streamed render frames,
and after archive closure. The XEMU harness compares the full hash, counts and
memory accounting against the PC probe.

`artifacts/xemu/20260909-094719-594497/report.json` passes with exactly 64 MiB
base RAM and no plugged memory: five controllers, five mover objects and five
references, checksum ffb0424a over 66 checks. Membership storage is 100 retained
bytes / 204 peak bytes, plus 120 retained object/controller table bytes. Existing
collision and renderer checks still pass. Available bytes after upload and CPU
mesh release remain 42,876,928 and 45,383,680 respectively. These remain limited
diagnostic memory observations.

No new frame was captured: membership binding does not yet activate controllers
or update mover rendering. General-object memberships, registry allocation,
rotation playback and gameplay trigger/event boundaries remain open.


### Connected authored door motion sequence

`--door-cycle` composes activation, staged ticks, controller pose synchronization,
attached-mover propagation and position commits for 40 quarter-second frames.
`tools/verify_door_cycle.py` supplies the actual Live Mines door key positions,
acceleration/deceleration, dwell and mode, initialized persistent controller data,
and owned mover base poses/radii. Each of the four doors has two keys, one mover,
no general-object memberships and no key event links. This controlled diagnostic
assumes unobstructed gates and disables original sound handles; it is not the
production trigger/event/sound dispatcher.

For every frame the verifier executes original activation block 46ac43..46acb2,
then complete unchanged 469800, 46bbe0 and 46a8f0 with the real handle lookup,
collector and pose helpers. It compares all 76 controller runtime bytes and both
236-byte controller/mover poses after commit against PC. The same complete
sequence also runs through compiled NXDK functions in Unicorn and matches the
original. All four trajectories / 160 ticks pass, including movement, dwell,
return and inactive frames. The attached object moves away from its initial pose
for 21 frames on doors 01a, 01b and 02a, and 23 frames on 02b.

This closes the integration evidence gap between previously separate primitive
checks. It does not yet run motion in XEMU or update rendered mover geometry.
The crate's rotation, trigger obstruction, general-object attachment and event
execution remain outside this controlled sequence and remain open.


### Resident door cycles in 64 MiB XEMU

The Xbox diagnostic now runs the connected four-door sequence on resident
controller and mover poses after level archive closure. It validates the fixture
(two keys, one attached mover, no general IDs or key event links), uses existing
diagnostic handle slots, activates each translation, and executes 40 quarter-second
ticks. Controller poses/runtime mirrors and collision views synchronize after each
commit. The rotation entry remains untouched. This controlled pass is sequential
and runs after rendered animation, not as a real-time gameplay frame loop.

The harness hashes each tick's status, controller runtime, controller pose and
mover pose and compares all 160 records against PC `--door-cycle`, whose results
are already compared against the original executable. It separately verifies the
final runtime collection and all final collision-view poses/bounds/matrices.
Initial runtime lifetime checks remain valid before motion; final state is checked
against the changed PC reference rather than an unchanged initial-state hash.

`artifacts/xemu/20260909-095422-354974/report.json` passes with stock 64 MiB:
four doors, 160 ticks, trace checksum 28986eb3, final collision-view checksum
f8ad8e42, final runtime checksum 0dc621a9. The temporary slot table is 40 bytes,
and observed available memory during motion is 45,379,584 bytes. Existing static
collision and renderer checks pass. There is no new screenshot because mover
geometry is not yet submitted to rendering. Trigger obstruction, sound dispatch,
crate rotation, real registry allocation and general-object/event behavior remain
open; the diagnostic explicitly assumes unobstructed translation gates.


### Transformed mover geometry for the shared renderer input

`rf_preview_build_transformed` extends the existing diagnostic projection and
clipping path to solid-local geometry at a committed origin/matrix. It uses the
original-verified contact transform convention for vertices and face normals,
preserves texture UVs and level lightmap indices, and offsets geometry-local
material slots into a caller-managed scene texture table. The existing static
entry point retains its untransformed behavior. Material overflow/reserved
sentinel indices are rejected. This remains diagnostic rendering, not a claim
that the original renderer/camera/material semantics have been reconstructed.

`tools/verify_mover_preview.py` tests identity, authored and translated poses for
all 1,406 movers across 68 levels: 4,218 pose cases and 597,693 emitted vertices.
Full vertex bytes match the existing projection of geometry baked into world
space, including transformed face-normal shading, UV/lightmap coordinates and
material remapping. Exact/short mesh budgets pass. All 1,406 translated meshes
change their projected result under the fixed inspection camera. PC/NXDK builds
and the four standard tests pass.

The transformed builder is available to both backends, but mover texture
residency/remapping, adding these meshes to the scene draw list, and frame-loop
motion integration are still open. No new scene image has been captured yet.
# Player-flag contact response reference

`python tools/inspect_player_contact.py` now executes original `49d7e0` with
physics flag 80, retaining every response callee. It stops at `49cd80` after
checking the damage call's entity argument and recording its signed impact
speed. This avoids inventing gameplay damage or ownership effects. The
SHA-checked original produces 768 fixtures across movement modes 1, 3 and 8,
varying normal, velocity, support/contact velocity, direction and yaw matrix.
The contact has no object, liquid or inverse mass, and the actor is non-rotating.
These preconditions match the targeted static-world integration, not all game
collision types. Outputs are reference evidence, not a shared C comparison.

The harness reaches all nine tracked response branches. Counts are 326 outgoing
normal-retention cases, 308 normal-clearing cases, 134 contact-normal boosts,
370 incoming-normal corrections, 480 free-tangent cases, and 240 direction
tests, of which 144 take the grounded clamp/add path. Every fixture reaches
the flag-80 branch and the damage boundary. Only entity velocity changes;
the separate angular auxiliary and all other entity storage remain unchanged.
The full fixture inputs, output words and branch sets are saved in
`artifacts/player-contact-reference.json` for the upcoming C/NXDK comparison.

Instruction map for reconstruction:

- `49d93c` tests flag 80; `49d94c` enters the player response.
- `49d94c..49d9c5` prepares velocity plus support, copies original velocity,
  splits its normal/tangential components, and computes signed impact from
  contact velocity versus combined velocity. The contact dot is stored as
  binary32 before subtracting the extended combined dot.
- `49d9c9..49da86` selects boosted contact-normal velocity when the contact dot
  is positive and support velocity is exactly zero; otherwise it retains only
  outgoing normal velocity or clears it.
- `49da86..49dad7` adjusts incoming normal velocity using normalized original
  velocity and the original binary32 1.5 constant at `5893c8`. Preserve the intermediate
  stores and operation order when translating this calculation.
- `49dad7..49dafc` skips a zero tangent or uses `42a020` to select unrestricted
  tangent addition. The predicate is true for movement modes 3/8 and some
  rotating-actor cases; this fixture only covers non-rotating actors.
- `49db01..49db78` transforms entity direction `+714` through orientation `+fc`,
  tests it against normalized tangential velocity, discards opposing tangential
  motion, and clamps tangent Y to at least zero in mode 1 before adding it.
- `49ddef` continues toward damage; the harness stops at the actual damage
  entry without replacing the call or executing its gameplay effects.

The existing `rf_physics_static_contact` implements the flag-clear response
and correctly rejects flag 80. Do not remove that guard as a substitute for
the distinct response above. Player creation flags remain disabled in the
campaign diagnostic until this response and its call-site inputs are integrated.

`rf_physics_player_contact` now implements that response in shared C. The caller
supplies the original body-space `+714` direction, movement mode, and resolved
`42a020` predicate. The helper uses body orientation to reproduce `4faa90`,
preserves all body fields except velocity, and returns the signed impact for a
future damage path. It requires flag 80 and commits only after finite-result
checks. It allocates nothing. The original `5893c8` constant was checked in the
executable and is **1.5**, correcting the earlier recovery note's .5.

`python tools/verify_player_contact.py` regenerates the original fixtures and
compares all 768 outputs bit-for-bit with PC and compiled NXDK execution.
All pass, including preservation of NXDK's incoming 027f control word and
complete body storage outside velocity. Another 52 compiled guard executions
check NaN/infinity in supplied vectors/orientation, missing flag 80 and invalid
predicate values, preserving both body and output. Report
`artifacts/player-contact-verification.json` records all executable hashes.
The existing flag-clear response still passes its 512 original/PC/NXDK cases;
both full builds and five CTests pass.

This helper is not yet called by the live scene. The next integration must
verify the source and update ordering of entity direction `+714` and choose
the correct predicate from live movement state. No live XEMU player-contact
verification or full collision-loop equivalence is claimed by the CPU checks.

## Direction writer and campaign contact integration

The ordinary player input path `4a6060` selects entity `+708` as the control
record and calls `430720 -> 430760 -> 4307a0`. Thus record `+c` is the direction
at entity `+714`. Original `4307a0..430838` writes X as action 14 minus 13,
Y as 15 minus 16, and Z as 11 minus 12; enabled gates add action 3 and subtract
action 4 from Y. It does not normalize this vector. Later code processes look
axes separately. `430fc0` clears direction, turn axes and the two extra look
scalars while preserving the intervening flag byte/storage.

`tools/inspect_player_direction.py` executes this writer prefix in 512 cases,
replacing only `43d390` with explicit action values, and executes the complete
clear routine. The action order, float stores, gate combinations and untouched
record bytes all pass. Report `artifacts/player-direction-reference.json` records
the original hash and inputs. Owner selection, vehicles, locked input and the
full frame scheduler are outside this fixture; the ordinary owner path above
is supported by disassembly rather than full factory execution.

The opt-in campaign now supplies creation flag 1 to the already verified physics
flag mapper. Scene contact dispatch selects `rf_physics_player_contact` when
body flag 80 is present, supplies the shared resolved movement command as the
body-space direction, and resolves the non-rotating mode predicate for 3/8.
The helper performs the body transform once. Falling diagnostic checks use
zero direction and mode 3. Existing non-player contacts retain their response.
This preserves the adapters' existing input shaping; it does not reconstruct
the original keyboard device layer, input locking or full crouch ownership.

All eight 480-tick PC sweeps pass, and the normal 664-input trace/final image
remain identical (`artifacts/player-contact-normal-regression.json`). Campaign
initialization now explicitly checks body flag 80. Both builds, five CTests,
768 PC/NXDK contact comparisons and 52 NXDK guards pass.
Stock-64-MiB XEMU `replay-20260909-225224` passes the 480-tick diagonal movement
and turn replay: exact PC input, initial state/cache, final body and world/camera
hashes. Guest final flags are `810000f8`, including flag 80, and 64 contact records
are retained (frames 60..305). This proves the player response is exercised in
the live diagnostic; it does not compare every live contact against original
execution or prove full game collision-loop equivalence. The normal interactive
disc profile is restored afterward. No new screenshot was captured.


### Climb motion proposal (2026-09-09)

Original `49f646` dispatch selects `49e400` for descriptor 2 as well as run.
Predicate `42a0d0` is false for climb, so it skips surface projection and traction
scaling. Unlike run, `49e49c..49e4ab` retains speed divided by class acceleration
in x87 extended precision through the decay calculation. The run path stores
that ratio to binary32 before dividing by traction. Reusing the run proposal
with zero normal and traction 1 produced a one-ULP velocity mismatch in case 1.

`rf_physics_climb_propose` now shares the integration body with run while using
a separate blend calculation that preserves the original ratio. GCC/Clang x86
uses the original x87 exponential sequence and restores the caller control word;
the PC implementation passes the fixture comparison using its double calculation.
The input must already be transformed by the selected movement descriptor.

`tools/verify_run_motion.py --climb` executes original dispatch and unchanged
callees for all 384 climb cases. Full body-state results match PC and compiled
NXDK, including three-axis input, force, support and repeated passes. The existing
384 run cases also pass after the integration-body refactor, as do both builds
and all five CTest checks. This is a motion proposal, not collision traversal or
a live climbing demonstration; campaign region and movement wiring remain open.
