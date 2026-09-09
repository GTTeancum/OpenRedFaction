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
