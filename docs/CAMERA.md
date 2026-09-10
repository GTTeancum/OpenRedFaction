# Camera reconstruction evidence

The preview currently uses the stored RFL spawn position/orientation, a 90-degree
horizontal field of view, and a provisional 0.1..1000 depth range. It is a frozen
diagnostic, not the original gameplay camera.

For the fingerprinted RF.exe, Ghidra exports establish these leads:

- 0x40d760 obtains camera position from the camera entity at offset 0x3c.
- 0x40ddf0 enters first-person mode through the player's entity, consuming its
  eye position at offset 0x7d4 and eye orientation at offset 0x7e0.
- The stored level spawn is therefore insufficient to reconstruct the eye point.
  The eye computation, standing/crouching offsets, model tags, animation and
  camera update path remain to trace. No arbitrary height adjustment was added.
- Original projection setup at 0x547150 uses tan(FOV * pi/180 * 0.5) in its
  degree-based path, with special handling for FOV values below 2.0. It also has
  depth-scale paths, viewport origin/dimensions and aspect handling absent from
  the current diagnostic.

`tools/inspect_camera.py` reads the actual binary32 constants referenced by that
function into artifacts/camera-evidence.json, including pi/180, 0.5, 0.000588,
31.25, 1000, 0.98 and 0.025. Their roles must be followed through the full
projection path; these constants alone are not near/far-plane specifications.

Community Camera/Entity structure names are layout leads from Dash Faction.
Raw Ghidra exports are in artifacts/analysis/rf_b8fb9ab4c9bf. No complete entity,
camera controller, collision response or cutscene camera has been recovered yet.

## Update path and helper ABI

Function 0x40d850 dispatches camera modes. Its first-person branch copies player
entity eye position (+0x7d4) into the camera entity position (+0x3c), then invokes
0x40db70 and 0x48a190. Those follow-up effects and the entity eye writer remain open.
The candidate scan in tools/scan_entity_eye.py records field displacements across
the original .text section; matching an offset does not prove entity semantics.

Original instructions verify ECX as destination for helpers 0x409f40 and 0x409f70.
The former copies a vector to ECX, then copies it again to a caller return buffer
and returns that buffer (ret 8); the latter copies a 12-byte vector to ECX (ret 4).
Their Ghidra calling conventions are now explicitly thiscall. The corrected
camera export exposes the source/destination relationship above. Some larger
caller exports still have unresolved stack/signature warnings and require more
typing; they are not compilable recovered source.

## Reconstructed eye update branch

0x4194e0 writes the entity eye point. Entity +0x29c points to class data, with
standing offset +0x9c, crouching offset +0xa8 and eye tag +0x1ec. Flags at the
object info pointer (+0x294), offset +0x728, select raw position (0x20) or linked
animated tag (0x40). Missing eye tag (-1) also uses raw position. States at
entity +0x138c and +0x1390 are tested against 8,9,10. Transition duration/elapsed
at +0x1394/+0x1398 determine the offset blend without clamping.

src/core/eye.c reconstructs the non-linked branch with explicit portable inputs;
it does not pretend a complete original entity layout exists in the new engine.
The linked branch requires model tag evaluation and returns RF_NOT_FOUND.
tools/verify_eye.py compares 400 deterministic fixtures against the original
routine in isolated Unicorn execution: maximum absolute difference is zero on
the tested domain. Actual model offsets and camera integration remain unfinished.

## Model-derived offset initialization

Original function 0x423bd0 initializes class +0x9c and, conditionally, +0xa8.
The standing path calls 0x503390 with model handle entity +0x80, the index at
entity +0x8e4 (when nonnegative), and float 1.0. It looks up the literal `eye`
through 0x503220 and stores the resulting tag at class +0x1ec. It then calls
0x5034f0(model, tag, identity rotation, zero origin, output rotation, output
position), with output position pointing directly to class +0x9c.

The crouching setup is gated by 0x428010. It calls 0x5033f0, then 0x503390 with
the index at entity +0x964 and 1.0, then 0x503360(model, 0.2f, 0, 0, 0, 1).
It evaluates the same tag into class +0xa8 using the same identity/zero inputs.
The exact semantics of these animation calls and their index sources remain
unrecovered; 0.2 is an observed argument, not yet a verified sample time.
At the end of this branch it repeats the animation setup with index +0x8e4.

Both paths write zero when the tag is -1. Otherwise, predicate 0x40a150 checks
bit 0x20000 at entity-info +0x724; when set, the setup clears offset X and Z
while retaining Y. This differs from the eye-update flags at info +0x728.

`tools/inspect_eye_setup.py` executes the original standing and crouching setup
slices for eight combinations of tag presence and flags. Only external model
lookup/evaluation/animation calls are hooked with explicitly synthetic data;
the original vector construction, assignments and flag predicate execute.
All eight cases pass, confirming six-argument tag evaluation, identity/zero
inputs, destination fields and component clearing. `artifacts/eye-setup.json`
records the call arguments. This is caller-contract evidence, not verification
of model parsing, animation correctness, or the player's actual camera height.

Model wrapper 0x503220 dispatches through 0x501220 according to the model's
type word: type 2 calls 0x51d5b0, type 1 calls 0x53c23f, and type 3 scans names.
Pose evaluation similarly dispatches through 0x5012a0. Identifying the player's
model type and recovering its asset loader and pose evaluator remain required.

## Character tag lookup reconstruction

`src/core/model.c` implements 0x51d5b0 with bounded name views rather than the
original in-memory object layout. It searches three ordered groups and returns
the combined index of the first match. Original group 1 has count +0x48 and
inline names at +0x4c with stride 0x4c. Group 2 has count +0x12b8 and name pointers
at +0x12bc with stride 0x38. Group 3 follows object +0x1a50, then +0x8c, then +4;
that final object has a name-array pointer +0x10, count +0x14 and stride 100.
These are runtime layouts, not file offsets. Group semantics remain unnamed.

The original comparator at 0x57c130 folds ASCII A..Z in its default-locale branch;
non-ASCII bytes remain unchanged. The reconstructed lookup explicitly implements
that branch and does not emulate the CRT's locale-dependent alternative.
`tools/verify_model_tags.py` executes the original lookup and comparator without
hooks for 306 synthetic fixtures and compares their exact indices with compiled
C. All cases pass. PC boundary tests additionally cover malformed name views,
missing arrays and combined-count overflow. Both MSVC and NXDK compile the code;
it is not yet called by the diagnostic renderer or backed by a model file loader.

The character pose route reaches 0x51c590 and then 0x51b2e0. Animation setup
0x51c190 resolves an index through 0x51bfd0 and writes the supplied float into
instance +0x12dc with stride 12; this does not yet establish full action semantics.
The installed meshes.vpp contains miner.v3c with `MCFR` magic; the entity table's
miner.vcm reference still needs its original filename resolution traced.

## Player asset resolution and pose dependencies

Player creation at 0x4a3310 calls 0x4251c0 with `miner1`; entity.tbl specifies
`miner.vcm` for that class. Character mesh loading at 0x51ce60 calls 0x5142d0
with `.v3c` and checks magic 0x5246434d (`MCFR`). The resolver removes the suffix
beginning at the last dot anywhere in the input and appends the supplied
extension. It does not protect dots in directory names. Preserve that behavior
when reproducing original asset references rather than applying host path rules.

`tools/inspect_model_assets.py` executes that resolver without hooks and audits
all 46 distinct entity-table .vcm references: 45 resolve to installed assets;
`edf_ship.vcm` has no matching .v3c entry. Its campaign use and any fallback remain
to investigate. The actual `miner.v3c` is 88,108 bytes in meshes.vpp. This audit
checks asset existence and headers, not mesh decoding. Results and hashes are in
`artifacts/model-assets.json`.

Pose getter 0x51b2e0 treats negative/out-of-range tags as identity, obtains group-1
parent indices at descriptor +0x94 with stride 0x4c, and walks/reverses the parent
chain before 0x51b500 evaluates stale poses. Group 2 recursively resolves a parent
at record +0x34 (stride 0x38); group 3 uses parent +0x60 (stride 100), with a
quaternion/position pair at +0x44/+0x54. The helper 0x51c620 composes these poses.
These are runtime fields; evaluation, caching and animation blending still need
reconstruction. The file reader reaches 0x53b408 for SUBM and 0x51cbe0 for BONE;
mesh record sizes cannot be inferred solely from the chunk size fields.

## Raw bone payload decoder

`rf_model_decode_bones` in src/core/model.c reads a caller-identified BONE payload:
little-endian count followed by 56-byte records containing a 24-byte name,
four binary32 rotation fields, three position fields and a signed parent index.
Original reader 0x51ca50 consumes those fields through 0x514da0, 0x515070,
0x514f90 and 0x514e10, converting the rotation through 0x519720 before composing
the runtime transform. The new decoder deliberately returns raw fields; it does
not yet implement that conversion or the original name processing at 0x576d48.

The decoder allocates no memory, checks exact payload size and caller capacity,
and rejects non-finite transforms, out-of-range parents and parent cycles before
writing output. Parent-chain validation costs O(bones * maximum chain depth).
It accepts fixed-width 24-byte names and adds a terminator in caller storage.
The payload and output storage must not overlap.

`tools/verify_bones.py` finds structurally matching BONE signatures in installed
.v3c files and compares the compiled decoder's serialized fields bit-for-bit:
95 sections, 2,452 bones pass, including miner's 25 bones. Signature scanning is
only diagnostic discovery, not the future model loader. Boundary tests exercise
truncation, capacity, invalid parents, cycles and NaNs with untouched error output.

## Local bone transform reconstruction

`rf_model_bone_transform` now reconstructs 0x519720 followed by 0x4fe900.
The former normalizes quaternion components using sqrt(1 / squared length).
The latter invokes 0x5193f0 to produce nine rotation floats, then 0x4fe860 to
copy the three position floats into the final 48-byte transform. Explicit
binary32 spills in the matrix routine are preserved by the C implementation.
Zero quaternions and non-finite inputs return RF_FORMAT without modifying output;
the original routine has no such guard and can produce non-finite results.

`tools/verify_transforms.py` compares the compiled PC implementation against those
original instructions with no hooks, using all 2,452 discovered asset bone
records plus 400 seeded quaternions at ordinary, very small and very large
magnitudes. All 2,852 transforms are bit-exact on the tested MSVC Win32 build.
This supplies local transforms only: parent composition, pose caches, animation
sampling/blending and complete V3C traversal are still required. Xbox compilation
does not by itself prove identical Xbox floating-point results.

## Parent transform composition

`rf_model_compose_transform` reconstructs original 0x51c620. ECX supplies the
local 48-byte transform, with stack arguments for output and parent. Four rows
of three floats multiply the parent with implicit final column (0,0,0,1),
confirmed by constants at 0x5a4d98. Each column's original accumulation order
is retained. A temporary result supports aliased output, as the original does.
The new API rejects non-finite inputs and overflowing outputs without writing
caller data; this validation is additional to original arithmetic semantics.

The transform verification script now compares 2,852 compositions against the
unhooked original, using asset local/parent pairs (identity for roots) and seeded
synthetic pairs. All outputs match bit-for-bit on MSVC Win32. Boundary tests
cover aliasing, invalid inputs and overflow. This verifies individual products;
it does not establish full hierarchy evaluation, original bone remapping,
animation sampling or complete model rendering.

## Bone evaluation order

The suspected remapping at 0x51cb50 is now identified as a separate byte-index
evaluation order, not a mutation of bone indices. Helper 0x51cba0 follows parent
links to determine depth. The caller emits indices in ascending depth and then
ascending original index. The descriptor stores this order at +0xf24. Bones at
equal depth retain file order, and a parent always precedes its children.

`rf_model_bone_order` reconstructs that order using 768 bytes of fixed scratch
storage, supporting up to 256 indices (the original output element is a byte).
The API rejects cyclic/out-of-range parents and values below -1 before modifying
output. Original 0x51cba0 treats all negative parents as a root; the stricter
validation matches the raw decoder's accepted parent domain. The algorithm costs
O(count * maximum depth) and allocates nothing.

`tools/verify_bone_order.py` compares unhooked original instructions to compiled
C across 95 asset skeletons and 105 synthetic cases, including empty input,
multiple roots, shuffled indices and a 256-bone chain. All 200 results match
exactly. This resolves the ordering helper; animated local transforms and the
complete model loader remain necessary for hierarchy pose evaluation.

## Structural V3C traversal

`src/core/model_file.c` now walks V3C v0x40000 directly inside VPP archives.
It records bounded section ranges without allocating or loading complete model
files. SUBM declared lengths are ignored as in the original: names, LOD envelopes,
batch descriptors, texture-name lists, materials and trailing groups determine
the next boundary. Opaque LOD blobs are skipped with checked lengths. Other
sections use their declared sizes. End placement and submesh count are checked.
The caller keeps the archive open and owns the fixed 128-section output table.

`tools/inspect_models.py` independently walks those structures in Python.
`tools/verify_model_files.py` compares the C boundaries for every installed V3C
and exercises corrupted headers, truncated endings and oversized LOD lengths.
The checked run passes 95 models, 399 section ranges and nine malformed fixtures;
MSVC/NXDK builds and existing C boundary tests also pass.
`tools/verify_bones.py` now uses structural discovery rather than signature scans;
the earlier scanning limitations above describe historical verification only.
This is a section directory: vertices, skinning data, attachments inside LOD
blobs and animations still require payload decoding and runtime integration.

## LOD attachment access

Original LOD allocation/relocation 0x569920 computes the attachment pointer by
walking the batch data, not by searching for names. It starts after aligned
56-byte batch headers, then advances through two position-sized arrays, UVs,
indices, optional triangle planes (flag 0x20), extra data, optional bone links
and optional auxiliary bytes (flag 1). Each advance aligns to 16 relative to
the LOD blob start. Auxiliary length is the LOD header word times **2 bytes**;
it is not a float count. The following attachment count describes 100-byte
records. All installed blobs end exactly after these records.

The shared reader now retains up to 128 LOD ranges and validates that computed
batch bytes plus attachment bytes equal the stored blob size. The new
`rf_model_file_attachment` reads a single record directly from its archive:
68 name bytes, four rotation floats, three position floats and a parent index.
It adds a name terminator, rejects non-finite floats and parent values below -1,
and leaves output unchanged on failure. Checking the upper parent bound requires
the model skeleton and remains a caller integration requirement.

`tools/verify_model_files.py` compares all 755 attachment records bit-for-bit
across the 95 installed models, alongside the 399 section boundaries. The miner
has six attachments in each LOD; its `eye` record references bone 8 in all three.
This is a local attachment transform, not a gameplay eye height. The remaining
work includes skeleton/attachment integration, animated bone poses, and geometry
and skinning decode. No character or camera rendering is claimed by this reader.

## Attachment transform arithmetic

Attachment setup in 0x51d420 and the third tag group in 0x51b2e0 call 0x4fe900
directly. They do not call the bone-normalization routine 0x519720. The shared
`rf_model_attachment_transform` therefore preserves the supplied quaternion
magnitude, sharing matrix construction with the normalized bone API. A zero
quaternion produces identity rotation as in the original; non-finite values and
overflow return errors without changing output. This distinction matters even
when asset quaternions are close to unit length.

The transform verifier now executes unhooked 0x4fe900 for 755 structurally located
asset attachments plus 101 synthetic cases. All 856 local transforms match
compiled Win32 C bit-for-bit. The 2,852 normalized bone and 2,852 composition
comparisons also pass after sharing the implementation. These results do not
cover animated bone poses, parent-index integration or Xbox arithmetic at runtime.

## State registration and motion-file leads

`rf_entity_state_set_open` now composes the verified name-cache and registration
helpers with table selection and compiled motion validation. It visits the 23
canonical state names in `0x418030` order, assigns looping registry IDs and
opens each new cache identity once. State aliases share a registered ID while
their cache reference counts retain every acquisition. Missing or explicitly
empty state declarations map to -1; a missing referenced motion fails the
whole operation and preserves the caller's result. Named weapon blocks remain
exact selections with no base fallback. Missing classes/weapon groups fail.

The table is read once into the same temporary allocation as the working state
set, both included in the caller's budget. The current reader rescans resident
text for each canonical name; it does not reread the archive per state. The
result contains fixed arrays of descriptors, borrowed motion handles and IDs,
with no owned heap allocation. The caller keeps the motion archive open and
must provide its result storage separately from the temporary budget. This is
port-owned composition, not a verified reconstruction of the complete original
entity registration loop, resource release or weapon-fallback policy.

`tools/verify_entity_state_sets.py` compares all 181 base/state-bearing weapon
groups against independent declaration, archive and canonical-name inventories.
180 groups succeed: 1,137 state references resolve to 972 registered motions
across those independent groups. Only the base `edf_ship` group fails, preserving
output for its missing `EDF1_Idle` input. Checks cover ID order, deduplication,
reference counts, compiled archive names, missing groups and insufficient
temporary budget. Miner1's base group registers 19 motions; its `stand` and
`crouch` states map to registered IDs 0 and 8. PC/NXDK builds and four CTest checks
pass. The new state sets are not yet consumed by scene playback; no new visual
result or emulator capture is claimed for this metadata integration.

`rf_motion_cache_acquire` reconstructs complete `0x539be0` lookup/initialization
and `0x539d00` reference acquisition. The original scans 800 descriptors of
124 bytes at `0x1c459e8`. It compares ASCII names case-insensitively after
stripping their last-dot suffix, returning the first match even when an earlier
slot is empty. Otherwise it initializes the first empty slot, preserving
unwritten bytes and the acquired name's spelling, then increments the +0x70
reference word with 32-bit wrap. Empty input preserves the original empty-name
behavior: the initialized slot still appears free to a later lookup. This
identity rule differs from the first-dot rule used to locate a compiled RFA.

The caller supplies a bounded descriptor array. Port guards reject a full
nonmatching cache, non-ASCII names and names that do not fit the original
60-byte comparison buffers. Guards preserve descriptors and output index;
the original has no equivalent safe full-cache behavior. Raw pointer fields
remain unclassified storage, not dereferenceable host pointers.
`tools/verify_motion_cache.py` executes both complete original functions and
their CRT callees without replacement. All 1,041 valid fixtures match the chosen
identity and every byte of eight supplied descriptors: 603 reuse and 438 new
records, including case/extension aliases, empty slots, 59-byte names and
reference wrap. Another 559 port guard fixtures preserve output. Payload lazy
loading, reference release and global cache ownership remain outside this API.

The running animation fixture now owns four heap descriptors (496 bytes), uses
cache-slot-plus-one identities for model registration, and releases its local
cache on every exit. It still opens four fixed files; the implementation does
not yet perform bulk authored-state registration or global payload reuse.
PC/NXDK builds and all four CTest checks pass. No new screenshot is needed for
this identity-resolution change. Numeric stock-memory XEMU validation completes
64 scene frames at `artifacts/xemu/20260908-232718-761231/report.json` without
capture; animation hashes and the final draw counts match the existing fixture.

`rf_model_register_motion` reconstructs the registry search/append portion of
`0x51cc10`. The original resolves a skeleton identity before `0x51cc42`, then
searches the count at instance +0xf58, identities at +0xf5c and exact flag bytes
at +0x120c. It reuses the first matching identity/flag pair or appends both and
increments the count. The same identity with another flag gets another ID.
The port uses caller-owned nonzero identity tokens and bounded parallel arrays;
invalid inputs or a full nonmatching registry preserve arrays and outputs.
An existing match remains valid at capacity. Resolution, lazy loading after
append and their original assertion/failure behavior remain outside this helper.

`tools/verify_motion_registration.py` executes original instructions from
`0x51cc42` to the reuse return boundary or the `0x51cc93` load-call boundary.
It does not replace executed instructions; an observation hook stops before
the excluded load call. All 2,098 valid fixtures match identities, flags, count
and reused indices exactly: 698 reuse and 1,400 append cases. The new index is
inferred from the original post-load count-minus-one expression, not a tested
loader effect. Another 302 port guard fixtures preserve all outputs. General
byte flags, duplicate matches, full-capacity reuse and bounds are covered.

The shared animation diagnostic now registers its four already opened files
and uses the resulting IDs in state/action arrays. Its four distinct resolved
files use local identity tokens 1..4; this does not establish global name-cache
identity or replace the fixed motion filenames with bulk table registration.
PC/NXDK builds, four CTest checks and the 64-frame combined scene pass at
`artifacts/xemu/20260908-232053-446765/report.json`, without capture. The PC final
image is byte-identical to its previous reference. The compiler-reported scene
stream stack subtotal is now 36,640 bytes, still excluding libraries, arguments
and kernel/interrupt usage; it remains an incomplete bound.

`rf_motion_compiled_filename` now reconstructs `0x53a9d6..0x53aa54` with
63-byte input/output limits. Executing the original block established that
its CRT call `0x575410` finds the **first** dot, including dots in directory
names: `two.dots.mvf` becomes `two.rfa`. This differs from the skeletal-model
filename helper's last-dot rule. The port copies the original input before
overwriting the extension, preserving bytes beyond the new terminator exactly
as the original does. Empty input produces `.rfa`; in-place use is supported.
Capacity failures preserve the destination and are explicit port guards.
`tools/verify_motion_filename.py` runs the unchanged instruction block and CRT
callee: 2,361 accepted cases match all 64 destination bytes exactly and 40
capacity/nontermination cases preserve output. File I/O is excluded from that
original-code comparison.

`rf_entity_state_motion_open` now composes exact table selection, compiled naming
and the existing bounded motion-file validator. It returns a handle borrowing
the caller's open motion archive; failure preserves output. Empty declarations
and missing compiled assets return NOT_FOUND without a substitute. The table
text is currently loaded and released per request, so bulk registration should
share parsed data rather than repeat that work for every state.
`tools/verify_state_motion_bindings.py` checks all 1,139 declarations against
independently indexed archive names and 80-byte headers: 1,137 open successfully,
and the two `EDF1_Idle` references remain missing. PC/NXDK builds, filename and
state guards, and four CTest checks pass. This is asset binding, not the original
registration/cache semantics or a change to the scene's scripted playback.

The port now exposes `rf_entity_state_motion_read/load` for one named `+State`
declaration in `entity.tbl`. Empty weapon means the base block; a named weapon
means only its `+Weapon Specific` block, with no inferred fallback. Keys ignore
ASCII case; an explicitly empty motion succeeds with an empty name, while an
absent declaration returns NOT_FOUND. Duplicate selected declarations and
malformed syntax are rejected. A successful output is zero-filled to 64 bytes;
all failures preserve the output. The load wrapper caps temporary table bytes
and frees them before return. It does not convert filenames or play animation.

Original `0x419a00` selects base state/action arrays when its weapon argument is
negative and weapon-specific arrays otherwise, then searches 52-byte records
and copies the matching motion string. The existing Ghidra export shows no
cross-table fallback inside this helper. This is a lead for the runtime binding,
not an unchanged-original-code equivalence test for the new text parser. The
caller and registration policy still need integration and verification.

`tools/verify_entity_states.py` independently splits class and weapon blocks
and compares all 1,139 installed declarations across 63 classes against the C
reader, including ASCII-insensitive keys and absent-state checks. Probe guards
cover base/weapon isolation, empty motions, absent class/weapon, duplicate and
malformed declarations, insufficient table budget and output preservation.
PC/NXDK builds and four CTest checks pass. Only two declarations lack a matching
compiled motion in `motions.vpp`: `edf_ship` base `stand` and `attack_stand` both
name `EDF1_Idle.mvf`; that class also lacks its compiled skeletal model. These
remain explicit missing inputs, with no replacement chosen. All 18 Live Mines
miner1 records have an empty per-entity state-animation field, so the authored
field alone does not establish their initial gameplay pose. The current scene
continues using its scripted sequence; no new visual capture was needed.

Initializer 0x418030 constructs 23 state names at 0x62f208 with 8-byte string
objects. Index 0 is `stand`, index 8 is `crouch`, and indices 9/10 are
`attack_crouch`/`attack_crouch_walk`. Entity initialization 0x422360 iterates
this list and writes 16-byte entries starting at entity +0x8e4. Thus +0x964
is the crouch slot, confirming the interpretation used by eye setup.

Registration asks 0x419a00 for a named state/action and passes the resulting
motion name to 0x51cc10. That routine reuses a registered skeleton/flag pair or
appends one, and loads the motion through 0x53a980. The latter replaces the
motion extension with `.rfa` and reads the resulting file. The miner1 table's
base `stand`/`crouch` names are `ult2_stand.mvf`/`ult2_crouch.mvf`, with matching
RFA assets present. Weapon-specific table entries also exist; their selection
must remain part of gameplay reconstruction rather than hardcoding these names.

`tools/inspect_motions.py` audits all 1,009 installed RFA files: magic `VMVF`,
596 version-7 files and 413 version-8 files. It records an 80-byte header,
the offset table sized by the word at +0x18, and candidate track ranges ending
at the word +0x48. Boundaries +0x48/+0x4c and file end split additional regions
in 57 files, including miner_talk.rfa. These region labels are deliberately
offsets, not guessed animation semantics. Track/keyframe encoding, time units,
facial-data interpretation and animation evaluation remain unfinished.

## Position key sampling

Original 0x53a130 confirms two signed 16-bit key counts at track +4/+6,
16-byte rotation keys beginning at +8, and 40-byte position keys following them.
A position key stores an int32 tick, position vector, incoming control point and
outgoing control point. Sampling clamps endpoints, returns zero for empty tracks,
and evaluates cubic Bezier interpolation between bracketing keys. Individual
scalar multiplies and vector adds round to binary32 in the original helper calls.

`src/core/motion.c` reconstructs position sampling on decoded key arrays, with
finite-field, increasing-time and overflow validation. It allocates nothing;
tick units and conversion from engine time remain caller responsibilities.
`tools/verify_motion_position.py` executes unhooked 0x53a130 against the C sampler
for all 50 tracks from ult2_stand/ult2_crouch and 101 synthetic tracks at five
times each. All 755 sampled vectors match bit-for-bit on the checked PC build.
This proves position sampling in that domain, not rotation sampling, animation
blending, full motion loading or a working campaign camera.
## Packed rotation components

`rf_motion_decode_rotation` reconstructs original `0x417e90`: four signed
little-endian 16-bit components become quaternion X/Y/Z/W floats by multiplying
by the exact float constant at `0x589524` (bits `0x38800200`). This is
`0.0000610388815402984619140625`, slightly larger than `1/16384`. The routine
does not normalize or clamp the result. It accepts unaligned bytes and checks
the input size before writing output.

`tools/verify_motion_rotation.py` executes the unhooked original instructions
and compares 65,536 vectors, permuting values so each component independently
covers every signed 16-bit value: 262,144 bit-exact component comparisons.
PC boundary tests and both PC/NXDK builds pass. This API is not yet integrated
into the Xbox scene or animated camera.

The separate track sampler at `0x539ed0` reads 16-byte rotation keys: tick at
offset 0, packed quaternion at offset 4, and signed easing bytes at offsets
12/13. It calls `0x53a040` for easing and `0x51a000` for packed interpolation,
then expands the packed result with `0x417e90`. Assembly establishes that
`0x51a5d0`, called afterward, merely copies four floats; it is not a decoder.
Raw decompilation of `0x51a000` shows interpolation quantization and a zero-W
replacement with packed value one. The reconstruction and verification of that
routine and track sampling are described below.
## Packed quaternion interpolation

`rf_motion_interpolate_rotation` reconstructs `0x51a000` for caller-normalized
blend factors in [0,1]. The original also repeatedly adds/subtracts one outside
that interval; the shared API rejects those values rather than exposing an
unbounded loop. Track sampling will supply the normalized, eased blend factor.

The sign choice uses signed 16-bit wrapped differences (`0x51a720`) and sums
(`0x51a760`), compared through packed dot products (`0x519d50`). Products sum
with 32-bit wrapping and use the original float scale `3.7257450458128005e-9`.
The difference norm rounds to float before comparison; the sum norm does not.
The chosen dot product rounds to float before threshold tests. Near-parallel
input selects the second quaternion, while the near-opposite branch mixes
permuted components. Other inputs use spherical weights, preserving the
original float spills of the angle and reciprocal sine. Components truncate
toward zero, wrap to signed 16-bit, and replace packed W zero with one.

`tools/verify_motion_interpolation.py` enumerates distinct adjacent key pairs
and single-key self-pairs from all installed RFA files, adds 1,000 deterministic
normalized synthetic pairs, and executes unhooked original `0x51a000` at
blend factors 0, 0.25, 0.5, 0.75 and 1. All 1,288,315 outputs match exactly,
covering 256,663 distinct installed pairs. This checks packed output on the
PC build; it does not establish every possible blend factor or Xbox runtime
math-library equivalence. Both builds and PC boundary/alias tests pass.
Animated skeleton integration and engine-time conversion remain open.

## Rotation track sampling and easing

`rf_motion_sample_rotation` joins `0x539ed0`, easing `0x53a040`, packed
interpolation and expansion. Keys are 16 bytes: tick, four packed components,
signed incoming/outgoing easing bytes, and two preserved reserved bytes. Empty
tracks return identity. Multi-key tracks choose the first key strictly after
the requested tick, use the preceding key as the lower endpoint, and clamp
the blend factor at either end. Endpoint evaluation still interpolates and
requantizes; copying an endpoint directly would change the original result.

Easing scales bytes by the original float `0.007874015718698502`; if their
sum exceeds one, both scaled values divide by that float sum. The piecewise
quadratic/linear/quadratic curve preserves the original float spills and uses
double intermediates to match its extended-precision arithmetic in the tested
cases. Negative easing bytes and non-increasing ticks are rejected. All
installed easing bytes are 0, 1 or 2; synthetic tracks exercise 0 through 127.

The original uses a second key at/before the first tick even when there is
only one key. The bounded C API instead decodes the single key directly.
`tools/verify_motion_sampling.py` excludes this unsafe original boundary and
tests it separately through the C boundary tests. It compares empty tracks,
single keys after their tick, multi-key endpoints and every interval midpoint,
using all 26,393 installed tracks plus 300 synthetic tracks. All 570,824
samples match the unhooked original, including decoded float bits. The PC and
NXDK builds and all four PC tests pass. This is not yet Xbox runtime validation
or a complete motion loader.

The adjacent original `0x539e10` is a blend-weight envelope, not time conversion:
it reads the per-track leading float, header start/end ticks at 0x10/0x14 and
fade durations at 0x24/0x28. Its reconstruction is described below; caller time
conversion and skeleton blending remain open.

## Per-track blend-weight envelope

`rf_motion_sample_weight` reconstructs `0x539e10` with a caller-supplied track
weight, start/end ticks and fade durations. Weights below the exact original
float threshold `1.0e-5f` return zero. The bypass flag returns the track weight
without time fades, but still applies that cutoff. Otherwise, negative elapsed
time returns zero, fade-in rises linearly, the middle holds the track weight,
and fade-out falls linearly; time after the duration returns zero. Fade-in is
tested before the duration, preserving results when a fade exceeds the motion
length. The result cannot exceed the supplied positive track weight.

The shared API rejects negative fade durations, reversed time bounds,
non-finite weights and signed time overflow without modifying output. It
allocates no memory. `tools/verify_motion_weight.py` executes original
`0x539e10` and its callees without hooks, then spills its returned x87 value to
a float just as a caller would. All 575,460 comparisons match exactly, covering
every installed track envelope (26,393), 500 synthetic envelopes, fade
boundaries, interior samples, outside times and both bypass settings. PC and
NXDK builds and all four PC tests pass. The envelope is not yet wired into
skeleton blending or the Xbox diagnostic.
## Archive-backed motion access

`include/rf/motion_file.h` and `src/core/motion_file.c` provide allocation-free
RFA access through the existing VPP reader. Open validates VMVF versions 7/8,
the 80-byte header, offset-table bounds, each track's exact length, signed key
count limits, finite track weights and time bounds. Track lengths must equal
8 + rotation_count * 16 + position_count * 40. The descriptor retains every
header word, including both additional-region boundaries, without assigning
unverified meanings to them. No entire motion or track is made resident.

Indexed track access supplies the verified blend-weight envelope and key
counts. Indexed rotation/position access converts little-endian bytes into the
shared sampler types, checks malformed numeric values and returns range errors
for out-of-bounds keys. Outputs remain unchanged on failure; failed open clears
the descriptor. The originating archive must remain open. Key access rereads
track metadata, so playback will need a cache or an adjacent-key cursor before
this path is suitable for per-frame use. Tick ordering is checked by the
samplers when given a complete track, not by a single-key read.

`tools/verify_motion_files.py` compares every decoded key and track boundary
against independent Python parsing of all 1,009 installed files: 26,393 tracks,
489,545 rotation keys and 147,716 position keys match bit-for-bit. Twelve
malformed-file fixtures are rejected, covering truncation, unsupported headers,
bad directories, track counts, time fades and non-finite weights. PC and NXDK
builds and all four existing PC tests pass. Motion playback, cached access,
engine-time conversion, additional-region semantics and skeleton integration
remain open; this is not yet an animated Xbox scene.
## Animation cursor update

The prefix of original update `0x51ba80` multiplies its elapsed float by 30
(`0x589478`) and 160 (`0x589e14`) without intermediate float spills, then calls
`__ftol` at `0x573528`. `rf_motion_elapsed_ticks` reproduces this conversion
using double intermediates and truncation toward zero, rejecting non-finite
input and results outside int32 without changing output. An elapsed input of
0.2 becomes 960 ticks. It does not retain fractional ticks between calls.

`tools/verify_motion_time.py` executes the original update entry through
`0x51badc` with the minimum valid descriptor state, including the original
conversion helper. All 8,005 comparisons pass: common frame durations,
positive/negative elapsed values and neighboring floats around tick boundaries.
This verifies the conversion prefix only. PC/NXDK builds and all four PC tests
pass; the full update routine has not been ported.

New Ghidra/assembly evidence establishes the next playback work:

- Active slots begin at instance +0x12d4, stride 12: motion index, integer
  cursor, float blend weight; count is +0x12d0. Starting a non-loop motion in
  `0x51c1c0` initializes its cursor through descriptor helper `0x53a840`.
- Update skips frozen instances (+0x1d4c), empty slot lists and descriptors
  without motions. It increments the pose cache generation at +0x1cf8.
- Motions marked in descriptor +0x120c contribute weight/duration and weight
  sums to a shared phase at instance +0x1d04. Phase wraps by subtracting one;
  each participating motion maps that phase to its own duration. Exact mapping,
  rounding and event handling still need instruction-level reconstruction.
- Unmarked active motions add the integer tick delta directly. Reaching the
  end either freezes the designated slot/instance or zeros its blend weight;
  inactive slots are subsequently removed through `0x51c090`.
- Skeleton evaluation `0x51b500` combines sampled track envelopes with slot
  weights, normalizes the positive contributions, and calls `0x51b110` to blend
  quaternion/position samples before composing parent transforms. This path
  must be recovered before treating an evaluated eye attachment as gameplay
  camera evidence.
## Local pose blending

`rf_model_blend_pose` reconstructs `0x51b110` for the evaluator's 1..16
positive-weight contributors. The caller supplies normalized weights; the API
rejects invalid counts, non-finite data and weights outside (0,1], but does not
renormalize them. One pose converts directly to a matrix without quaternion
normalization. Two poses interpolate float quaternions at the second weight
and sum separately rounded weighted positions. Three or more accumulate
positions and interpolate rotations in input order, using each weight divided
by the cumulative weight. This order is part of the recovered behavior.

The float quaternion helper follows `0x519da0`, with sign selection from
sum/difference norms, original dot-product order, float spills of the angle
and reciprocal sine, and a 1e-6 replacement when the computed W is exactly
zero. It is distinct from packed motion-key interpolation. After matrix
conversion, the three-or-more path also replaces a zero first matrix element
with 1e-6, as the original does. No allocation is performed and errors leave
the caller's matrix unchanged.

`tools/verify_pose_blend.py` runs unhooked original `0x51b110`, including its
float interpolation and matrix helpers, for 1,600 deterministic synthetic
cases covering all contributor counts from 1 to 16. All twelve output floats
match bit-for-bit in every case. PC boundary tests, PC build and NXDK build
pass. This verifies local blending on the PC; it does not establish every
degenerate quaternion case, Xbox runtime arithmetic equivalence, the complete
skeleton evaluator, root-motion handling or gameplay camera integration.
## Sampling directly from archives

`rf_motion_file_sample` now evaluates a track's rotation, position and blend
weight at an integer tick without allocating the whole track. File open checks
strictly increasing ticks for each rotation and position stream, permitting
binary search for the adjacent pair. Evaluation holds at most two rotation
keys and two position keys and commits the output only after every read and
sampler succeeds. The archive must remain immutable while the handle is open.

An exact interior position key requires curve evaluation even at blend factor
zero. Treating its selected pair as a whole track would copy the first position
instead, changing signed-zero results in installed `esci_walk.rfa` track 26.
The shared cubic evaluation was extracted as `rf_motion_interpolate_position`
so archive sampling preserves that distinction without reading extra keys.

The expanded `tools/verify_motion_files.py` compares 131,965 archive-backed
samples against full-track linear evaluation, covering five times per installed
track (start-1, start, midpoint, end, end+1) and both fade modes. All output bits
match. This is an integration comparison using the previously original-verified
samplers, not a new direct-original comparison of every sample. All installed
keys still match their serialized bytes, and 62 malformed fixtures—including
duplicate ticks in both key streams—are rejected. The 755 direct-original
position comparisons, PC tests and PC/NXDK builds also pass.

Metadata and key reads still seek into the archive. A playback cache or cursor,
the original slot update state machine, skeleton evaluation and Xbox runtime
validation remain necessary before animation can drive the gameplay scene.
## Initial single-motion skeleton and animated eye

`rf_model_sample_single_motion` connects archive-backed samples and envelopes
to depth-ordered parent composition. It supports the initial pose with one
active motion, no root displacement and no bone overrides. Positive envelopes
produce the sampled local transform without bone-quaternion normalization;
zero envelopes produce identity. Every root explicitly adds zero displacement,
matching original signed-zero results. Caller-owned output matrices avoid a
large temporary skeleton allocation; failures can leave partial output, which
the caller must discard. Model and motion bone counts must match (maximum 256).

`tests/skeleton_probe.c` loads the miner's BONE section and the requested RFA
through shared C readers, evaluates the skeleton, and composes the LOD-0 `eye`
attachment with its animated parent. `tools/verify_skeleton.py` compares all
bone matrices with unhooked original `0x51b500`, then compares the eye with
unhooked original tag evaluation `0x51b2e0`. All 300 bone matrices and 12 eye
transforms match exactly across six times each in `ult2_stand.rfa` and
`ult2_crouch.rfa`. Descriptor/instance state explicitly has one non-loop slot,
weight one, fresh pose generation, zero root displacement and no overrides.

At tick 1120, the stand eye's model-space translation is approximately
(0.003951758, 0.785902619, 0.076405764); crouch is
(0.061145991, 0.148407608, 0.383740306). These are evaluated asset poses, not
established world-space camera heights. At/after end tick 9600, the tested
non-loop envelope becomes zero and the eye reduces to its local attachment
transform. Actual looping and transition state must therefore be recovered
before choosing a gameplay pose. Original object scaling, root displacement,
slot selection and bone overrides remain outside this helper.

PC and NXDK builds and all four PC tests pass. The Xbox diagnostic has not yet
executed this skeleton path; this verification does not establish animation
performance, hardware equivalence or complete gameplay-camera integration.
## Character tag placement

`rf_model_place_tag` reconstructs the placement portion of `0x5034f0` after
character tag evaluation. The original calls `0x503230` → `0x5012a0`; model
type 2 obtains the tag matrix via `0x51c590` → `0x51b2e0`, then extracts its
orientation and position. It multiplies tag orientation by the supplied
orientation through `0x40ea80`, rotates its position via `0x4facb0`, rounds
that rotated position to float, and adds the supplied position. There is no
additional scale parameter or scale multiplication in this character wrapper.
This does not exclude scale already present in its inputs or earlier setup.

Its matrix accumulation order and separate translation rounding differ from
`0x51c620` bone composition, so the shared implementation preserves this as a
separate operation. Invalid/non-finite input and overflow leave output unchanged;
aliased output is supported. No allocation is required.

`tools/verify_tag_placement.py` compares 1,000 deterministic cases against the
unhooked original `0x5034f0` and its character dispatch/helper path. A valid
cached bone supplies the input tag matrix; animation itself is outside this
test. Identity and general supplied matrices, zero/nonzero translations and
all output matrix bits match. PC boundary/alias tests, all four PC tests, and
PC/NXDK builds pass. Actual player setup inputs, motion-slot selection and
integrated Xbox camera validation remain open.
## Shared looping phase

`rf_motion_advance_phase` reconstructs the weighted phase portion of
`0x51ba80`. Looping slots accumulate weight/duration and weight separately,
with a float spill after each addition. Their ratio times the integer tick
delta advances the instance phase. The first greatest positive looping weight
selects the dominant slot. Non-looping slots do not affect these accumulators;
zero total looping weight resets phase to zero and leaves no dominant slot.

The original compares the unspilled advanced value with one before deciding
to wrap, even though it has already stored a float copy. That distinction is
preserved. Within the supported range below 2^24, subtracting the integer part
of the stored phase gives the same result as the original repeated subtraction
of one. Larger advances are rejected. The API supports forward updates and up
to 16 slots, rejects invalid durations, negative/non-finite weights and invalid
phases, and leaves output unchanged on error. It neither allocates memory nor
updates motion cursors or events.

`tools/verify_motion_phase.py` executes original `0x51ba80` through `0x51bc2c`
using synthetic loaded motion descriptors. All 1,600 comparisons match phase
bits, dominant-slot index and wrap flag, covering mixed loop/non-loop slots,
unequal durations, equal-weight ties and wrapped/unwrapped updates. These
direct-original cases have a positive looping contribution; PC tests also
cover no looping contribution and invalid input. PC/NXDK builds and all four
PC tests pass.

The subsequent cursor mapping calls `0x573e83` before integer conversion. Its
helper `0x579704` uses `frndint` under a control-word setup, so its rounding
must be established rather than assumed from the decompiler. Cursor mapping,
dominant-motion event crossings, non-loop end behavior and inactive-slot
removal remain to complete playback update reconstruction.
## Loop cursor mapping and event markers

`rf_motion_map_loop` reconstructs `0x51bc78` through `0x51bd1b`: multiply the
stored phase by end-minus-start in double precision, floor it, and add the
start tick. The original `0x573e83`/`0x579704` path was executed directly to
establish floor behavior; it is not nearest rounding or truncation of negative
values. The supported phase range is [0,1], accommodating a phase rounded to
one before the original wrap comparison.

Ordinary marker crossing is `previous < marker <= new`. On wrap, the original
also fires when `previous < marker` or `marker < new`; the latter boundary is
strict. The helper returns a two-bit mask. The update caller must apply it only
for the dominant looping slot and OR it into the existing event flags, which
the original block does not clear. Marker positions are accepted as supplied;
this helper does not interpret or load their meanings from motion descriptors.

`tools/verify_motion_loop.py` executes the original cursor/event block and its
CRT floor helper without hooks for 3,000 deterministic cases. Cursor integers
and both event flags match exactly, including phase 0/0.5/1, negative start
ticks, wrap/no-wrap and markers equal to previous/new ticks. The fixture sets a
dominant active looping slot; the rest of the update routine is outside this
comparison. PC boundary tests, PC/NXDK builds and all four PC tests pass.
Non-loop completion, frozen-slot handling, inactive-slot removal and continuous
playback integration remain open.
## Active animation-slot removal

`rf_motion_remove_slot` reconstructs original `0x51c090` with a portable
16-slot state. It removes the first matching motion ID, decrements that motion's
reference count with a zero floor (`0x539d70`), and shifts later slots left in
order. It preserves the unused tail just as the original does. Each of the
freeze, primary and dominant selected indices becomes -1 if it referred to
the removed slot, or decreases by one if it referred to a later slot. An
absent motion leaves both state and reference count unchanged.

The caller owns the reference count for the requested motion and must provide
storage separate from the slot state. This helper does not load or unload
assets. It validates counts, active motion IDs and selected-index ranges
before mutation. Slot weight bits and tick cursors are copied without
reinterpretation; removing a slot does not itself choose a replacement
primary motion or unfreeze an instance.

`tools/verify_motion_slots.py` executes unhooked original `0x51c090` and
reference decrement for 2,040 cases spanning counts 0..16, duplicate/absent
motion IDs, selected-index combinations and zero/nonzero reference counts.
All stored slot records, count, selected indices and reference count match.
PC tests cover unchanged state on invalid input; PC/NXDK builds and all four
PC tests pass. Non-loop completion, frozen-instance state and integration of
the complete playback update remain open.
## Non-loop completion pass

`rf_motion_complete_slots` reconstructs the pass at `0x51be6a` before inactive
slot compaction. It visits active slots in order, ignores looping slots, and
tests cursor >= end tick even when the slot already has zero weight. A
completed freeze-designated slot clamps to the end and sets the instance's
frozen flag without clearing its weight. Other completed slots clamp to the
end and clear their weights. Freezing does not stop this pass from processing
later slots.

If a cleared slot was primary, the pass resets that selection to -1 and clears
the original flag at +0x1d14, words at +0x1d18/+0x1d1c, and vectors at
+0x1d20/+0x1d2c. The portable state retains those fields without assigning
unverified gameplay semantics to them. It leaves freeze/dominant selection
unchanged; later compaction repairs those indices. The helper validates state
before mutation and allocates no memory. It does not perform the full update's
early exit for an already frozen instance, advance cursors, or release motions.

`tools/verify_motion_completion.py` compares against the unhooked original
pass through `0x51bf57`, including original vector-clearing helpers, for 1,360
cases spanning 0..16 slots. All slots, selected indices, frozen flag and
primary auxiliary fields match. Cases include end-1/end/end+1 cursors,
loop/non-loop flags, zero/nonzero weights and existing frozen state. PC/NXDK
builds and all four PC tests pass. Consecutive-frame verification of the
complete update, primary selection during advancement, and integration with
asset ownership and skeleton evaluation remain open.
## Primary selection during mixed-loop updates

`rf_motion_advance_candidate` reconstructs the non-loop candidate block at
`0x51bd20` inside the positive-looping-contribution branch. A nonzero-weight
slot advances its cursor by the integer delta. If there is no primary slot,
it becomes primary and clears the two state words at +0x1d18/+0x1d1c.
Otherwise the candidate's comparison-bone envelope rounds to float while the
existing primary's envelope retains higher precision for the comparison.
Only a strictly larger candidate replaces the primary. These comparisons do
not multiply by active-slot blend weights. The public envelope sampler still
returns float; its shared internal calculation now supplies the higher
precision result needed here.

Original instruction tracing confirms an asymmetric flag lookup: candidate
bypass uses descriptor flags indexed by the active slot index, while primary
bypass uses the primary motion ID. The integration caller must preserve that
distinction, and supply envelopes for the descriptor-selected comparison bone
at +0x1a54. Selecting a primary clears only the two words; it does not clear
the flag/vectors reset by the later completion pass. The helper supports
forward deltas and commits its copied state only on success.

`tools/verify_motion_primary.py` executes the unhooked original block and
envelope callees for 3,000 cases with active index 2 mapping to motion 9 and
primary index 0 mapping to motion 4. It also covers missing/self primary,
zero active weights, distinct bypass flags and varied fade envelopes. Entire
slot/auxiliary state matches. This verifies the block in isolation, not the
full update, and does not apply to the branch with no looping contribution.

After sharing the envelope calculation, all 575,460 direct-original envelope
comparisons still pass. PC/NXDK builds and all four PC tests also pass.

## Complete forward playback update

`rf_motion_update` now assembles the stages of `0x51ba80`: early exits, 16-bit
generation increment, shared phase/dominant selection, cursor/event updates,
mixed-loop primary selection, completion/freezing and zero-weight removal.
The no-loop-contribution branch advances non-loop cursors without selecting
a primary. Events are sticky bits; the owner must clear them when consumed.
The resource array is indexed by registered motion ID and carries comparison
bone envelopes, loop flags, two markers and reference counts. The mixed-loop
candidate lookup retains the original active-index bypass asymmetry.

State is copied before update, and resource decrements commit only after all
stages succeed. The supported domain is forward elapsed time, at most 16
unique active IDs, positive signed-32-bit durations and finite nonnegative
slot weights. No heap allocation or whole resource-array copy is required.
Original early returns for frozen/empty instances or no registered resources
remain no-ops without inspecting unused playback inputs.

Consecutive testing exposed an x87 rounding edge in `0x51bbde..0x51bc01`:
rate bits `0x3bb34203`, total 3.75, delta 9600 and old phase `0x3d5fdcc0`
produce phase `0x3d726100`; double arithmetic produces `0x3d726000`.
An exact rational calculation also chooses the latter because the original
extended-precision division introduces a tiny positive rounding residual.
A small isolated x87 helper preserves that instruction sequence on both
supported x86 targets. It temporarily selects extended/nearest precision
and restores the caller's control word; no modern CPU extension is needed.
The remaining state machine is C. MSVC Win32 and NXDK compile this path;
unsupported architectures currently produce an explicit build error.

`tools/verify_motion_playback.py` executes the entire original function and
all callees without replacements for 160 synthetic scenarios, 64 consecutive
calls each. It compares every mapped state field, including stale slot tails,
auxiliary vectors, generation, marker flags and all 32 reference counts.
All 10,240 comparisons pass; the report separately counts advancing calls
and frozen/empty early returns. The earlier 1,600 phase cases still pass.
Error checks cover rollback after a later cursor overflows, duplicate IDs,
invalid elapsed time and early exits with unused invalid inputs.

This is playback-state verification, not rendered animation or a complete
camera. Archive/skeleton integration, player initialization, slot insertion,
transition/root-motion behavior and Xbox runtime verification remain open.

## Playback-driven skeleton sampling

`rf_model_sample_playback` now consumes the current active slots and registered
archive handles. For each bone it obtains track envelopes, computes contributing
weights, samples the contributing poses, blends them in slot order and composes
the result with its evaluated parent. Empty/no-contribution bones use identity,
matching `0x51b500`; roots retain the original addition of zero displacement.
The operation allocates nothing and uses fixed arrays for at most 256 bones
and 16 active contributions. Archive metadata is still reread, so a cache is
required before this becomes an efficient frame-time path.

`rf_motion_bone_weights` reconstructs the selection preceding `0x51b110`.
An active primary slot supplies its time-faded envelope for the current bone,
regardless of its looping flag. The looping attenuation is the float result
of `(10 - primary_envelope) * 0.10000000149011612`, using constants at
`0x58957c` and `0x5893c4`. Without an active primary, attenuation is exactly
one. Each non-loop contribution is its faded envelope times active weight;
loop contributions use the bypassed envelope times active weight times
attenuation. Only positive contributions enter the blend. The total sums
unspilled contributions with a float store after each addition; stored float
weights are then divided by that total. Suppression is therefore per bone,
not one global blend factor or a multiplication of final matrices.

`tools/verify_playback_skeleton.py` runs original `0x51ba80`, `0x51b500` and
`0x51b2e0` in sequence, without replacing callees. The C probe updates its
playback state, samples archives and composes the miner's eye attachment.
All 160 synthetic slot configurations match complete playback state, 4,000
bone matrices and 160 eye transforms bit for bit. Inputs include zero through
three active slots, reordered registered IDs, stand/crouch motions, primary
selection, mixed looping flags and end handling. This checks individual
update-plus-evaluation cases; the separate playback verifier covers consecutive
state updates. Unit checks cover 16-way normalization, primary suppression at
and above envelope weight ten, empty states and error rollback. The previous
single-motion check still matches 300 matrices and 12 eye transforms.

PC tests and NXDK compilation pass. This path still lacks bone overrides,
root displacement, generation caching, real player setup and an Xbox runtime
animation check. No rendered diagnostic or gameplay camera behavior changes
in this step, and no visual-parity claim follows from these matrix comparisons.

## Pending root displacement consumption

The playback skeleton API now accepts a mutable three-float pending displacement,
corresponding to instance `+0x12c0`. Original `0x51b8c7..0x51b924` obtains the
root's sampled translation, adds displacement first and translation second,
writes that translation back, then assigns positive zero to all three pending
components. Subsequent roots therefore receive zero; descendants inherit the
translated root through normal parent composition. This applies even with no
active motions. A displacement is not a persistent model/world position.

The evaluator rejects non-finite displacement before reading archives and checks
addition overflow before consuming it. As with existing partial matrix output,
a later archive failure can leave an already-evaluated root's displacement
consumed. Callers must not alias displacement with output matrices.

The integrated original-code comparison now covers 320 cases, 8,000 matrices,
320 eye transforms and the remaining displacement bytes. Inputs retain the
zero-displacement baseline and add positive/negative offsets and signed zeros.
Some cases make miner bone 8 an additional root in both implementations and
recompute depth order, explicitly testing consume-once semantics. All fields
match. This synthetic topology is a test fixture, not a change to game assets.

This recovers the application of pending displacement, not its producers,
root-motion extraction, generation-cache behavior or bone overrides. Actual
player setup and an Xbox runtime check remain required before camera use.

## Per-bone evaluation generations

`rf_model_evaluate_playback` adds the generation comparison from `0x51b500`:
each bone's unsigned 16-bit stamp is compared with instance generation
`+0x1cf8`, and a matching stamp skips evaluation. A successfully composed bone
receives the current generation, corresponding to the original stamp at
`instance + 0x1394 + bone * 48`. The existing uncached sampling API remains
available for explicit resampling. Both paths share the same implementation.

The owner supplies stamps alongside its matrices (at most 512 bytes for 256
bones), initializing stamps to a value different from the current generation.
A cache hit makes no archive reads and does not consume pending displacement.
Generation changes cause reevaluation; the first root actually evaluated
consumes displacement. If an earlier root is cached, a later uncached root
consumes it instead. Stamps commit individually after successful composition,
so partial read failures preserve completed work just like partial matrices.

Stamp equality is authoritative: invalidating a parent alone does not implicitly
invalidate descendants. The caller must maintain the intended generation/state
relationship. This preserves original behavior and avoids silently introducing
a different dependency-invalidating policy. Generation is validated as 16-bit;
the playback updater already performs its wrap.

`verify_playback_skeleton.py --cache` runs 320 initial update/evaluation cases
plus three queries per case: same generation with queued displacement, changed
generation, then only root zero invalidated with a different displacement.
All 32,000 compared bone matrices, 1,280 eye transforms and 960 query records
including stamps and remaining displacement match the unhooked original.
The ordinary uncached comparison remains available. Unit checks additionally
cover cached-first/stale-second roots, invalid generation and wrap to zero.

This is an evaluation cache, not the still-needed archive key/metadata cache.
Bone overrides and generation invalidation by the original initialization and
control paths remain to recover; Xbox runtime integration remains open.

## Loaded motion insertion and control

`rf_motion_set_weight` reconstructs `0x51c190`, while `rf_motion_start`
reconstructs `0x51c1c0`. They share the loaded-descriptor portion of insertion
at `0x51bfd0`: find and reuse an existing motion slot first; otherwise append
a zero-tick/zero-weight slot if fewer than 16 exist, and increment its motion
reference count (`0x539d60`). Reusing a slot never adds another reference.

Weight assignment clears the frozen flag and writes weight without resetting
the cursor. Restart acts only for positive weight and a loop byte not exactly
one. That exact comparison is distinct from the update evaluator's nonzero
loop test. Restart clears frozen, writes weight, resets the cursor to the file's
start tick, and sets the freeze slot only when the supplied byte equals one.
If no primary exists, it selects this slot and clears only `+0x1d18`; the
adjacent word, flag and auxiliary vectors remain untouched. Neither control
call resets shared phase or increments the evaluation generation.

The caller must open/register resources first. The original lazy `0x53a980`
load path and its partially inserted slot on failure are not reproduced here.
Invalid IDs/state, non-finite weights, negative assignment weights and reference
overflow are rejected before mutation; a full table returns `RF_RANGE` for a
new motion but still permits an existing motion to be controlled.

`tools/verify_motion_control.py` compares 3,200 complete original control calls
and their loaded callees, including 0..16 slots, reused/new IDs, frozen states,
full tables, primary/freeze selections and loop bytes 0/1/2/255. All mapped
state and all 32 reference counts match. Unit checks cover error rollback,
reference overflow, cursor retention and the single-word primary reset.

The shared 64-frame runtime diagnostic now uses recovered weight assignment
for insertion. Its original-code oracle executes `0x51c190` too; all four
existing animation hashes remain unchanged. Stock 64 MiB XEMU passes in
`artifacts/xemu/20260908-161400-294874/report.json`, using `--no-capture` so no
framebuffer is acquired for this nonvisual change. Resource registration and
actual player control-call sequences remain open.

## Character reset and deferred stop controls

Instruction tracing confirms that `0x503390` forwards character type 2 through
`0x501af0` to recovered weight assignment `0x51c190`. The reset wrapper
`0x5033f0 -> 0x501ca0 -> 0x51c340` does not remove all motions. It clears the
freeze designation and frozen flag, then sets weights to zero only for motion
descriptors whose loop byte equals one. Primary, phase, generation, cursors,
slot count, auxiliary state and reference counts remain unchanged.

`rf_motion_stop_looping` implements that behavior. The companion
`rf_motion_stop_nonlooping` (`0x51c390`) targets exact byte zero and additionally
clears primary. `rf_motion_stop_slot` (`0x51c3f0`) zeroes the first matching
motion's weight and clears primary even when primary refers to a different
slot. An absent motion is a complete no-op. It does not unfreeze or clear the
freeze designation. None of these calls clear the primary auxiliary words or
vectors, increment generation or immediately decrement references.

`tools/verify_motion_stop.py` executes all three original functions for 1,800
cases covering empty/full tables, reordered IDs, absent targets, primary/freeze
selections and loop bytes 0/1/2/255. Every mapped state field and reference
count matches. Unit checks exercise stop-looping, activate another loop, then
update: zero-weight slots and references survive the stop and are removed by
the subsequent update. Invalid resource bounds leave the complete state intact.

This resolves the reset used by `0x423bd0` before switching from standing
animation `entity+0x8e4` to crouching `entity+0x964`, advancing by 0.2 seconds,
sampling the eye, and switching back. Because reset preserves shared phase,
the whole sequence must be verified rather than treating each pose as an
independent animation at a chosen tick. The standing eye is initially queried
before that explicit update; initial-pose sampling and actual registered IDs
remain to validate. The renderer and gameplay camera are unchanged.

## Standing/crouching sequence verification

`tools/verify_eye_setup.py` now drives original character type-2 wrappers
`0x503390`, `0x5033f0` and `0x503360`, followed by original skeleton and eye-tag
evaluation. The C probe performs the corresponding recovered operations. Eight
sequences vary initial shared phase (0, 0.1, 0.5, 0.9) and whether a 1/30-second
update precedes the first standing query. It then stops looping weights, selects
crouch, updates by 0.2 seconds, queries the eye, and repeats with standing.
All 600 bone matrices, 24 eye transforms, mapped playback states and remaining
root displacement match. This verifies tick-zero standing sampling for these
installed miner tracks as well as phase carry and wraparound; it does not
generalize the original single-key out-of-bounds behavior to other assets.

The final standing query is a follow-up verification step: `0x423bd0` restores
standing and updates before returning, without another explicit eye query there.
The test omits the surrounding collision/physics setup and entity-specific
XZ suppression, and supplies loaded miner stand/crouch resources. Those limits
remain distinct from the verified sequence of pose operations.

Additional creation-path tracing establishes:

- `0x422fc3..0x422fd5` registers the named looping-state motions with loop byte
  one through `0x51cc10`; this is the flag used by the sequence fixture.
- `0x4231d4..0x4231ea` initializes current state `entity+0x138c` to zero,
  next state `+0x1390` to -1 and transition duration/elapsed to zero.
- `0x4231f0` calls controller `0x41f270`, then `0x423208` updates by the float
  at `0x5a4014`: 0.03333333507180214 (1/30 second).
- `0x423b90` calls eye/physics setup `0x423bd0` only if class flag
  `+0x724 & 0x40000000` is clear. It sets that flag afterward.

Ghidra now exports `0x41f270`: it gates on entity flags, calls locomotion
selection or a scripted-animation path, then resets looping weights and assigns
the current state or a timed current/next blend. Therefore the creation-path
update is not sufficient proof that the initial motion is standing. Its
`0x41f400` selector and actual player flags must be resolved before applying a
specific offset to gameplay.

For the verified fixture with phase zero and standing already selected before
the 1/30-second update, the model-space eye results are:

| Query | Cursor | Eye X | Eye Y | Eye Z |
| --- | ---: | ---: | ---: | ---: |
| Standing | 320 | 0.003930965 | 0.785402536 | 0.077420831 |
| Crouching | 1279 | 0.060915217 | 0.149211273 | 0.382877767 |
| Restored standing follow-up | 2240 | 0.003969240 | 0.786622703 | 0.074905924 |

These are conditional model-space results, not world camera heights. With no
preceding update, the first crouching cursor is 1119, rather than an independently
selected 1120, because the original phase-to-tick path floors its float-derived
product. Evidence is in `artifacts/eye-setup-verification.json`. No camera offset
or rendered output is changed by this verification work.
# Animation controller after state selection

`rf_motion_apply_controller` reconstructs original instructions
0x41f2b6..0x41f3f3. This starts after the locomotion/alternate selector returns;
it excludes entity gates and state selection itself. The 23-entry mapping
comes from entity +8e4 with original stride 16. Current/next logical states
are +138c/+1390, duration/elapsed +1394/+1398, override state +1384, and
override flag bit 0x20 at +810.

Positive duration advances elapsed using global frame time +5a4014. The
completion comparison uses the unspilled sum, even though elapsed has already
been stored as float. Completion sets duration to positive zero, promotes next
to current and clears next to -1; it retains elapsed, including overshoot.
This transition runs even when an override is active. Every invocation stops
looping weights through 0x5033f0 before assigning weights through 0x503390.
An override assigns its mapped motion weight one. Otherwise duration zero
assigns current weight one; a transition with both motions present assigns
current `1-f` then next `f`, where f is the float-rounded elapsed/duration.
If either transition motion is missing, neither is assigned. Aliased motion
IDs therefore receive the second weight, rather than the sum.

The C API preserves controller, playback and resource references on malformed
input or insertion failure. This is a bounded safety behavior, not a claim
about original invalid memory access. No heap allocation is needed. Playback
cursors and generation advance separately through `rf_motion_update`.

`tools/verify_motion_controller.py` matches 2,406 executions of the original
post-selector block with unmodified character/control callees, comparing all
controller/playback fields and 32 reference counts. Six targeted cases cover
float-rounded elapsed reaching duration before the unspilled sum does, plus
exact/overshoot completion, with and without override. Three C-only rejection
cases verify rollback, negative frame time and an invalid mapping. PC and
NXDK builds pass; controller runtime integration and selector recovery remain.
# Logical state requests

`rf_motion_request_state` reconstructs 0x42a580. Requests outside [0,22] or
mapped to -1 fall back to logical state zero; if zero also maps to -1, the
request changes nothing. With zero existing duration it sets next, copies the
requested duration and resets elapsed to positive zero. It does not immediately
promote next even if the new duration is zero.

During a transition, it divides elapsed by duration without a float spill.
Above the binary32 constant 0.5 at 0x5893c0, it promotes the previous next state
to current and uses one minus that fraction. At or below half it retains the
current state and uses the fraction itself. That fraction times the newly
requested duration becomes elapsed, preserving progress from the nearer
endpoint. It always writes the requested next state and duration, including
repeated requests; suppression belongs to the caller.

`rf_motion_has_state` reconstructs 0x42a650: current OR next equality, independent
of duration. A null controller returns false. Request -1 can match next -1.
These functions allocate nothing and do not modify playback slots. The safe
request API accepts finite nonnegative durations and progress within an active
transition; malformed input preserves the controller.

`tools/verify_motion_request.py` compares both complete original functions
against C over 10,000 cases, checking all controller fields and membership.
It includes unavailable and out-of-range requests, repeated requests, zero
durations, midpoint neighbors and varied duration magnitudes. Four additional
C-only rejection cases verify unchanged state. This does not recover the
locomotion predicates feeding these requests.
# Controller-to-eye runtime integration

The shared PC/Xbox animation diagnostic now drives the recovered state request
and post-selector controller before each playback/skeleton/eye evaluation.
Scripted stand/crouch transitions include interruption and a temporary standing
override during a crouch transition. Original instruction execution and a
64 MiB XEMU run match all four sequence hashes; see docs/VALIDATION.md and
artifacts/xemu/20260908-163832-540225/report.json. No screenshot was acquired.

Locomotion selector 0x41f400 remains unrecovered. Its inspected paths reach
0x429ae0 (entity/AI decisions), 0x428a60 (collision query and subsequent effects),
0x4289d0 (entity flag and physics updates), and other predicates. Scripted
requests in the diagnostic do not stand in for these gameplay decisions.
# Movement selector tail

`rf_motion_select_movement` reconstructs 0x41f7c1..0x41f94f, beginning after
the optional collision/physics routine 0x428a60 returns. The vector compared
with zero is entity +714, confirmed by `lea edi,[esi+714]` at 0x41f7cc.
0x416270 and 0x4162b0 implement componentwise equality and inequality, not
directional dot products. Finite signed zero components compare equal to zero;
even a tiny nonzero component takes the moving branch.

The mode is `*(entity+858)+4`. Modes 4 and 7 (predicate 0x42a0a0) choose
logical 18 when the vector is zero and 19 otherwise. Other modes choose the
upstream idle candidate for a zero vector. With nonzero movement, mode 1 and
entity +8c4 equal zero choose the upstream movement candidate (0x429fc0).
Mode 1 with +8c4 equal one (0x429ff0), or modes 12,15,13,11,9 (0x42a060),
choose the alternate candidate. Other combinations leave the controller alone.
These mode/direction field names describe inputs without claiming their full
gameplay meanings are resolved.

Every chosen state is tested against current AND next through 0x42a650 before
requesting it with duration .25 through 0x42a580. The candidates arrive in
EBX, stack +0c and reused argument stack +20. Their upstream weapon/entity
selection and the priority/physics/AI branches remain unrecovered.

`tools/verify_motion_movement.py` compares all controller fields across 6,000
executions of the original tail and unmodified callees. Tests vary vector axes,
signed zeros, tiny values, modes, direction values, candidates, mappings and
transition progress; two C-only cases check malformed inputs preserve state.
The API is compiled for both PC and Xbox but not wired into the runtime script.
# Animation priority prefix

`rf_motion_select_priority` reconstructs 0x41f400..0x41f5ad, returning a handled
flag so the caller can continue into the unrecovered middle selection block.
Priority order is forced state (entity +834 != -1), flag +810 bit 0x02000000
(logical 22), linked classification four (15), linked first-occupant match
(20), mode 3/8 or classification one with +1380 == -1 (14), then the velocity
branch when +1a8 bit 0x8000 and +810 bit 0x400 are both set (9/10).
All except the velocity branch suppress requests matching current or next.
Missing mapped motions still count as handled, using the request setter's
standing fallback. Priority does not mean a request necessarily changed state.

The linked predicates resolve entity +200 through 0x426fc0 and 0x40a0e0: the
low 16 handle bits must be below 1024, the pointer table at 0x7394cc must have
an entry, its +2c full handle must match, and entity type +24 must be zero.
Classification comes from 0x486c90; for type zero it reads info +1b4 through
entity +294. Occupancy requires info +724 bit 0x00400000, then compares the
first seat's +4 handle with the selecting entity +2c. The C priority input
uses already-resolved linked fields; a shared entity registry remains work.

Both 0x41f4b1 and 0x41f4e8 call 0x42ac80, the first-occupant predicate. The
logical 21 branch is unreachable for stable inputs. The distinct second-seat
predicate 0x42acd0 exists but is not called here; it was not substituted.

The velocity source is entity +144; its x87 magnitude at 0x40a000 is compared
with binary32 0.01 at 0x589568, without spilling to float or double. An initial
double rewrite incorrectly selected 9 for (.01f,1e-11f,0), while original x87
selected 10. The shared x86 helper now preserves original extended precision,
sum order and square root, restoring the caller's control word afterward.

`tools/verify_motion_priority.py` matches 5,010 original executions using
unmodified handle lookup, linked/movement predicates and state controls. An
observation hook stops at 0x41f5ae for fallthrough; no callee is replaced.
Tests compare all controller fields and handled outcomes. This does not verify
the later middle candidate/physics/AI block or actual player initialization.
# Candidate helper dependencies and action activity

Targeted Ghidra exports now include 0x41f950, 0x41f9f0, 0x429ae0,
0x428d10, 0x408dc0, 0x408e90 and 0x48aaf0. Raw exports remain local evidence.
The middle selector is not merely a candidate table: before candidate selection
it can update entity +744 through timer setter 0x4fa360 and call 0x41ae70.
The candidate/turn helper 0x41f9f0 can start actions 17..20 via 0x428c90,
write entity +7bc, update five timers and invoke 0x427450, which itself modifies
movement-related fields. These effects must be recovered before treating the
whole selector as implemented.

Direct candidate rules observed at 0x41f61d..0x41f729:

- Entity +520 equal 17: idle 0, movement/alternate 7, special 8; missing logical
  motion 7 changes movement and alternate to 4.
- +520 equal 7, or equal 12 with +554 equal 1: idle 13, movement/alternate 6,
  special 8; missing motion 6 changes only alternate to 4.
- Ordinary fallback: idle 0, movement 2, alternate 4, special 8.
- The conditional 0x41f950/0x427020/0x428e60 path obtains movement/alternate
  from 0x41f9f0, starts with idle 1 and special 9, then can replace idle with
  alternate based on +144 velocity, +8c0 times binary32 .3, +740 and a global.

`rf_motion_remaining` reconstructs loaded type-two 0x51c270. It finds the first
matching active slot and returns max(file end tick minus cursor,0), multiplied
by binary32 1/160 (0x589e18) and binary32 1/30 (0x5898e4), with no intermediate
float spill. An absent slot returns positive zero. Weight, looping and frozen
state do not affect this query. The safe C API rejects signed subtraction
overflow rather than emulating original wrap for malformed timelines.

`rf_motion_action_active` reconstructs 0x428d10 for a type-two character. The
45-entry action mapping is entity +a54 with stride 16. Invalid action indices
or negative mapped IDs return false. Otherwise it calls remaining-time query
through 0x5033d0/0x501bd0 and tests for nonzero. In particular, a deferred
zero-weight stop does not necessarily make an action inactive until update
removes the slot or the cursor reaches the end. This is the query used by
0x41f9f0 for turn-action decisions, not a test of slot weight.

`tools/verify_motion_action.py` matches 7,000 complete original action queries
and remaining-time calls with unmodified callees, covering zero weights,
loop/freeze flags, missing slots, invalid action indices, cursor boundaries
and 2,000 positive end ticks across the int32 range. Original entity/model RAM
is checked unchanged. Other character types and the candidate helper's side
effects remain open; no runtime visual behavior changes in this step.
# Entity action starts

`rf_motion_start_action` reconstructs 0x428c90 up to sound-class dispatch for
loaded type-two characters. Actions outside [0,44] or with negative mapped
motion IDs are no-ops. Valid actions map through entity +a54 (stride 16),
forward weight and freeze to 0x5033b0/0x501b50/0x51c1c0, then inspect the sound
flag's low byte. Exactly one requests the sound class at entity +a5c for the
same action; other values do not request sound. The C API returns that class
for its caller to resolve and play, or -1 when no class was requested.

Sound dispatch is independent of whether the restart actually changed a slot.
Weight zero and loop byte exactly one can suppress restart while still requesting
sound. Missing action mappings suppress both. The C layer preserves output and
playback when bounded restart validation fails.

Original 0x434da0 resolves a class from the table at 0x6300f8 with 44-byte
stride, validates against count 0x636ef8, and for multiple entries selects using
0x57312d modulo entry count. A resolved sound other than -1 goes to 0x5056a0
with entity position +3c, volume 1, pointer 0x173c378 and final zero argument.
This class selection and audio playback are not implemented by the new API.

`tools/verify_motion_action_start.py` matches 6,000 original 0x428c90 executions
through unmodified loaded-control callees, comparing all playback fields,
32 reference counts and the requested sound class. An observation hook stops
at sound resolver entry; it neither replaces a callee nor claims audio output
verification. Cases include zero/positive weights, loop flags 0/1/2/255,
freeze/sound flag low-byte behavior, missing mappings and invalid action IDs.
The candidate/turn helper can now use the reconstructed action-start interface;
its remaining timer, movement and sound side effects are still open.
# Turn-helper timer dependency

The shared timer module now reconstructs clock advance, nested pause/resume,
deadline setup/clear and expiration/remaining queries. Original-code testing
matches 9,800 cases. The five turn-helper 1,200 ms deadlines are entity +79c,
+4d0, +4d4, +744 and +798. See docs/TIMERS.md for addresses, wrap behavior and
validation limits. The helper's movement effects and integration remain open.
# Turn-helper movement settings

Routine 0x427450 is now reconstructed as `rf_movement_set_mode`, matching
6,000 original executions. It updates animation-selection inputs +8c0/+8c4
and conditionally entity +8c. Forced-action and global-override behavior are
preserved; docs/MOVEMENT.md records field mappings and unresolved semantics.
The candidate/turn helper still needs its combined effects and decision logic
integrated before the full selector can drive the gameplay camera.
# Integrated selected-turn effects

The shared turn module now composes action start, candidate assignment, turning
flag, five deadlines and movement setting for 0x41fbdc..0x41fc83, matching 2,400
original executions. See docs/TURN.md for scope and the deferred audio boundary.
The preceding 0x41ae70 call resets entry state and reaches additional subsystems;
it is not established as an aim-refresh routine. The full helper's decisions
and reset effects still need recovery before gameplay-camera integration.
# Turn direction gate

The direction gate and local-vector calculation from 0x41fa7c now match 5,208
original executions. It preserves the x87 dot-product boundary and transforms
the unnormalized vector after checking a normalized copy. See docs/TURN.md;
remaining candidate/reset decisions and gameplay integration are still open.
# Remaining turn-candidate branches

The remaining branches starting at 0x41fc84 now compose movement settings,
remaining-time activity checks and conditional action starts in shared C.
They match 4,800 original executions, including unchanged deadlines/turn flag
and fallback candidates 2/4 versus 3/5. See docs/TURN.md. Preceding reset and
decision logic, valid audio and complete gameplay integration remain open.
# Assembled candidate helper

`rf_turn_update` now combines the helper's early exits, direction gate, required
reset callback, post-reset target-distance test and both effect branches.
It matches 3,004 complete original executions with absent weapon entries and
sound classes, covering all six outcomes. See docs/TURN.md for adapter boundaries;
populated reset/audio behavior and actual player initialization remain open.

The latest shared diagnostic includes empty-ammo selection-tail execution:
one queued replacement and followup clear, followed by duplicate suppression.
State hash d5f86d40 matches original instructions, PC and stock 64 MiB XEMU;
pose/cache/eye hashes remain dc7c08a6/21cd6b06/60a29326. This is still a scripted
rig with a fixed current weapon, not initialized gameplay or the final camera.
Earlier selection gates, local transition/presentation and actual activation
remain open. Numeric-only report: artifacts/xemu/20260908-183800-793148/report.json.


## First-person pose transfer before effects

`rf_first_person_pose_copy` reconstructs 40d88c..40d8be. The resolved player eye
at +7d4 becomes camera entity position +3c; player body orientation +48 becomes
camera body orientation +48; player eye orientation +7e0 becomes camera eye
orientation +7e0. These are distinct matrices, not a single inferred orientation.
The portable output retains all three fields (84 bytes). It does not derive
an eye height or substitute the diagnostic follow-camera offset.

`tools/verify_first_person_pose.py` executes this block with original 409f40 and
40a3b0 callees intact, stopping before 40db70. All 258 prepared finite cases match
PC and compiled NXDK, including distinct matrices and signed zero. NXDK also
checks in-place output aliasing, for 516 compiled cases. The portable finite-input
guard rejects NaN without modifying output. Existing eye verification, both
builds and four CTests pass. This is an original instruction comparison, not an
XEMU gameplay camera run; the scene view remains the diagnostic follow view.

Inspection of the next helper, 40db70, shows a resolved-player lookup, a timer
at player +8bc, a scalar at +8b4 and modification of camera eye orientation via
4fae00 followed by 4fc960. It is not another eye-position copy. Timer helpers
are already reconstructed, but this camera effect path, its initialization and
random/orientation operations remain uncomposed. The subsequent 48a190 entity
commit also has state-dependent behavior and is not replaced by this pose helper.
Player handle resolution, the actual player eye source, effects/commit and the
first-person rendering binding are the next required integration work.

## Post-camera room refresh (48a190)

`rf_entity_room_refresh` now reconstructs the complete room-refresh control flow
using caller-owned room tokens and explicit locator/notification callbacks.
The earlier shorthand "entity commit" for this call was too broad: it does not
copy orientation or physics state. It refreshes object +0 room and +4 previous
query position, then clears object +7c bit 04000000.

If there is no room, or the current +3c position differs, 48a190 calls 4cd970
with `(0, position, position, 0)`. That wrapper takes its null-room branch into
4e1630. A null result preserves the old room and previous query position. A
successful result copies both even if the room token is unchanged. Only a
changed room for the local player's entity invokes the notification boundary,
before committing those fields. 4ce080 tests the room liquid byte +184 and
`minimum_y (+c) + liquid_depth (+188) >= position.y`; it selects the literal
"underwater", otherwise room name +4a. 5231e0 conditionally forwards that text
through 5230b0/527d90; its downstream subsystem remains unidentified.

`tools/verify_entity_room_refresh.py` executes the unmodified 48a190, its original
vector/squared-distance helpers, and 4ce080. Lookup and notification are fixture
boundaries. 384 PC cases and 384 compiled NXDK executions match room, query
position, flags, query count and notification selection. Cases include absent,
unchanged, changed and missed rooms, nonlocal entities, water boundaries and
subnormal movement. Both builds and four CTests pass. This is not an XEMU-bound
camera/entity membership implementation yet.

The containing-room routine 4e1630 is now exported for continued reconstruction.
It traverses solid +9c rooms and room face trees/lists, uses 4e3780/4e3800 query
state, and retries a direction up to the counter threshold 16. It derives a
query length from solid bounds and finally inspects the selected face and
4e3a70 classification before returning face +44 owner. The Ghidra stack model
is unreliable around 5754d0; confirm the query helpers and assembly before
porting this. An AABB-only room choice would not reproduce the original.

## Containing-room face query (4e3800)

`rf_collision_room_query_face` implements the null-reference-face branch used
by 4e1630. It reuses reconstructed 508b70 bounds/segment rejection and 4e1f50
polygon containment. It preserves signed plane tolerance (-0.0001), stored
float parameter/distance, front-side priority within the 0.0001 distance band,
and the shortened query endpoint after an accepted face. A nonzero caller token
represents the selected face without retaining original executable pointers.

A polygon-contained hit with parameter greater than 0.0001 invokes the edge
proximity test (4e0850/4e08c0). This uses squared distance to an infinite edge
line, not a clamped segment. A distance below original constant 589d3c
(approximately 1e-8) returns retry without changing selected query state.
Near-zero edge length falls back to distance from its first endpoint.
Reference-face coplanarity voting is not implemented because the containing-room
caller explicitly supplies null; do not use this API for reference-face callers.
Room skip bytes and face flags are the traversal's responsibility.

`tools/verify_collision_room_face.py` runs original 4e3800 with unchanged
geometric callees and real circular edge lists. All 1,200 PC and 1,200 compiled
NXDK cases match status/retry and complete query state byte-for-byte, including
345 selected faces and 29 retry outcomes. The fixtures include axis-aligned
and rotated quads, boundaries and tied distances. This is evidence for those
fixtures, not an exhaustive proof of double-versus-x87 arithmetic equivalence.
Both builds and four CTests pass. The routine is not yet bound to XEMU rendering.

Remaining containing-room work: 4e0c20 direction generation, 4e1630 ordered
room/tree/face traversal and retry threshold, and final face-owner resolution.
Exported 4e3a70 returns 2 without a face; with null reference face it returns 1
for a front selected face and 2 otherwise. 4ce4a0 rejects face flags 0x0c.

## Complete containing-room traversal and deterministic retries

`rf_collision_room_direction` reconstructs 4e0c20 and the 4fcfa0 basis path.
The phase depends on the two largest absolute forward components. It is not
random. Original x87 sine/cosine and the intermediate float radius are retained;
the vertical special case uses strict +/-0.0001 comparisons, and the original
non-unit forward vector behavior is preserved. The caller may update in place.
`tools/verify_room_direction.py` matches 2,176 original sequence steps on PC and
4,352 compiled NXDK calls, including aliasing, vertical inputs and non-unit seeds.

`rf_collision_locate_room` now composes the face query and direction generator
into 4e1630's traversal. Inputs are prepared room views, the ordered primary
list (solid +9c), original solid bounds and the point. It skips room +1, ignores
face flags 0x0c, processes each node's face list before its children, and visits
the right child before the left. No detail-child expansion is done by this
routine. It also supports the original room-without-tree ordered face path.
Shared tree scratch requires serialization; the query performs no allocation.

The initial direction is derived from (0,1,0), parameter .9753. On an ambiguous
edge, the global retry count increments. Counts below 16 restart all rooms with
the next direction, decrementing the parameter by .13579 and clamping to -1.
At count 16 and later the ambiguous face is ignored and traversal continues;
this is not an unconditional failure after 16 hits. Accepted hits shorten the
query endpoint. Final selection requires a front classified face and returns
its owning room, with the caller responsible for mapping the local face index.
The output uses UINT32_MAX for an absent room/face.

`tools/verify_room_locator.py` executes complete original 4e1630 and all its
callees without replacing geometry or direction helpers. The 600 synthetic
cube cases exercise flat face lists, three-node trees, skips, flags, interior,
exterior, boundary and edge-aligned points. PC and compiled NXDK room/face/retry
results match: 296 owned points and 50 retries. These fixtures do not establish
multi-room level ownership, all retry-limit combinations or runtime integration.
Both builds and four CTests pass. Next bind this to retained loaded geometry,
map selected source face/room identities, and verify real level points before
using it for campaign entity membership. No new XEMU image for these APIs.

## Loaded-world locator binding

`rf_geometry_collision_world_locate` now binds the locator to retained primary
room views and maps its local selected-face index through the owning tree's
`source_indices`. The world stores 24 additional bytes of solid bounds, derived
from every serialized vertex (including unused vertices), with the original
0.0001 expansion. Those bytes are included in existing retained/peak budgets.
Lookup reuses tree scratch and allocates nothing. It remains valid after source
geometry closure; no original game pointers or loading-buffer pointers escape.

`rf_collision_probe --world-locate-dump` loads a world, closes source geometry,
allocates poison storage and repeats each query, checking stable output and
source-face ownership. `tools/verify_loaded_room_locator.py --all` materializes
those same real-level room trees, faces and edge loops in the original RF.exe
layout, executes complete 4e1630, and compares room/source-face/retry results.
It also materializes the independent compiled NXDK layout and executes the
new mapping wrapper. The original loader is not executed: these comparisons
validate queries over reconstructed-loader data, not original loader equivalence.
Samples are three points around one face per primary room; they do not prove
all points in each level. Moving entities and live XEMU membership are next.

Final result: all 94 installed levels pass 8,214 original/PC comparisons and
8,214 compiled NXDK wrapper executions: 6,428 owned query points and six retries.
Report: `artifacts/loaded-room-locator-verification.json`. Both builds, four
CTests and the Live Mines retained-world sweep/budget regression pass. No new
XEMU run or image is claimed for this loaded-data verification.

## First-person effect state and reset

`rf_camera_effect_reset` reconstructs the per-player slice 41d9af..41d9c7:
zero strength/duration and set the deadline to the current game time via
4fa360(0). This is an expired timer, not an inactive negative deadline. The
entity creation tail also calls 4fa360 at 423746; its full initialization
context remains a separate proof obligation.

`rf_camera_effect_step` reconstructs 40db70 through its timer, strength and
clamp calculations. Expired effects do nothing. Otherwise it stores binary32
`1 - strength` BEFORE any decay. With less than 1000ms remaining, it multiplies
stored strength by original constant 58949c (0.6002401113510132). It clamps the
pre-decay value to [-1,1] and exposes that cone cosine. Negative disabled timers
are not expired and therefore still enter the effect path; they must not be
used as the default reset representation. Random direction and orientation
rebuilding are not yet performed by this state API.

`tools/verify_camera_effect.py` matches 600 original cases on PC and 600 compiled
NXDK executions, including reset, timer wrap, disabled/expired timers, the
999/1000/1001ms boundary and out-of-range strengths before clamping. Original
40db70 timer/clamp code executes; player lookup, 4fae00 and 4fc960 are observed
fixture boundaries. 346 cases enter the active path. Both builds and four
CTests pass. No runtime camera change or new XEMU image is claimed.

Next direction-effect leads: 4fae00 builds the 4fcfa0 basis around the current
forward vector, calls 4fadb0, then places the local vector using 4facb0. 4fadb0
consumes two random draws: 504e40 selects Z between the cone cosine and 1;
504db0 supplies the azimuth fraction multiplied by 2*pi. It uses an intermediate
float radius and x87 trig. 504db0 calls 57312d and scales by 589de8. Finally
4fc960 reconstructs the eye orientation using the changed forward and prior
basis vectors, with fallback paths. These random and orientation stages remain
unreconstructed; the deterministic room-direction helper is not a substitute.

## Full first-person effect geometry

`rf_camera_effect_apply` now composes the timer/decay logic with 4fae00 cone
sampling and the complete 4fc960 orientation rebuild. The caller supplies the
two original rand outputs (0..32767); they matter only for an active effect.
The API does not silently use host rand() or claim ownership of the game's
random stream. Expired effects leave the orientation/state unchanged.

The first draw is scaled by 1/32768 and interpolates between the cone cosine
and 1, storing the resulting local Z as float. The second draw selects an
azimuth using the same scale and original binary32 2*pi constant. The radius
has a separate float store before x87 sine/cosine multiplication. The generated
vector is transformed through the original forward-derived basis, including
its strict near-vertical branch and non-unit input behavior.

Rebuilding normalizes forward, prefers the previous up vector when present,
otherwise derives up from the previous right or an axis fallback. It then
crosses up/forward to form right and forward/right to form up. There is no
extra final normalization of those cross products. This detail is intentionally
preserved rather than substituting a generic look-at matrix.

`tools/verify_camera_effect_apply.py` executes full original 40db70 with all
geometry, timer and clamp callees unchanged. Only player resolution and rand
return values are supplied at boundaries. All 1,800 PC cases and 1,800 compiled
NXDK executions match strength/deadline, all nine orientation floats and
active status byte-for-byte. 1,285 cases are active; fixtures include expired
and disabled timers, cone extremes, vertical views, non-unit bases and missing
up/right vectors. Both builds and four CTests pass. These are instruction-level
comparisons, not an XEMU camera rendering result. Initial eye offsets and game
RNG sequence ownership remain necessary before full player-view integration.

## Explicit random stream for camera effects

`rf_random_next` reconstructs original 57312d using caller-owned 32-bit state:
`state = state * 214013 + 2531011` modulo 2^32, returning bits 16..30. The
original obtains this state at CRT thread-data +14 through 577eef. The port
uses explicit state rather than NXDK/host rand(), and infers neither a seed
nor a global initialization order.

`rf_camera_effect_apply_random` consumes exactly two draws when the timer is
not expired and none otherwise, then applies the reconstructed camera effect.
It commits RNG advancement only if the port operation succeeds. Invalid inputs
preserve RNG, effect state and orientation. Runtime callers must share the
appropriate stream with other consumers from the same original thread; a new
private seed for each camera call would not reproduce game-wide behavior.

`tools/verify_camera_effect_random.py` executes full original 40db70 and the
original rand implementation. Only resolved-player lookup and CRT thread-data
access are supplied at boundaries. All 1,800 PC and 1,800 compiled NXDK cases
match orientation, effect state, active status and final RNG state exactly.
Of those, 1,285 are active and consume two draws; the other 515 consume none.
A malformed-state guard also verifies no partial RNG/camera update. Both builds
and four CTests pass. The API is ready for player ownership integration; actual
seeding and ordering of other game RNG consumers remain unrecovered. No new
XEMU camera binding or visible result is claimed.


## First selector during player creation

`tools/inspect_initial_player_motion.py` executes the complete original
`41f270`, including its `41f400` selector and loaded-character weight-control
callees, without replacing a callee. Eight creation-field fixtures compare
all 260 playback bytes, 24 controller bytes and 32 resource reference counts
against the existing PC movement/controller composition. All pass. This is
an original-instruction/PC test, not live XEMU or complete spawn execution.

The armed-player fixture starts a transition from logical state 0 (`stand`)
to 1 (`attack_stand`) lasting .25 seconds. The controller's first 1/30-second
update leaves weights .8666666746 and .1333333403 respectively. Non-player
fixtures, and an unarmed player fixture, retain standing at weight one.
Either a primary or secondary weapon satisfies the armed predicate.

The creation evidence used to choose these cases is:
- `4a3310` calls `4251c0("miner1")`; **4251c0 is class lookup**, not entity
  creation. Actual player entity creation calls `422360` at `4a41d3`.
- `4a41bf` supplies creation flags 1. The beginning of `422360` maps that
  low bit to generic object flag 8, consumed by `48aaf0` in armed selection.
- `miner1` declares movement mode `run` and default primary `12mm handgun`.
  The relevant selector mode is `(entity+858)->+4`, not generic object +1f8.
- `402c20` initializes the inventory/AI subobject at entity+2a0. The test
  executes its scalar-write span `402d68..402dac` against poisoned action
  +520 and behavior +554 and verifies both become zero.
- `422d21` assigns the default primary; `422df5` resolves movement by name;
  `422e03` installs that descriptor. `4231d4..4231ea` seeds state zero with
  no transition, then calls `41f270` before the model update.

Limits: the remaining creation fields are materialized fixtures, with no
linked entity, no script, zero steering/velocity, ordinary class and SP
flags. Motion IDs and resource envelopes are synthetic control data; no
loaded bone pose or actual default-weapon override table is sampled here.
This does not prove all intervening spawn calls preserve these fields.
The test verifies the whole controller for those conditions; it does not
implement a complete player initializer or general middle selector in C.

Do not replace the diagnostic camera with a hardcoded standing eye or the
armed blend yet. `423b90` caches eye/physics setup per class using flag
40000000, so the **first instance that initializes the class** matters.
`423bd0` also raises standing weight without first clearing other loops.
Next establish whether class preloading initializes miner1 before player
spawn, then verify the resulting loaded stand/attack/crouch eye sequence.


## Live Mines class order and integrated eye offsets

The cold single-player level-loading path in `45c540` calls `460820` before
its conditional player spawn through `4a4130`. `460820` dispatches entity
section 30000 to `464010` (call at 461006). That loader iterates records in
serialized order and supplies creation flags only from bits 2/4, never the
player bit 1. `422360` performs its first controller/model update before
calling the class-cache gate `423b90` at 42324f. The gate tests class+724 bit
40000000 before setting eye/physics data. The later loader scalar overlay and
script/state handling occur after factory return.

Reading the installed L1S1.rfl with the shared owned-entity parser gives:

| Record index | UID | Class | Creation flags |
| --- | --- | --- | --- |
| 0 | 8456 | env_guard | 0 |
| 1 | 8462 | env_guard | 0 |
| 2 | 8625 | env_guard | 0 |
| 3 | 8431 | miner1 | 0 |
| 4 | 8432 | miner1 | 0 |

Thus UID 8431 is the first serialized miner1, before ordinary player spawn;
the diagnostic UID 9858 is record 77. The earlier candidate at 41884a belongs
to the debug spawn command starting 418740, not class preloading. These are
static call-path and actual-level-record observations, not a complete original
level-load execution. Save restoration, prior class caches, recursive creation
and other levels still need their own lifecycle handling.

The shared diagnostic now retains six initial model-space eye coordinates in
`rf_scene_actor_initial_eye_offsets`. It evaluates the loaded standing and
crouching poses in the existing temporary stance workspace, applies original
class flag 20000 (zero X/Z), and commits output only on successful setup. This
adds 24 resident bytes and no new heap allocation. It does not yet implement
a game-wide per-class cache or bind the camera to a player.

An original/PC comparison caught and removed the legacy diagnostic root
translation from the standing eye calculation: simply copying the rendered
tag gave Y .535402536 instead of .785402536. The setup now independently
samples the initial pose at zero displacement before the crouch update. It
preserves the existing diagnostic body and animation behavior.

`python tools/verify_scene_eye.py` runs the original loaded-model sequence and
the integrated PC follow fixture, then compares all six float words. Passing
values are (0, .7854025363922119, 0) standing and
(0, .14921127259731293, 0) crouching. No heights are hardcoded in runtime code.

Live XEMU run `artifacts/xemu/20260909-193237-875652/report.json` PASS verifies
the same six words from guest RAM on stock 64 MiB alongside all 664 frames,
body/animation/room/world comparisons. No framebuffer captured. First-person
placement, camera collision and campaign input remain the next integration.


## First-person diagnostic binding

`--eye` in rf_scene_check, `--scene-eye-last` in rf_pc_preview, and the Xbox
`actor-eye.flag` profile bind the cached eye offsets to the current physics
body and animation controller. The scene calls shared `rf_eye_position`
(4194e0 non-linked branch), then `rf_first_person_pose_copy` (40d88c..40d8be).
The view callback now runs after initial body/class-eye setup, and takes the
current controller explicitly. The ordinary follow profile retains its prior
camera/world/body hashes after this ordering change.

The eye view follows actual body position and current/next stance blending.
Body orientation supplies look orientation until player look input is wired.
The actor is still animated/simulated but its mesh is excluded from the visible
scene in this first-person diagnostic; no weapon view model is supplied.
The attachment index retained in `rf_scene_actor_initial_eye_tag` is the LOD
attachment index (2 for this miner), used only as the nonnegative presence
indicator for this non-linked eye path. It is not a reconstructed unified
bone/attachment tag handle and must not be used for linked-eye evaluation.

PC emits every `EYE_FRAME` record as frame ID, 96-byte eye input and 84-byte
first-person pose. A 64-record ring retains the same data for guest reads,
adding 11,776 bytes plus mode/index fields. Camera/world hashes cover all
664 frames. `tools/verify_scene_eye_view.py artifacts/scene-eye-view.txt`
replays **all 664** logged inputs through unmodified original 4194e0 and
40d88c..40d8be, including 30 transition frames; every pose byte matches.

Run `artifacts/xemu/20260909-194149-709809/report.json` PASS: 64 MiB, all
existing body/animation/room checks, complete final eye ring matching PC, and
native QMP framebuffer capture. Its PC comparison has maximum channel error
1 and zero pixels over 3. Final scene has 882 world triangles, zero visible
actor triangles. Available memory after upload is 41,041,920 bytes and after
temporary CPU mesh release 44,199,936. The report's `eye_view` field specifies
camera policy; legacy `actor_follow` fields retain world-reprojection data.

World hash is 954268577, camera hash 2233034194, body hash 533762320. These
are diagnostic-route results, not campaign gameplay or whole-game memory
peaks. Local-player identity/spawn, persistent class cache, separate eye look,
effect dispatch, view weapon, camera collision and live input remain open.

Reproduce (matching actor-eye.flag in the built disc):

```powershell
python tools/xemu_smoke.py --actor-eye --reference artifacts/scene-eye-view.ppm --seconds 300
```

The PC image command is:

```powershell
./build/pc/Release/rf_pc_preview.exe --scene-eye-last Installed_Game/levels1.vpp L1S1.rfl artifacts/scene-eye-view.ppm Installed_Game/meshes.vpp Installed_Game/motions.vpp Installed_Game/tables.vpp 9858 Installed_Game/maps1.vpp Installed_Game/maps2.vpp Installed_Game/maps3.vpp Installed_Game/maps4.vpp Installed_Game/maps_en.vpp
```


## Look angle update (49de50)

`rf_look_update` reconstructs the scalar path through 49dfcd. Entity fields:
rotation command 708/70c/710, pending pitch 728 and yaw 724, body angles
864/868/86c, eye-relative angles 87c/880/884, angular velocity 150/154/158.
Class info 294->64 supplies angular speed; entity 1b0 supplies the timestep.
Pitch delta uses speed * timestep * command.x + pending pitch; yaw uses
command.y * speed * timestep + pending yaw. The original rounds each delta
before dividing by timestep for angular velocity. Both pending deltas and
all three command components are consumed.

Body pitch/roll and eye yaw/roll become zero. Eye pitch clamps to the binary32
values +/-1.5707963705062866. Yaw outside +/-6283.185546875 resets to zero;
otherwise repeated binary32 additions/subtractions wrap into inclusive
+/-6.2831854820251465. This is not wrapping into +/-pi. Nonpositive timestep
and nonfinite inputs are explicit port errors, preserving state.

`python tools/verify_look_update.py --nxdk` executes the original unmodified
49de50..49dfcd instructions, including vector-clearing callees. All 1,200
finite cases match all 56 state bytes on PC and compiled NXDK under Unicorn.
This is not a live XEMU test and does not validate original caller selection.
Full PC/NXDK builds and the four existing CTests pass.

The remaining original path builds body orientation with 4fbee0, copies it to
120 and 48, then builds eye orientation at 7e0 through 4a0d70 using body plus
eye angles. **Do not replace 4a0d70 with a conventional Euler matrix:** its raw
code uses sin(pitch) vertically and 1-abs(sin(pitch)) horizontally before
4fc500 normalization. Verify these instructions and matrix conventions next,
then bind separate look orientation into the shared scene and live XEMU
telemetry. The current diagnostic still uses body orientation for the eyes.
49cd30 physics commit and the 174/870/888 auxiliary clears are not owned by
this scalar API. No screenshot was captured for this unbound change.


## Reconstructed eye matrix (4a0d70)

`rf_look_orientation` now executes the original eye-matrix construction in shared
C with x87 precision helpers. The horizontal right vector is
(cos(yaw), 0, -sin(yaw)); the forward vector uses sin(pitch) vertically and
1-abs(sin(pitch)) horizontally. The initial up vector is forward cross right.
The recovered 4fc500 path normalizes forward and up, then reconstructs right
and up with two cross products. It does not perform another normalization
following those cross products. Roll is ignored. The API deliberately accepts
only the wrapped look domain: pitch +/-binary32 pi/2, yaw +/-binary32 2pi.

`python tools/verify_look_orientation.py --nxdk` executes the whole original
4a0d70, including all callees, with no intercepted math. It explicitly sets
Unicorn's initial FPU control word to 037f; an uninitialized Unicorn instance
reports zero, which is unsuitable as an implicit floating-point contract.
Original CRT/game-wide FPU initialization is not proven by this harness.
Port x87 helpers select extended precision and restore the caller's control.

Results: 1,200 compiled NXDK cases byte-exact under Unicorn; 1,188 native PC
cases byte-exact. Twelve native PC vertical-pitch cases differ from emulated
original math by at most 4.3855977877038654e-17 in near-zero matrix terms.
The absolute tolerance is 1e-16, not a pixel-sized or single-precision epsilon.
The suite includes signed zero, +/-pi/2 pitch, +/-pi and +/-2pi yaw, ignored
roll and deterministic random angles. PC and NXDK builds pass; four existing
CTests pass. This evidence does not claim live XEMU integration.

Next combine the angle updater with the original yaw-only body matrix and eye
matrix, bind the distinct orientations to scene motion/camera telemetry, and
verify the full 49de50 pose-writing path plus live XEMU output. Input acquisition,
player identity and camera collision remain open. No new rendered screenshot
was captured for this unbound function.


## Live pitch-control binding

`rf_look_update_pose` combines the scalar update, original yaw-only body matrix
(4fbee0/4fbe40), and eye matrix. `verify_look_update.py --pose --nxdk` now runs
49de50 through 49e02c with all callees intact: 1,200 compiled NXDK cases match
all scalar/body/eye words. PC scalar fields match exactly; matrix differences
are bounded by 4.5102810375396984e-17 at vertical pitch. Physics commit and
auxiliary entity vectors remain outside the adapter.

The `--look` scene check, `--scene-look-last` PC preview and Xbox
`actor-look.flag` select a process-local pitch-only profile on the retained
664-frame route. The input alternates +/-0.25 over 90-frame intervals, with
speed 1 and timestep 1/60. Frame zero starts upright at yaw zero and consumes
zero input. This is explicit diagnostic input, not the original controller
mapping. The physics body's orientation remains unchanged; yaw/body turning
requires orientation/physics integration. The first-person camera now uses
the recovered eye matrix rather than the body's matrix.

`python tools/verify_scene_eye_view.py artifacts/scene-look.txt` replays all
664 original angle/pose updates sequentially, then original 4194e0 and camera
copy. All bytes match, including 30 eye-height transition frames. It also
checks the assumption that every fixture body orientation is identity. A
64-record ring stores frame plus rf_look_pose (33 words per record), alongside
the existing eye and camera rings. The standard eye profile's PC camera/ring
trace is unchanged.

Live run `20260909-200813-666493` passes in XEMU: exactly 67,108,864 base bytes,
zero plugged memory, final look/eye/camera rings match PC, all 664-frame world
and camera hashes agree. World hash 586925489, camera hash 1016562889,
peak projected world bytes 162624, two MiB CPU/GPU vertex capacity. Body hash
533762320 remains unchanged (663 updates, three support losses, four landings).
No framebuffer was captured for this memory validation.

```powershell
python tools/xemu_smoke.py --actor-look --no-capture --seconds 300
```

The disc currently includes actor-look.flag as well as the prior eye/follow
profile flags. Disable the look flag before running the older eye-only smoke
expectation. Main campaign player identity/input, yaw/body physics, camera
collision and view weapon remain open.


## Yaw and physics orientation binding

The `--turn` scene checker, `--scene-turn-last` PC preview and Xbox
`actor-turn.flag` extend the pitch profile with alternating +/-0.2 yaw input
on 120-frame intervals. Frame zero starts with zero input. The recovered look
pose now commits current and pending physics orientation, and recalculates the
world inertia tensor through the existing complete 49cd30 reconstruction.
Body rendering and swept sphere centers already read the current orientation;
movement command transformation does too. Eye height is recalculated using the
new body matrix, while eye orientation retains the separate pitch.

The original 49cd30 only rebuilds the world tensor; it does not rotate sphere
records or rebuild position bounds. Static ground preparation explicitly uses
identity query orientation. This fixture's selected lower sphere is on the
vertical axis. Do not infer that sphere centers should be permanently rotated.

`verify_scene_eye_view.py artifacts/scene-turn.txt` now replays all 664 complete
49de50 calls, including its world tensor update, then original eye/camera
instructions. It verifies the committed body matrix, eye matrix, all scalar
look state and local/world tensor trace for every frame. All bytes match.
Caller scheduling and controller acquisition are still diagnostic boundaries.

On PC the changed route stays supported after its initial landing: one landing,
zero support losses, 447 contact passes and no capped physics update. The old
straight-route check required at least one ledge departure. Only the turn
profile relaxes that route-specific condition, retaining the invariant that
landings equal support losses plus one when finally grounded (zero when in air),
plus all frame, ring, room, allocation and collision-pass checks. The pitch-only
camera/body trace remains identical after this change. Four CTests pass.

The next major step is player input and ongoing runtime ownership. Installed
NXDK has an SDL controller example at samples/sdl_gamecontroller/main.c and
an Xbox joystick backend using usbh_lib/xid_driver in
lib/sdl/SDL2/src/joystick/xbox/SDL_xboxjoystick.c. These are local API references,
not copied project code. Any testing must remain process-local; no host input.


Live XEMU run `20260909-201342-341995` PASS: stock 67,108,864 bytes,
zero plugged memory. All final look/eye/camera rings and ten actor rings match
PC; the final 308-byte physics body matches exactly. Across 664 rendered frames
and 663 physics updates: body hash 2897823093, actor geometry hash 934208692,
world hash 1553922570, camera hash 2407323874. Peak world projection is 208320
bytes within the unchanged two MiB vertex allocation. Available bytes after
upload: 41029632; after CPU mesh release: 44187648. One landing, no support
losses. This is a diagnostic working set, not a full campaign memory budget.
No framebuffer capture was requested.

```powershell
python tools/xemu_smoke.py --actor-turn --no-capture --seconds 300
```

The current disc contains actor-turn.flag (alongside look/eye/follow flags).
Disable it before asking the smoke harness to expect a pitch-only profile.

## Original campaign player-start provenance

`tools/verify_player_start.py` executes the installed RF.exe instructions for
all 94 level player-start payloads and compares them with the compiled shared C
parser. Original executable SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

The section reader at `463d20` loads position into `6460fc` through `52ca00`
and orientation into `646108` through `52cac0`. The matrix reader consumes
three disk vectors into destination offsets +24, +0, +12, respectively
(`52caef`, `52cb01`, `52cb0e`). Thus runtime rows are disk rows 1,2,0;
the existing C expression assigns disk rows to runtime indices 2,0,1. These
are the same ordering, with no additional transpose or coordinate conversion.

The ordinary single-player startup span `45c798..45c807` copies these globals
through the original vector/matrix assignment routines, then calls `4a4130`
with the local player, class identity from player+18, copied position, copied
orientation, and skin index -1. The verifier executes that span through the
factory entry and checks the actual argument pointers and all 48 transform
bytes. All 94 installed levels match the compiled C parser exactly. The harness
then executes the original player-factory prefix through generic entity factory
`422360` entry, including `4a6200` and the player-name accessor `4ff480`.

The generic factory receives class identity, player name, UID -1, position,
orientation, creation flags 1, and normalized skin index 0. Original `4a415b`
normalizes a negative/out-of-range skin index and clears local player+f5c only
in that invalid-index branch. `4a6200` clears bit 8 of local player+10. Neither
write applies to a nonlocal player. The creation prefix does not change the
ordinary spawn transform when the two pending-override flags are clear.

An additional 24 original-execution cases cover local/nonlocal player, skin
indices -1/0/2/3 with a three-entry table, and position-override flag 0/1/2.
Only exactly 1 consumes the position at `7c7628`, writes it into the caller's
position buffer, and clears `7c75c8`. Flag 2 remains untouched. Orientation is
unchanged in these cases; the separate `7c73b4` orientation-override branch
through `4fcea0` is not covered. These branch assertions currently characterize
the original; they are not a claim of a reconstructed C player factory.

Scope: `523990` is a fixture returning field-present and `52cf60` is a fixture
supplying sequential 12-byte reads from the real player-start payload. The
original section reader, vector/matrix reader control flow, copy routines and
SP argument preparation execute without replacement. The fixture supplies an
opaque class identity/name pointer and ordinary load mode; it stops before
generic entity allocation at `422360`.
It does not validate complete file I/O, mode selection, multiplayer spawn,
later factory side effects, or subsequent camera initialization. Reports are local
at `artifacts/player-start-verification.json`.

The live diagnostic still binds serialized miner UID9858 and diagnostic camera
offsets. This result establishes the campaign spawn source for upcoming player
lifecycle integration; it does not make that diagnostic a campaign player.
