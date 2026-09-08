# Model-instance materials

`rf_materials_open_names` extends the existing bounded TGA loader to model
texture name lists. It shares the geometry loader's archive-order lookup,
decoding, budget accounting and failure cleanup. Slots preserve caller order
and duplicates. Empty/null/overlong names reject; absent textures remain
explicit missing slots. This provides decoded images, not original engine
texture handles or GPU residency.

All 385 unique texture names from the 733 model material records are TGA files
present in four installed archives. The new verifier loads every one and
compares dimensions and RGBA hashes with Pillow. The all-textures test batch
accounts for 38,517,912 bytes including material slots; it is not a proposed
gameplay residency set. Per-model loading, deduplication, eviction and combined
world/character memory budgeting remain required for 64 MiB Xbox gameplay.
An insufficient-budget probe verifies cleanup. PC tests and NXDK build pass.

Original transparency predicate 0x510710 calls 0x5106f0 and returns true only
for format values 4, 7 or 5. It does not inspect decoded alpha pixels. Mapping
those formats to the port's loaded image metadata remains open; no guessed
alpha classification is supplied to runtime material conversion yet.

`rf_model_material_from_disk` implements the field conversion recovered from
0x53ae5f, with texture resolution supplied by its caller. Disk +0 is a required
nonempty 32-byte texture name; +48 is an optional 32-byte secondary name.
Both must contain a terminator. Disk +32 becomes one owned 32-bit element of
runtime array +b8/+bc; disk +36/+40/+44 copy to runtime +84/+88/+8c. The names
map to runtime +14/+90 and resolved handles to +10/+b4. Empty secondary names
force handle -1. Identifier becomes zero.

Disk flags at +80 set runtime byte +8 from mask 2. Runtime flags always include
1, include 8 when disk mask 2 or resolved primary transparency (0x510710) is
true, and include 0x10 for disk mask 1. Scalars are copied as bits. Native
ownership uses the bounded instance layer; texture handles remain caller-owned.
Malformed records or budget rejection leave a fresh output unchanged.

All 733 installed material records pass conversion checks for both resolved
transparency values (1,466 cases), plus malformed-name and budget rejection
cases. Expected fields come from the recovered mapping, not a complete original
loader execution: filesystem reads, texture loading and original allocation
are deliberately outside this test. Synthetic handles verify conversion only.
Win32 Release, four CTest cases and NXDK build pass. Actual texture resolution,
model rendering and local weapon presentation integration remain open.

`rf_model_file_material` now streams one 84-byte serialized material from
the selected SUBM section without loading a LOD blob or whole model. Structural
traversal records each section's material count and offset, after validating
the record span. The accessor checks submesh/index and section bounds, reads
into temporary storage and preserves caller output on failure. Section metadata
grows by eight bytes per section; the maximum metadata increase is 1,024 bytes.

These records are not the 200-byte runtime layout. Conversion must still be
recovered before passing actual asset materials to instance ownership; no
cast or guessed mapping connects them. `tools/verify_model_material_files.py`
compares every record against independent Python archive traversal: 95 models,
95 submeshes and 733 materials match byte-for-byte. The probe also checks one
out-of-range access per submesh and confirms output preservation. Win32 Release,
four CTest cases and NXDK build pass; rendering and runtime conversion remain
open. This change produces no new visible result.

`rf_model_material_instance_open` now finishes material copying with native
ownership. It initializes a new record, applies recovered fixed fields, checks
the source array views, and independently copies the three planned arrays.
Kind 3 retains all positive-count elements; other kinds retain one. Source
mutation cannot change the instance, and instance mutation cannot change the
source. The source record's legacy pointer values are never dereferenced.
Native `arrays` and `counts` hold the owned views; pointer slots in `record`
stay zero. Close releases the one backing allocation and zeroes the instance.

This is a port ownership implementation using the recovered count/copy rules,
not a reconstruction of the original CRT allocator. The backing allocation
combines the three arrays. A caller budget includes sizeof(instance) plus
array payload; allocator metadata is excluded. Wide arithmetic prevents size
wrap, and insufficient capacities, missing required arrays, malformed names,
overflow and budget rejection leave the output unchanged. Allocation failure
returns RF_IO without publishing a partial instance. The instance must start
zeroed and be closed before reuse. New storage starts zeroed before applying
the original partial constructor, so formerly unspecified bytes are deterministic.

The model tests cover exact/insufficient budgets, independent ownership,
model-kind copy lengths, repeated close, rejection of overwriting a live
instance, insufficient source capacity, overflow, malformed names and empty
arrays. Win32 Release, four CTest cases and NXDK build pass. Original allocator
execution and live-model/Xbox runtime integration are not covered by these tests.

`rf_model_material_prepare_copy` reconstructs the fixed-field portion of
0x503950 and reports the subsequent three array lengths. It copies identifier,
flags (forcing bit 0), byte +8, RGBA, both texture handles and their scalar
metadata, fields +78/+84/+88/+8c/+b4, and the three 36-byte-bounded names through
their terminators. Destination name tails, padding and owned-array fields are
preserved. Unterminated source names fail before mutation; disjoint source,
destination and output-plan storage are required.

Source counts at +7c/+b8/+c0 select arrays addressed by +80/+bc/+c4. Positive
counts are retained in full for model kind 3; other kinds copy only the first
element. Nonpositive counts leave the destination array fields unchanged in
the original. The new helper returns this plan but does not allocate or copy
those arrays, and its output must not be published as a finished material.

`tools/verify_model_material_copy.py` compares 2,000 original executions:
567 complete no-allocation paths and 1,433 observations at the first allocator
entry. All 200 destination bytes, source immutability, surrounding canaries
and the first requested allocation size match. One malformed-name case rejects
without mutation. Successful allocation/deep-copy paths are not covered yet.
Win32 Release, four CTest cases and NXDK build pass; no runtime integration or
visual change is claimed for this preparation helper.

`rf_model_material_initialize` reconstructs the complete 0x54a7c0 constructor
over a fixed-width 200-byte record. It sets the primary identifier and both
texture handles (+10/+44) to -1, RGBA bytes at +9 to ff, +78 to 15, +b4 to -1,
and the original zero-valued fields. It preserves all bytes the original
leaves untouched, including most names and unknown fields. Raw pointer-valued
slots are not native C pointers; material ownership is not established by
initializing this record. Never use the constructor as a live-material reset
or destructor.

The original color constructor 0x50cc00 and texture constructors 0x54a610
are effectively no-ops; vector construction still executes those callees.
`tools/verify_model_material_init.py` compares all 200 bytes across 1,000
zero-filled, ff-filled, patterned and random initial states. It executes
the complete original constructor, including SEH and vector/color helpers,
and checks return pointer, surrounding canaries and restored SEH head.
Win32 Release, four CTest cases and NXDK build pass. Instance allocation,
copying and texture binding remain open; no Xbox runtime integration or
new visual result is claimed for initialization alone.

The weapon presentation mode path reaches 0x48ab90, which finds a named
material override and calls 0x48ac00. That function obtains mutable instance
materials through 0x503650, then requests textures with 0x50f6a0(name,-1,1)
and stores each result at material +10 with a 200-byte stride. The loop is
limited by both override count and material count. Texture loading, mutable
material ownership and the binding loop are not reconstructed yet.

0x503650 allocates an instance array through 0x503730 when wrapper +50 is null,
sets byte +54 to one, and returns the array and the 0x503690 count. The new
`rf_model_material_count` reconstructs that count query over resolved views:

- Kind 1 returns zero if the static model's +50 is greater than one; otherwise
  it returns the material count at the referenced mesh +84.
- Kind 2 sums every submesh's +84 count through 0x4a76f0; wrapper +8 leads to
  the animated object, with submesh count +19bc and 148-byte records at +19c0.
- Kind 3 returns the direct model count at +88.
- Other kinds return zero.

Negative submesh counts perform no iteration. Signed direct values and
32-bit wrapping sums match the original; this query is not allocation-size
validation. The future allocation layer must validate counts and byte budgets
before reserving stock Xbox memory. An insufficient supplied submesh view
returns RF_RANGE with output unchanged.

`tools/verify_model_material_count.py` compares 2,000 full original executions
of 0x503690 and its unchanged 0x4a76f0 callee, with one added bounds rejection.
It checks that the original model views remain unchanged. Win32 Release,
four CTest cases and NXDK build pass. This query is not yet integrated with
instance allocation or the Xbox diagnostic; no new visual result is claimed.
