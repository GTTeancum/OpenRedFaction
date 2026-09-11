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
through `4fcea0` is not covered.

`src/core/player.c` now reconstructs this bounded prefix as
`rf_player_spawn_prepare`, with caller-owned player state, spawn request and
pending position. It preserves the original signed skin-index comparison,
local-only resets, and exactly-one override consumption. It allocates nothing
and is compiled in both targets. Null pointers or a pending scalar outside the
original byte range return RF_RANGE before mutations. Distinct stable records
are an API precondition. The routine deliberately contains no orientation
override or generic factory call; live scene ownership integration remains open.

Run `python tools/verify_player_start.py --nxdk` after both builds. All 24 cases
compare all 48 bytes of compact inputs/outputs against original instruction
results, through the PC probe and the compiled NXDK routine executed in Unicorn.
This verifies compiled CPU behavior, not an XEMU runtime integration. The
report retains both compiled executable hashes. Both builds and five CTests
also pass after this addition.

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

## Local player binding after creation

`rf_player_bind_local` in `src/core/player.c` reconstructs `4a40f0` and its
`489f70`/`4895f0` callees using compact borrowed views. Original instruction
order publishes entity at `5cb054` and its inventory at `5af45c` (entity+2a0),
writes entity+1f8 to 2, clears entity+560 to -1 only for object type zero,
copies entity+2c into local player+14, then invokes `4a4980` with the inventory's
primary weapon (entity+2a4). The C binding publishes matching borrowed pointers
and scalar state before invoking its required weapon-selection callback.
It does not implement `4a4980`, whose animation/weapon/viewmodel effects remain
separate. Null arguments return RF_RANGE before any mutation.

`python tools/verify_player_binding.py --nxdk` executes the whole original
binding and both mode/type callees. Only the weapon-selection boundary is
observed and returned without executing its behavior. Across 81 combinations
of type, handle, primary weapon and prior state, it checks the published
original pointers, callback arguments and complete entity/player fixture
storage for unintended writes. The PC probe and compiled NXDK routine match
all compact state at callback entry, including inventory identity and one
callback invocation. Reports and executable hashes are retained locally in
`artifacts/player-binding-verification.json`. Both builds and five CTests pass.

This is compiled CPU verification in Unicorn, not a live XEMU player-ownership
test. The binding borrows an already-created entity; it performs no allocation,
registry insertion, lifetime management or camera construction. Those steps
must be connected before replacing the diagnostic miner in the live scene.

## Class-based assets for player creation

`rf_entity_skeletal_assets_load` loads class/skin metadata and resolves the
compiled skeletal mesh without a `rf_level_entity` or UID scan. It shares the
same internal resolver with `rf_level_actor_assets_load`; the level wrapper
still validates the authored entity and retains its exact transform/skin.
Each public wrapper has one local working result and commits only on success.
The internal resolver uses those working fields directly, avoiding a second
large nested asset record on the Xbox stack. No persistent allocation or
ownership is introduced; temporary table storage remains budgeted and released.

`tools/verify_class_actor_assets.py` checks all 168 authored class/skin selections
against separately verified table metadata and archive entries, including mesh
offsets/sizes and ordered replacements. Four absent/unsupported/missing-mesh
cases verify unchanged output. `tools/verify_level_actor_assets.py` still passes
all 78 Live Mines bindings and its five failure guards after the refactor.
Both PC and NXDK builds and five CTests pass. This is port-owned resource
composition, not evidence that the original complete class loader is recovered.

Campaign player creation can now request `miner1` directly instead of borrowing
UID9858's class/skin resource binding. The live scene still uses that diagnostic
record until factory state, spawn transform and local ownership are connected;
no campaign-start rendering or input behavior changed in this refactor.

## Spawn orientation initializes body and eye angles

`tools/inspect_player_spawn_look.py` executes original `422e2c..422e82` and all
callees without replacements for all 94 verified level-start matrices. The
fixture supplies the parsed run descriptor (rotation references 1,3,0) and
copies the spawn matrix into entity+48 and physics orientation entity+fc.
The latter equality is an explicit fixture precondition, not execution of the
complete generic factory. FPCW is 037f, consistent with earlier CPU probes.

`4fc060` extracts pitch/yaw/roll into the local vector. `422e50..422e59`
initializes entity+864 body angles to (0,yaw,0). `433cf0` then applies
`4faa30` row dot products against entity+fc, and `433d30` preserves only axes
whose movement rotation reference equals 1; `422e7d` stores that result at
entity+87c. For run mode only pitch survives. This path precedes the regular
look tick and is not equivalent to initializing all angles to zero.

89 of 94 authored starts produce nonzero body yaw. L1S1 produces body words
`[0,1074762629,0]` (yaw 2.2433788776397705 radians) and eye words
`[595976323,0,0]` (pitch 1.451497330547018e-17). The tiny eye residual is retained
by original arithmetic. Reports retain all inputs/results in
`artifacts/player-spawn-look.json`; this report is original execution evidence,
not yet a shared C comparison or a first-tick/live XEMU test.

The extractor calls `504ea0`, which uses atan(y/x) and adds binary32 pi when
x is negative, with explicit zero-axis branches. Its quadrant/range and
rounding differ from replacing it with a plain atan2 call. The extractor also
uses a selected sin(yaw)*forward.x or rounded-cos(yaw)*forward.z product when
deriving pitch/roll; preserve the actual instruction math during reconstruction.
The live diagnostic's frame-zero memset of `actor_look` remains appropriate
only to its current zero-angle fixture, and must be replaced for campaign spawn.

`rf_look_spawn_angles` now reconstructs that span in shared C. It accepts the
body/physics matrices and three movement rotation references, and returns body
and eye angles without mutating unrelated look state. It preserves the original
atan quotient/quadrant rules, binary32 constants and stores, yaw-only body,
and row-dot/filter sequence. NXDK uses x87 atan/sin/cos and extended arithmetic,
temporarily selecting extended precision and restoring the caller's control
word. Finite inputs are checked before computation and failures preserve output.
This helper does not select the initial movement mode or create/attach a player.

`python tools/verify_spawn_look.py --nxdk` compares the full original span and
all callees against the PC probe and compiled NXDK helper. All 601 cases are
bit-exact on both targets: 94 installed starts, seven axis/degenerate cases and
500 additional matrices with varying rotation references. NXDK also preserves
an incoming 027f control word while reproducing the 037f reference. Local report
`artifacts/spawn-look-verification.json` includes executable hashes. Both full
builds and five CTests pass. The diagnostic continues using its old initialization
until the campaign player spawn/ownership path is connected; no live XEMU
integration is claimed by these CPU checks.

## Campaign-start scene integration (opt-in)

`rf_scene_set_campaign_spawn` retains the authored start before diagnostic
camera mutation. In this profile the scene loads miner1's base assets by class,
uses the retained position/matrix for initial physics placement, and seeds body
and eye angles through `rf_look_spawn_angles` before the first regular look tick.
The temporary entity-shaped placement record carries local UID -999 but is not
a registered gameplay entity. It has no serialized level record or UID lookup.
The existing fixture still supplies unarmed locomotion/initial poses; armed
player initialization and class-cache ownership must be integrated separately.

PC: `rf_pc_play --spawn-replay Installed_Game <inputs.bin> <output.ppm>`.
Xbox: `campaign-spawn.flag` alongside the existing input/scene flags. The replay
harness accepts `--campaign-spawn`, compares the 19-word spawn telemetry with
the original start/look reports and PC, and preserves/restores the flag and
command file. Normal interactive startup remains on the previous profile.

Stock-64-MiB XEMU `replay-20260909-220550` passes all 120 neutral ticks, exact
input ring, spawn telemetry, final 308-byte body and world/camera hashes.
CPU and sampled completed GPU world mesh peak at 2,077,320 bytes within the
unchanged 2,097,152-byte allocation. The earlier `replay-20260909-220423` correctly
remains FAIL: Xbox's earlier preview-camera call replaced the spawn before it
was retained. Capture now precedes that mutation and the rerun passes.

PC's 480-tick diagonal replay also completes (2,065,896-byte world peak).
The stationary yaw replay fails at tick 342, stage 5, RF_RANGE: the full world
allocation is exhausted while emitting another mover triangle. This is a new
reproduced capacity case, not a passing test or a reason to increase the stock
memory target. Projection/visibility or bounded submission work remains open.
Logs are retained as `artifacts/input-replay/spawn-*.txt`. The old 664-input
diagnostic trace and final image remain exact after these changes. Both builds
and five CTests pass. No new screenshot is published: the neutral starting view
is another bare tunnel, not the meaningful scene requested for the README.

## Class cache versus player animation at creation

`python tools/inspect_player_class_cache.py` executes original `423b90` and
the factory span `42326c..4232b4`, including their unmodified copy callees.
The SHA-checked original passes four cached/uncached, player/non-player gate
cases and 180 sphere-copy fixtures (counts zero through eight). The report is
`artifacts/player-class-cache.json`. These are original-execution fixtures,
not a PC/NXDK implementation comparison or a complete spawn test.

The gate reads bit `40000000` from class `entity+29c`, flags `+724`.
A warm cache returns without entering `423bd0`, regardless of object player
flag 8. Cold cases stop at the actual builder entry and verify its entity
argument; this harness does not replace or execute the cold builder.

After initial motion selection and the cache gate, the factory copies each
standing center from the class's inline array at `+cec` (four-byte header,
40-byte records, center at record `+18`) to the entity array at `+184`
(storage pointer at array `+8`, 24-byte records, center at record `+0`).
The executed span copies exactly 12 bytes per sphere, preserves the remaining
sphere bytes and both source class and entity storage, and balances the stack.
The loop bounds come from the entity array count; the original assumes matching
class storage. The fixture supplies that valid precondition.

Together with the separately recovered Live Mines entity order and first
selector, this means the player can start with the armed blend while retaining
centers cached by the earlier miner1 NPC. The current diagnostic instead derives
its body spheres and class offsets from one initial unarmed pose. Integration
must separate class cache initialization from per-entity playback; changing the
selector alone would incorrectly make collision centers depend on the armed
blend. Full cold-cache sampling, runtime class ownership, and the remaining
player factory are still open.

Class sampling is now separated from body construction in the shared diagnostic:
`class_spheres_build` produces resolved sphere records, and `stance_cache_build`
uses those records directly instead of reading an allocated entity body. Stance
and eye sampling completes before body allocation. No persistent allocation is
added. The pose source remains the existing unarmed fixture; this refactor does
not yet implement first-user class ownership or enable armed player playback.
The before/after PC 664-input normal replay and 120-input campaign-neutral replay
have identical complete stdout traces and final PPM bytes; evidence is retained
in `artifacts/class-cache-refactor/report.json`. PC/NXDK builds and five CTests pass.
Stock-64-MiB XEMU campaign replay `replay-20260909-222821` also passes all
120 inputs with the harness's PC/original spawn, body, input and mesh checks;
the normal interactive disc profile is restored afterward.

## Armed campaign initialization with independent class sampling

The opt-in campaign profile now uses armed locomotion candidates 1/3/5 from
the verified miner1 default-handgun/player-flag creation case. Frame zero
uses the original neutral movement request and run mode before accepting
subsequent movement/stance commands. Its controller starts from state zero
and transitions toward attack_stand over .25 seconds. This selects skeletal
animation only: no weapon inventory owner, firing or first-person gun is added.

`campaign_class_build` constructs the separately verified earlier miner NPC's
neutral first-controller state in its own playback/resource storage and samples
class spheres, stance centers and eye offsets there. Player animation weights
cannot alter that cache. This uses temporary bone matrices plus the existing
stance workspace, freed during initialization; no new persistent allocation.
The caller still emulates the known Live Mines first use, rather than owning
a campaign-wide class registry.

The independent sampling exposed a legacy rendering-fixture translation in
standing body spheres: two centers were .25 units too low. The original
`503270` queries, now included in `verify_eye_setup.py` after the loaded original
stand/crouch sequence, confirm corrected standing Y values .4198943079 and
.8807211518. The unattached sphere is unchanged. Standing/crouching height
difference is now .6367877722; both eye offsets remain bit-identical. The
normal miner diagnostic retains its historical fixture and trace for now.

`tools/verify_campaign_initialization.py` verifies the integrated initial
controller, active-slot count and first-slot weight, six eye words and all
50 stance-cache words against the original selector/loaded-pose fixtures.
It runs 120 neutral inputs; report `artifacts/campaign-initialization/report.json`.
The normal 664-input trace and final image remain identical to the preceding
build, and all eight 480-input PC capacity sweeps and five CTests pass.
Both PC/NXDK builds pass. Stock-64-MiB XEMU `replay-20260909-223324` passes
120 ticks with PC-matching initial animation, eye, stance, spawn, input, body
and world/camera telemetry. The replay harness now retains these extra comparisons.
The 480-tick campaign diagonal movement/turn replay `replay-20260909-223646`
also passes on stock-64-MiB XEMU, including those initial-state comparisons.
Normal interactive flags are restored after both runs; no new screenshot.

Attempting the original creation flag 1 exposed a separate required integration:
it sets physics flag 80, which the current `rf_physics_static_contact` explicitly
rejects. The scene stopped in its frame-zero falling check with RF_RANGE.
Original `49d7e0` has a distinct response for that flag, including tangential
velocity handling and additional predicates. The campaign scene therefore still
uses its existing diagnostic physics flags; enabling actual player flags requires
reconstructing that branch, not removing the guard or routing players through
the non-player response. The full player factory remains incomplete.

## Player crouch eligibility versus diagnostic stance selection

Original `430c70` has a distinct player-input crouch path. With its ownership,
movement, UI/input and player-state gates satisfied, a new crouch request calls
`4a5c50`. On success it calls `4289d0` immediately, sets player byte `+b1`,
selects speed mode zero via `427450`, and requests logical animation state 9
with duration .25. It does not wait for the animation transition before replacing
collision centers. Release calls `428a60` and clears the player crouch state/
restores speed only when clearance succeeds. These are disassembly/decompiler
observations; the full outer input routine is not yet replayed or integrated.

`rf_player_can_crouch` now reconstructs `4a5c50` using resolved object metadata.
It rejects control-object kind 5, a missing entity, parent kind 1 or 4, entity
attachment `+75c` other than -1, and movement modes other than 1/3. The original
uses `4a5b30`, `4290d0`, `429d20`, `429f90`, real object lookups and `486c90` to
resolve those gates. The C helper supplies eligibility only, without allocating
an entity, mutating stance, checking clearance or inventing outer ownership.

`python tools/verify_player_crouch.py` executes complete original `4a5c50` with
all callees unchanged and no hooks, using registered object/class fixtures.
All 3,456 combinations match PC and compiled NXDK exactly, with 40 allowed
cases and unchanged input/object storage. Cases span entity presence, six
control/parent kinds, three attachment handles and all movement modes 0..15.
Report `artifacts/player-crouch-verification.json` includes executable hashes.
Both builds and five CTests pass. Live campaign crouching still uses the prior
diagnostic selector until immediate stance effects and the outer gates are
connected; no live XEMU crouch equivalence is claimed by this predicate test.

## Owned-player animation selection after initialization

The generic `41f270` controller update chooses `4a5cd0` when `42a8e0` sees a
player entity with a non-null owner at `+1430`. Before ownership is installed,
the first creation call takes the existing entity-selector path. Thus the
verified initial armed blend does not prove the subsequent player selector is
already integrated. The campaign diagnostic still uses ordinary movement
candidates and must switch its later-frame path when player ownership is built.

`tools/inspect_player_motion.py` executes complete original `4a5cd0` and all
callees without hooks in 3,328 ordinary non-rotating, parentless entity cases.
It supplies identity logical-motion mappings and a fresh controller; actual
animation sampling and unavailable-state fallback are separate tests. Inputs
span all 16 movement modes, crouch, attachment, primary weapon, weapon-hidden
flag and 13 direction/threshold cases. Only controller storage changes, and
all selected states/durations match the recovered branch rules. Reference
inputs/results are retained in `artifacts/player-motion-reference.json`.

Within those preconditions, crouch takes precedence over falling/swimming:
state 10 is selected when X or Z is strictly outside +/-binary32 .1, otherwise
state 9. Y alone does not select crouch movement. Non-crouched modes 3/8 use
state 14; modes 4/7 choose 18 for exact-zero direction or 19 otherwise.
Other modes use the original `4fa7a0` magnitude estimate:
largest absolute component plus 1.5 times (middle times .25 plus smallest
times .125). Its unrounded comparison against binary32 .25 selects idle versus
movement. An attachment selects 16/17; an available, unhidden primary weapon
selects 1/5; otherwise selection is 0/4. Each new request uses duration .25.

Earlier priorities in `4a5cd0` select state 15 for parent kind 4 and 20/21 for
the two `42ac80`/`42acd0` seat predicates. Those branches remain outside this
parentless fixture. The full player selector and immediate input-driven stance
effects still need shared C/live integration; reusing the diagnostic's generic
nonzero-command selector would lose the original thresholds and priorities.

`rf_player_motion_choose` now implements the complete selection priority in
shared C with explicit resolved predicate inputs. It returns the logical state
or -1 when no entity is present; the caller still owns the current/next check,
missing-motion fallback and .25-second controller request. Finite directions
and canonical predicate values are required, and invalid input preserves output.
No allocation or gameplay side effects are introduced.

The original fixture now adds 64 registered-parent/seat or missing-entity cases.
All original callees remain unchanged: parent class flag 400000 and actual seat
records drive `42ac80`/`42acd0`, including both seats matching to verify first-seat
priority. This expands coverage to all selected-state branches (15,20,21 included)
and 3,392 total cases. `tools/verify_player_motion.py` compares all selected states
exactly on PC and compiled NXDK; 13 compiled invalid-input cases preserve input
and output. Report `artifacts/player-motion-verification.json` retains executable
hashes. Both builds and five CTests pass.

Live campaign integration remains coupled to the immediate crouch effects:
switching only the movement selector would let the current delayed diagnostic
stance path overwrite or lose crouch transitions. The new selector therefore
remains separate until that owner transition is connected; no live XEMU stance
equivalence is claimed yet.


### Campaign immediate stance integration (2026-09-09)

The opt-in campaign fixture now follows the ordinary unattached player branch
of `430c70`: eligible crouch immediately applies sphere/ground/speed changes,
then requests state 9. Release uses the existing standing clearance path.
After the original first initialization frame, `rf_player_motion_choose`
selects logical animation states through the recovered `4a5cd0` priorities.
The fixture supplies an absent parent/attachment and a present default handgun;
full entity ownership, inventory, input locks and vehicle gates are still open.

`tools/verify_player_stance_live.py` checks all 64 recorded ticks, with crouch
press at tick 8, movement at 24 and unobstructed release at 40. Physical crouch
changes on tick 8, logical states are 9 then 10, and tick 40 clears crouch and
selects armed idle 1. XEMU replay `20260909-231211` passes in stock 64 MiB with
all 512 stance and 768 motion telemetry words exactly matching PC, plus final
body, input, spawn and class cache checks. This does not verify blocked standing
or physical controller crouch. The user has confirmed interactive movement.
The normal 664-tick diagnostic replay retains identical output and raster.

PC campaign replay now exports the existing stance and motion rings; no new
resident telemetry arrays are allocated. Motion ring word 0 is 1 for the generic
selector and 2 for the owned-player fixture. In the latter, word 6 is the selected
logical state and words 7/8 are -1; remaining columns retain their previous meaning.


### Original blocked-standing release evidence (2026-09-09)

`tools/inspect_player_stand_release.py` executes `430df8..430e19`, including
all of `428a60`, unchanged circular player lookup `4a3740`, sphere copying and
speed setter `427450`. Its 162 fixtures combine 0�8 spheres, blocked/clear
query outcomes, absent/matching/nonmatching player lists, and crouch bytes
0, 1 and 255. It verifies complete actor/player/sphere storage and balanced
stack state. A blocked query changes none of those records and invokes neither
ground refresh nor the speed setter. A clear query copies just each sphere's
12-byte center, clears entity flag 0x400 and the caller's crouch byte, then
requests ground refresh before restoring normal speed. An uncrouched caller
makes no query at all.

Only the collision-query return at `499ed0` and ground-refresh boundary at
`4a0840` are supplied/skipped. The query's start, endpoint and body argument
are checked; ground refresh sees the cleared stance flag. This verifies the
release control flow, including preservation on failure, rather than collision
geometry or the complete outer input path. The live campaign still needs a
recorded low-ceiling traversal; the existing copied-actor clearance diagnostic
and this original-code check are narrower evidence.


### Outer stance gate (2026-09-09)

`rf_player_stance_enabled` reconstructs the gate from `430c70` to the action-4
query at `430daa`, using resolved entity ownership, environment-query result,
movement/speed modes, entity kind, attachment +1380, player byte +f38 and the
result of `444ac0`. Both press and release are gated. Mode 2 takes an environment
transition path and never reaches stance input. Otherwise mode 1 with speed
0/1, mode 3/8, or kind 1 with attachment +1380 equal to -1 can reach the query,
provided the ownership, environment and blocking checks allow it. The meanings
of +f38 and the global blocker are not named as UI states without evidence.

`tools/verify_player_stance_gate.py` runs the original prefix with unchanged
entity lookup, ownership comparison, movement/kind predicates and `444ac0`.
Its 4,608 cases span missing/unowned/owned entities, environment absent/present,
all 16 movement modes, three speed modes, two entity kinds, attachment -1/0,
player block byte 0/255 and global result 0/1. Shared PC and compiled NXDK match
all cases (69 reach stance input); a null NXDK input returns disabled. The world
environment query is supplied and its transition handlers are skipped. This
proves the gate, not environment mutation, action processing or full ownership.

The campaign callback consumes live movement and speed mode through this gate.
Ownership, environment, kind and block fields remain ordinary-player fixture
values until the corresponding lifecycle is implemented; those are not live
registry or lock checks yet. The existing 64-tick press/move/release test passes.

Stock-64-MiB XEMU replay `20260909-231939` also passes, with exact PC stance,
animation, input and final-body memory comparisons. PC/NXDK builds and all five
CTest checks pass.


### Movement-region query (2026-09-09)

The previously unnamed `45cca0` query traverses the pointer list at `6460b0`
and returns the first region whose oriented box contains the player position.
Region center is +4, orientation matrix +10 and full dimensions +34. The
`507a50` predicate subtracts the center with float stores, applies matrix rows
in original Z/Y/X accumulation order, and tests inclusive half-size bounds.
The region kind at +0 does not filter this query. Its caller `4281e0` connects
the selected region to movement mode 2, named `climb` by the authored movement
table. This is distinct from the room-water predicate `4ce080`.

Shared `rf_collision_point_oriented_box` and `rf_player_movement_region_find`
now implement those operations without allocation. The latter returns the
first matching index or UINT32_MAX and borrows the input array. It stops on
an earlier match, preserving the original priority for overlapping regions.
`tools/verify_player_regions.py` executes complete original `45cca0` and all
unchanged callees, with static vector constructors already initialized. All
1,521 PC/NXDK cases match, including empty lists, 1�3 boxes, overlapping regions,
translated/yaw-rotated boxes, exact faces/edges/corners and neighboring float
values. Three compiled invalid-input checks preserve the output sentinel;
compiled calls preserve the caller's x87 control word. Builds and CTest pass.

This does not yet load authored region records or implement climb entry/exit.
The campaign's empty-region assumption remains until those are connected;
replacing it solely with box detection would suppress stance without providing
the corresponding climb behavior.


### Authored movement-region reader (2026-09-09)

The level dispatch at `460e51..460e7a` maps section 0xD00 to `462e40`.
That loader allocates a 64-byte runtime region per record and appends it to
`6460b0` in file order. For installed v180 records it reads a discarded UID,
a length-prefixed name, position and orientation, another length-prefixed name,
one editor byte, a 32-bit kind and three full box dimensions. Versions below
67 have an extra string; the shared reader deliberately supports v180 only.
The original default constructor is `465f40`.

`rf_level_regions_begin` / `rf_level_region_next` now decode these records
without allocating. They reuse the bounded section-reader cursor, discard the
editor-only fields, reorder orientation rows consistently with the existing
level transform reader, and produce `rf_player_movement_region`. Errors preserve
both cursor and output; the final record must exhaust the section exactly.
Nonfinite transforms and negative/nonfinite dimensions are rejected.

`tools/verify_level_regions.py` independently decodes every installed section
and compares 147 records across 39 levels with both the PC file reader and the
compiled NXDK next-record reader. The NXDK fixture supplies archive reads while
retaining the compiled level bounds checks. All 80 truncated/malformed fixtures
preserve cursor and output. Full PC/NXDK builds and CTest pass. This evidence
checks the recovered layout against installed data; it does not execute the
original loader or establish live climb behavior. Retained region ownership
and climb entry/exit integration are still required.


### Original climb-entry state and ordering (2026-09-09)

`tools/inspect_climb_entry.py` executes complete `4281e0` with unchanged
`427fd0`, `42a020`, `427450` and `4339d0`. All 768 combinations of capability,
16 movement modes, class kinds 0/1, attachment +1380 -1/0, region kinds 0/1/2
and descriptor enabled/disabled pass. The fixture uses ordinary entity type 0;
`486c90` does not resolve arbitrary entity types as ordinary class kinds.

Without class flag +724 bit 4, the entire actor remains unchanged and no
callbacks run. Otherwise entry clears entity +13ec, sets +13f0 to the selected
region, and optionally calls `48a9c0` if `42a020` succeeds and region kind is 2.
Its arguments are entity, the three position floats by value, ID 18, scale 1,
and zero. At this boundary the region fields are already committed, while the
old movement descriptor and speed mode remain installed. The effect's concrete
semantics and implementation remain unrecovered; it is not assumed to be damage.

After that call, the original sets speed mode 1 through `427450`, resolves
movement descriptor 2 through `4339d0`, installs it at +858, installs the region
orientation (+10) at +85c, and sets +8ac to -1. An unavailable descriptor 2
falls back to descriptor 0 and updates global selected descriptor `630050`.
The verifier checks complete actor bytes, balanced stack, effect arguments,
intermediate state, and effect/speed/descriptor call order. It skips only the
`48a9c0` body. No shared-C climb transition or live climbing is claimed yet.


### Climb effect resolved as sound routing (2026-09-09)

`48a9c0` dispatches to non-positional `505560` or positional `5056a0` playback.
`48acf0` requires ordinary entity type 0 and a non-null owner at +1430;
`40d740` reads owner camera (+c4) mode +8. Only an owned ordinary entity with
camera mode zero takes non-positional playback. This names the branch from
its actual fields without assuming every nonzero mode is third-person.

Shared `rf_player_sound_route` produces a borrowed-data-free request. The
non-positional branch forwards sound ID, caller pan and volume, with group 0.
The positional branch forwards ID and position, uses volume 1 and group 0,
and leaves pan for backend computation. Original `5056a0` ignores its fourth
argument; the dispatch fixture checks that `48a9c0` supplies `173c378` there.
The climb call's ID is 18; its specific sound asset has not been identified.

`tools/verify_player_sound.py` executes original routing with unchanged owner
and camera predicates and captures playback-boundary arguments. All 648
combinations of entity type, owner presence, camera mode, sound ID, volume
and pan match PC and compiled NXDK requests. Both builds and CTest pass.
This does not execute the audio backend, load sound assets, or complete live
climb entry. The earlier term "effect 18" can now be read as sound ID 18;
no damage or visual effect is implied.


### Shared climb entry (2026-09-09)

`rf_player_climb_enter` now performs the recovered `4281e0` transition using
borrowed region and movement-descriptor storage. Class capability bit 4 gates
all writes. It prevalidates speed and sound routing, commits previous-region
clear/current-region assignment, emits the optional sound request, restores
speed through `rf_movement_set_mode`, selects descriptor 2 with descriptor-0
fallback, installs the region matrix and resets the contact handle to -1.
The caller supplies the resolved free-motion predicate and sound ownership.
The sound callback sees the new region and old movement/speed; it must not
mutate the input or state. No allocation or audio backend is introduced.

`tools/verify_climb_entry.py` regenerates original entry evidence and compares
768 cases against PC and compiled NXDK, including final state and intermediate
sound callback state. Compiled checks also verify that missing required sound
callbacks and malformed speed configuration preserve all outputs and emit
nothing. PC/NXDK builds and all five CTest checks pass. Climb exit, retained
region lifetime in the live scene, movement integration and audible playback
remain separate work; the campaign has not yet switched to this transition.


### Shared climb exit (2026-09-09)

`4280b0` tests class walk flag bit 1 through `427fb0`. Without it, only normal
speed is requested. Otherwise entity crouch flag 0x400 requires successful
`428a60` standing; a blocked query returns without changing climb state or speed.
On success it clears +13ec, restores speed, resolves the class movement name
(+30 string object, pointer +34) through `433a00`, installs the selected descriptor
at +858, installs global identity orientation `73a858` at +85c and clears +148.
It leaves current region +13f0 and contact handle +8ac unchanged. An unmatched
name produces a null descriptor without changing global selection; a disabled
matched descriptor falls back to index 0.

`tools/inspect_climb_exit.py` executes the complete original routine and its
unchanged predicates, standing helper, speed setter and named-descriptor lookup.
It supplies collision query results and skips ground refresh. All 128 combinations
of walk flag, crouch, blocked result, matched/unmatched names, enabled descriptor
and forced action pass, with full actor storage and global selection checked.

`rf_player_climb_exit` implements this using a resolved default index, borrowed
descriptor/identity storage and a standing callback. The callback owns clearance
and stance effects; a blocked result preserves the compact climb state. The state
now includes +148 as `vertical_velocity`. `tools/verify_climb_exit.py` matches the 128
cases on PC and compiled NXDK, including callback timing, full compact state and
selection output. The 768 entry cases still pass, as do both builds and CTest.
Live region ownership, movement while climbing, name resolution wiring and
campaign transitions remain open; these tests do not demonstrate live climbing.


### Retained movement-region lifetime (2026-09-09)

`rf_level_owned_regions_open` now validates the complete section, checks the
owner-plus-record byte budget, and allocates a single contiguous region array.
The source archive may close afterward. Empty sections allocate no heap storage;
failed opens preserve the destination. `rf_level_owned_regions_close` frees and
zeros the owner and is repeatable. Callers must release all player pointers
before closing it; this API does not relocate regions or reference-count them.

The expanded `tools/verify_level_regions.py` compares owned PC and compiled
NXDK results for all 147 records in 39 installed levels. The PC probe closes
the archive and overwrites the level record before accessing the regions. The
NXDK fixture overwrites loader storage, verifies each retained byte, checks
rejection at one byte below the required budget, accepts the exact budget,
observes one allocation for nonempty sections, and verifies one free across
two closes. Allocation/free and archive reads are fixture boundaries. Both
builds and CTest pass. Live scene ownership and climb wiring are still open.


### Campaign climb wiring and pending replay (2026-09-09)

The opt-in campaign scene now owns movement regions until scene cleanup,
loads the movement descriptor table, queries the player position before stance
selection, and invokes the recovered entry/exit helpers. Mode 2 movement uses
its descriptor's eye/body/region axes and `rf_physics_climb_propose`; ordinary
run/fall paths remain available. Sound requests are counted with their IDs;
there is still no audio backend. Region and transition pointers are cleared
before the region owner is freed. The class default remains the campaign
miner's validated run descriptor, with unattached owner/camera fixture values.

A field-mapping correction was necessary: original entity +144 is velocity,
so +148 is its Y component, not a step offset. `rf_player_climb_state` now calls
this `vertical_velocity`, and exit clears the actual body Y velocity. Prior
compact-state fixture values were correct; their semantic name was wrong.

`RF_REPLAY_LEVEL` and `RF_REPLAY_REGION_START` are process-local, headless
campaign test options in the PC frontend. The latter stages the start at the
first authored region center and must not be described as an ordinary spawn.
`tools/replay_campaign_climb.py` reproduces the L1S2 test and records failure
or incomplete transitions explicitly. Currently it fails before simulation in
`rf_scene_world_open_retained`, material stage 11 (RF_FORMAT); stages 10/11/12
now distinguish movers, materials and initial projection in failure diagnostics.
No successful live climb traversal or image is claimed.

The existing L1S1 64-tick crouch replay passes in stock-64-MiB XEMU
`20260909-235522`, comparing PC body, input, stance, animation and the new eight
climb telemetry words. L1S1 has no regions, so this is regression evidence only.
The 768 entry and 128 exit PC/NXDK fixtures and CTest still pass. Next work is
the L1S2 material failure, followed by actual entry/motion/exit replay and XEMU
coverage of a region-containing level.


L1S2 live climb integration (2026-09-10)

The shared campaign budget is now 12 MiB materials and 4 MiB lightmaps, with a
16 MiB referenced-image guard on Xbox. These are bounded initial campaign
limits based on L1S2's measured 9,367,476-byte world material peak and room for
actor textures. CPU/GPU pixels share storage; page rounding and other resident
systems are validated by guest memory, not inferred from the material budget.

replay_campaign_climb.py stages the player at the first authored L1S2 region
center. Neutral ticks are followed by parent-Y input on ticks 8..52 and forward
input from tick 53 to exit sideways. This is process-local test input, not a
claim that controller buttons currently provide the same command. The timeline
records frame/region/mode/position/velocity before motion in a 128x9 word ring
(4608 bytes). The original horizontal-only fixture had entered/exited but did
not climb; the stronger gate now requires more than one unit of vertical travel.

xemu_replay_check.py --climb selects the same fixture using campaign-climb.flag,
restores the prior flag/replay/spawn state and rebuilds the ordinary ISO afterward.
Native run replay-20260910-003213 passes at 120 frames: 62 climbing ticks,
4.371705532 units ascent, one entry and one sideways exit. All 1152 climb timeline
words plus the prior body/input/spawn/stance/animation records match PC. The
emulator reports stock 64 MiB; 9428 pages remain at final presentation, before
post-scene cleanup. Referenced image payload is 10741764 bytes; GPU vertices
use 2097152 bytes. This is sampled runtime residency, not a load-peak proof for
all resources or all levels. Framebuffer comparison: 307200 pixels, maximum
channel error 1, zero pixels above tolerance 3. No screenshot is published
because the final wall view does not meaningfully illustrate the climb.

The first Xbox attempt rendered all 120 frames then returned RF_RANGE in the
unconditional post-run door fixture. That fixture explicitly requires two keys,
no events and one attached mover, which L1S2 does not universally satisfy.
It remains active for the L1S1 diagnostic and is not invoked for staged L1S2;
this does not implement or validate L1S2 doors. Full group simulation is open.

A separate vertical-input probe continued through the top boundary and repeatedly
re-entered while falling back into it. Authored approach, look-directed/controller
climbing, top-platform exit, sound playback and full player lifecycle still need
reconstruction/validation. The staged ascent is not a complete ladder or campaign
fidelity claim. Entity +148 remains correctly wired as vertical velocity.

L1S1 64-tick campaign crouch regression replay-20260910-003315 also passes
with the normal level restored, including the new climb timeline ring.


Look-directed climb regression (2026-09-10)

replay_climb_controls.py uses only move-Z and look-pitch commands, all within
[-1,1], with move-Y identically zero. Existing Xbox input.c maps left-stick up
to positive move-Z and right-stick up to positive pitch, so these commands are
reachable without adding a vertical movement button. This is source inspection
plus process-local command replay, not injected SDL events or a physical device
test. The authored movemodes.tbl climb translation selects eye X/Z and parent Y;
the recovered movement transform therefore supports the view-directed ascent.

The 180-tick scenario looks up for 80 ticks, moves forward from tick 80, then
lowers the view from tick 105 to leave sideways before the top boundary.
PC and stock-64-MiB XEMU replay-20260910-003732 pass with one entry/one exit,
5.204667568 units of retained climbing ascent and no vertical command. The
128-entry timeline wraps: validation compares the complete retained ring and
sorts its valid records by frame; it does not claim all 180 intermediate frames
are stored. The existing native body/stance/animation records also match.

Original call-site review: 430c70 queries 45cca0 on entity+3c. With a region,
42a100 accepts modes 1/2 or a changed previous-region pointer; 42d8b0 then
prevents re-entering mode 2. Without a region, mode 2 invokes 4280b0. The latter
restores the default movement descriptor and clears entity+148 vertical velocity.
This agrees with the current transition policy. No upward impulse or ledge
snap was found in this dispatch/exit path. That is a limited call-path finding,
not proof that no other player/collision behavior assists climbing elsewhere.
A look-held-at-the-top probe also re-enters repeatedly, as the parent-Y probe did.
Test the authored approach, geometry, view and any jump action before deciding
whether that observation represents missing traversal behavior or fixture setup.


Near-base climb approach (2026-09-10)

rf_scene_stage_climb centralizes replay-only setup on both targets. Mode 1 is
the previous center start; mode 2 offsets along negative region row 2 by
half-depth+1 and along row 1 by 1-half-height. This is an explicit staged
starting point, not reconstructed spawning or a guarantee of clearance.
L1S2's region is centered at (89.144348,-9.387844,-17.428513), size (1,11,1).
The approach begins on its positive-Z side near the lower floor. Existing
collision/gravity settles it; no position correction or motion bypass is added.

replay_climb_controls.py --approach uses 220 ticks: look up through 79,
quarter-strength strafe on 80..129, forward from 130, lower view from 175.
Move-Y stays zero. The corresponding Xbox --approach mode writes '2' to the
owned campaign-climb fixture flag and restores its prior contents afterward.
Native replay-20260910-004650 passes in 64 MiB XEMU: 73 retained climbing
ticks, 6.431220531 units ascent, one entry/one exit. The retained timeline
includes 36 prior walking ticks outside the region, covering 0.808611543 units
(computed from the native ring and matching PC). There are 9444 available pages
at the final presented frame. All existing native player records match PC.
The center-start 180-tick PC control case still passes unchanged. Neither
scenario proves an approach from the authored level player start or upper exit.

The full-speed first side approach crossed the narrow region before enough
forward/up motion developed; quarter-stick approach avoids that overshoot.
This changes test inputs only. A separate attempt from the negative-X side was
blocked before entry; do not infer universal walkability from the passing side.

inspect_climb_ground.py casts 196 downward static rays at four heights over
an X/Z grid; --movers repeats them with initial mover geometry. The batch probe
owns and closes geometry/archives before querying the retained collision world.
Both runs find the same sampled surfaces: lower floor -14.875 (face 3854),
upper surfaces -4.875 at offset positions (including faces 61,5884,5885), and
other sloped/intermediate faces. Near the center, rays from -2 hit the lower
floor. This does not locate every upper surface, establish capsule clearance,
or account for later mover motion. It narrows the next task to the intended
upper route/action rather than proving an error in region-exit policy.


Original jump and fall transition (2026-09-10)

inspect_jump.py executes fingerprinted RF.exe 4288b0 and its 4281a0 fall
transition in Unicorn. It also executes the original fsqrt initializer ending
at 433eb4 using the installed game.tbl $Max Entity Jump Height value 1.33.
The resulting float impulse is 5.105683326721191, sqrt(2*gravity*height).
This is original behavior evidence; no shared jump implementation is claimed.

The entry accepts mode 1, or mode 4 with actor flag 0x2000 clear. It rejects
crouch flag 0x400, the supplied parent-kind predicate returning 1, and mode 2
(climbing). Null entities return immediately. Mode 4 scales the impulse by
1.25f - (0.1f - frame_dt) * -4.200000286102295f. Negative existing vertical
velocity is then added; positive velocity is replaced. At 428935..428977,
x87 retains the scaled impulse through that addition, with only the final
binary32 store to entity+148. An intermediate float store produced a one-ULP
mismatch in the shipped-strength, 1/60-second, downward-velocity fixture.
The Python arithmetic oracle matches these tested inputs exactly; it is not
a general proof of binary64 equivalence to x87 extended arithmetic.

Accepted jumps set actor+810 bit 2 and physics+1a8 bit 1. Fall selects mode 3
or alternate mode 8 through 4339d0, falling back to descriptor 0 when the
requested descriptor is disabled. The orientation pointer becomes 73a858.
The routine requests class+120's sound through 434d00 with arguments
(sound,0,0,0,1.0f), passes its result to 505560, then stamps entity+7b4 with
6460f0's current time. The harness supplies the parent predicate 4290d0,
alternate-fall predicate 40a270, and sound lookup/playback boundaries; their
implementations and audible playback are outside this evidence.

All 6,144 cases pass exact whole-entity byte comparison, selected-descriptor
state and sound request checks: 16 modes, crouch/water flags, parent blocking,
alternate fall, three vertical velocities, two jump strengths, independent
1/30 and 1/60 timesteps, and enabled/disabled fall descriptors. There are 144
accepted cases. A separate null-entity check also passes. Generated evidence
is artifacts/jump-original/report.json. No Xbox build or live jump is tested
by this harness. Original action 3 in 4a6210 reaches this routine through
4a5c00 and another action gate; those gates and the campaign input wiring
still need reconstruction. The climb rejection rules out an assumed jump
boost while mode 2 is active, without proving the intended upper exit route.


Shared jump transition (2026-09-10)

rf_player_jump now reconstructs the resolved 4288b0/4281a0 transition in shared
C. Its state carries actor/physics flags, vertical velocity, movement/orientation
pointers and jump timestamp. Its input supplies the stable descriptor table,
identity matrix, configured impulse, frame dt, resolved parent/fall predicates,
class sound and current time. No allocation is performed. A sound callback runs
after the fall state is committed but before the jump timestamp is written;
the caller must implement the recovered lookup/playback boundary. Rejected
jumps preserve state, descriptor selection and callback silence. Null state is
a no-op. Missing accepted-path dependencies and nonfinite numeric inputs are
reported before mutations; these defensive checks are not original behavior.

verify_jump.py reruns the original fixtures and compares PC probe output plus
compiled NXDK function execution in Unicorn. All 6,144 cases match flags,
velocity bits, descriptor/fallback, orientation selection and timestamp.
NXDK callback checks cover the entire 24-byte committed state with the old
timestamp still present. Original execution reports now retain randomized
input/output flag words and timestamps for direct comparison. NXDK null-state
execution also passes. Output: artifacts/jump-verification.json. Long double
keeps the arithmetic unrounded until the final float store; MSVC uses binary64
for this type, and agreement is limited to the tested cases, not every float.

Both targets build. The five registered PC CTests pass, and the existing 128
climb-exit / 768 climb-entry PC/NXDK fixtures still pass. This change adds no
live input binding or XEMU gameplay evidence. Recover action gates, wire the
configured jump strength and body flags into campaign ownership, then validate
press/hold/release and landing through process-local PC/XEMU replays. The upper
climb exit and route from the authored spawn remain separate open questions.


Jump request gating (2026-09-10)

inspect_jump_dispatch.py executes 4a6210 with action 3, including 4a5c00,
4a5b30, 429f90, 427020 and the actual 51f220 keyboard-state read. Only object
lookup/type (426fc0,40a0e0,486c90), game-state query 434200 and the final jump
boundary are supplied. The branch rejects missing entities, nonzero 64ecb9
with game state 34, control kind 5 from player+b4, parent kind 4 from entity+200,
actor+810 bit 0, and nonzero keyboard-state byte 18868f4+0x29. Types are resolved
values, not object category codes. This does not name key 0x29 or recreate the
keyboard backend. The original branch ignores the third (edge) argument.

rf_player_jump_enabled mirrors those resolved conditions. All 864 combinations
of entity presence, override 0/1/2, game states 0/34, absent/nonblocking/blocking
control and parent kinds, actor bit, keyboard state, and edge argument pass.
Original dispatcher execution preserves the seeded player/entity bytes and
calls jump zero or one times. verify_jump_gate.py compares PC and compiled
NXDK results exactly, with input preservation and an NXDK null-gate check.
Both builds succeed; the 6,144 shared jump-transition cases still pass.
Generated reports: artifacts/jump-dispatch.json and jump-gate-verification.json.

The direct caller found at 430e3e is the action loop in 430c70. Its 436320
outer lock must allow dispatch, and 43d4f0 must return an active action. Review
of 43d4f0 shows 28-byte binding records at controls+c+action*28, a type field
at record+8, two keyboard bindings at +14/+16, and a mouse binding at +18.
Type 1 permits held-state queries as well as edge queries; other types omit
the held keyboard/mouse checks. Wheel direction has its own path. The query
also consults 444ac0,43d470 and 50b520. These are disassembly/decompiler findings,
not yet an executed input-query reconstruction. Recover jump's actual binding
record/type and those locks before choosing press/hold semantics for the port.
No live input, XEMU jumping or audio playback is added by this gate change.


Default jump binding and press consumption (2026-09-10)

inspect_jump_binding.py executes the original 43d060 initializer through the
fourth 43cfd0 registration, stopping at 43d0c5. The fourth record is action 3:
type 0, keyboard key 0x39, secondary key -1, mouse binding -1. The nearby JUMP
label path at 4386db reads the same 7cbe40 label global used by this registration.
The initializer's string assignment is supplied; actual record registration
and indexing execute. No original string or asset data is copied into source.

The harness then executes 43d4f0 and actual 51f140/51f220 keyboard helpers.
51f140 reads and clears the key's count at 18860e8+key*4 when 1886a1c enables
the keyboard backend; held bytes are separate at 18868f4+key. Only critical
section calls are replaced with process-local RET 4 stubs. No host input or
keyboard API is invoked. Menu (444ac0), reserved-key (43d470) and text-entry
(50b520) results are supplied as explicit case inputs.

All 48 combinations of those gates, pending counts 0/1/3 and held state pass.
An allowed pending count produces active=true and edge=true, then clears the
whole count. Holding alone produces no action for this type-0 binding. The
menu-plus-reserved branch drains the count without dispatch. Text-entry alone
suppresses the query without consuming a pending count; this fixture does not
establish whether the full backend later clears that count elsewhere.
Sequential press, hold, hold, release, repress yields true,false,false,false,true
without manually clearing the count between frames. The binding record remains
byte-identical. Report: artifacts/jump-binding.json. This proves the installed
default keyboard jump policy, not arbitrary rebound mouse/wheel behavior.

Raw 436320 consists of reading byte 637086 and returning it. 430c70 invokes
its action loop only when this byte is zero. The byte's lifecycle and the
menu/text-entry predicates are not reconstructed by this harness. Next connect
rising-edge PC/controller jump input, preserve the recovered jump-request gates,
and synchronize velocity/fall flags with the campaign body before live replays.
Neither holding jump to auto-repeat nor a climbing boost follows this evidence.


Jump support-update dependency (2026-09-10)

Live wiring review found the diagnostic landing block in scene.c immediately
calls rf_physics_static_land for mode 3 plus a nearby walkable ground hit.
Simply setting jump velocity/mode before this block could immediately clear
that velocity. The input binding is therefore still unconnected while the
original ordering is recovered; no invented grace period or velocity-only
landing rule has been added.

inspect_jump_support.py executes prepared 487f73..487fc9, immediately after
the actor's 41e4b0 update. Actor+810 bit 2 takes precedence: invoke 4281a0 and
skip 4a0840 support handling. Without that bit, actual 42a020 selects support
for modes 3/8 or kind-one actors with attachment+1380 equal to -1. Otherwise
mode 1 requires parent+200 == -1 and at least one of the preceding moved flag,
physics+1a8 bit 0x400000, or object+7c bit 8 (actual 4895d0). Other cases skip
both. The harness supplies kind-one and fall/support call boundaries; it does
not run world collision or the preceding actor update.

All 2,048 combinations of 16 movement modes and the seven binary conditions
pass exact branch/call checks, stack balance and seeded entity preservation.
Generated report: artifacts/jump-support-dispatch.json. Contrary to an early
working inference, airborne modes still use 4a0840 once actor flag 2 is clear;
this evidence does not justify replacing support with contact-only landing.

Raw 42a8e0 returns true only for object+7c bit 8 and nonnull owner+1430. In
41e4b0, the flag is cleared when that predicate is false (41ebb5 vicinity).
A separate owned-player update, containing entry 4aa6d0, consumes actor bit 2
at 4aadb5: it conditionally calls 4a9380(player,9), then clears the bit. These
are disassembly/decompiler findings, not yet full update-order execution proof.
Recover the ordering of this consumer, physics and input before connecting the
shared jump function to the diagnostic scene. This is required to avoid both
cancelled takeoff and a permanently suppressed support probe.


Shared support selector and frame-order recovery (2026-09-10)

rf_player_support_route now implements the resolved 487f73..487fc9 decision
in shared C, returning none, fall transition or support query. Jump flag 2 has
precedence; mode/kind/attachment, parent, movement and support/object flags
otherwise select the query. It does not mutate state or perform the operation.
verify_support_route.py matches all 2,048 original execution cases on PC and
compiled NXDK, with input preservation and a null-input check. Both targets
build; the 6,144 shared jump cases still pass. No live input is added yet.

Call-chain recovery for the ordinary local-player path:
- 433520 calls input update 4a6060 at 433633, then simulation 433260 at 43363b.
- 4a6060 dispatches 430c70 at 4a61c1/4a61d8, which can invoke action 3.
- 433260 calls the world/physics update 487a40 at 433326.
- 487a40 calls post-update 487e00 at 487c33; that contains the support selector.
- Later in 433260, 4a26d0 loops player updates through 4a2700 at 4a26e5.
- 4a2700's local-player branch calls 4aa6d0 at 4a2780; the latter consumes
  jump flag 2 at 4aadb5 after an optional 4a9380(player,9) request.

This is static call-path evidence from the fingerprinted binary and selected
Ghidra exports, corroborated by instruction inspection; the complete frame
loop and its state-dependent branches have not been executed in a harness.
It establishes the intended integration order for the ordinary local fixture:
input, movement/physics, support selection while the jump flag remains set,
then owned-player flag consumption. Other control modes, input locks, server
branches and exceptional early returns require their own lifecycle handling.
The port's diagnostic rendering/tick order still needs to be aligned and
validated by takeoff/hold/landing replay; the selector alone does not prove it.


Campaign support ordering integrated (2026-09-10)

The campaign path now advances actor_tick before selecting support handling.
actor_ground_query_state accepts an explicit body state, allowing a fresh query
at the updated position. The recovered selector receives current mode, actor
flags, physics flags and movement, with explicit fixture values kind-one=false,
attachment/parent=-1 and owned-player object bit 8. Support results commit to
the advanced body. A jump-flag route selects falling and skips support; flag 2
is consumed afterward, at the local-player phase. The optional first-person
motion request and full ownership/weapon lifecycle remain unimplemented.

The historical noncampaign diagnostic order is retained. Pre-tick ground rings
still describe their original diagnostic query; the new post-tick query is a
local temporary record used for campaign commits, not silently substituted
into the earlier trace. No input ABI or existing replay files change here.

Both PC climb scenarios pass: center ascent 5.204667568, approach ascent
6.431220531, each one entry/one exit. Stock-64-MiB XEMU approach replay
20260910-012038 matches PC's complete retained state and timeline checks across
220 ticks, with 9444 available pages at completion. Outside walking distance
is now 0.808611395. The 64-tick campaign crouch/move/stand replay also passes
on PC and XEMU 20260910-012117. All five registered CTests pass; both targets
build. These checks validate the reordered walking/climbing fixtures, not a
jump: jump input still needs connection and takeoff/hold/landing coverage.


Campaign jump connected (2026-09-10)

rf_scene_input now has a held jump word after crouch. PC maps Space; Xbox maps
A as a platform policy. The campaign update detects rising edges after region
transitions and immediate crouch action (ordering corrected below), then applies rf_player_jump_enabled and
rf_player_jump to the body velocity/physics flags, actor flags and movement
mode. Existing post-physics support consumes jump flag 2. Resolved ownership,
parent/input-lock and alternate-fall values remain ordinary-player fixture
values. Strength uses the verified installed height 1.33 and existing gravity
9.8; full game.tbl configuration, retained timestamp ownership and class sound
resolution/playback are still open. The sound callback counts pending requests
without claiming to resolve or play them.

PC interactive campaign mode is available through rf_pc_play --campaign
Installed_Game. The legacy interactive route remains selectable without that
option. Xbox jump applies when campaign-spawn.flag selects campaign mode;
harness cleanup restores prior flags and does not change the normal disc mode.
The physical input paths have not been operated by the harness. As currently
polled, a very short PC press/release wholly between ticks may be missed; the
original keyboard press-count queue remains a fidelity item.

Replays accept legacy raw 24-byte records (jump zero) or an RFI2 header followed
by uint32 record size 28 and 28-byte records. The common header reader rejects
empty, partial, wrong-size and over-60000-record files; eight malformed PC
fixtures were rejected. PC zero-initializes expanded records; Xbox streams them
into a cleared input object. The legacy 448-word input ring retains its exact
layout. New jump diagnostics add four counters and a 128x8-word ring: frame,
held, edge, accepted, mode, position Y, velocity Y, actor flags. XEMU compares
all 1024 timeline words and counters with PC for every campaign replay.

replay_player_jump.py covers 128 ticks. A press at 8 while initially airborne
is rejected. A grounded press at 24 reaches 1.329908729 units above takeoff,
lands by 90, and does not repeat while held through 95. Release at 96 and
repress at 100 produces a second accepted jump. XEMU replay-20260910-012451
passes in 64 MiB with exact PC state/timeline comparison and 10889 available
pages. Legacy 64-tick stance replay also passes natively at 20260910-012603;
legacy PC approach/stance and five CTests pass. The later PC --campaign CLI
addition was rebuilt and the jump replay rerun successfully; its interactive
window was not launched. No complete original-game jump-arc comparison is
claimed by matching the two reconstructed platforms.


Jump restrictions and immediate stance order (2026-09-10)

Review of 430c70 shows the eligible ordinary-player stance query/transition
before LAB_430e19 and the action loop. The first jump integration placed its
request before actor_player_stance's immediate crouch effect. This is corrected:
resolve stance, request crouch motion if needed, then process the jump edge.
Thus simultaneous crouch press+jump is rejected, while a clear stand release
plus a fresh jump edge on the same tick succeeds. This is the ordinary stance
path; full toggle/lock/ownership policies remain explicit fixture assumptions.

replay_jump_restrictions.py covers three variants:
- Default: crouch at 8, reject jump at 24, stand at 40 while jump remains held
  without dispatching, release at 48 and accept a new press at 52, then land.
- --simultaneous: crouch and jump together at 24 are rejected; release crouch
  and freshly press jump at 40 succeeds, with landing by 110.
- --climb: staged L1S2 center/look-directed route; reject mode-2 jump edges at
  90 and 120, then accept a fresh edge at 139 after region exit restores mode 1.
  The region timeline records exit before jump changes the movement to mode 3.

All PC variants pass after the ordering correction, as do existing basic jump
and immediate stance replays. The 128-tick simultaneous scenario passes in
stock-64-MiB XEMU 20260910-013000 with exact PC state and jump-ring matching.
The first native climb run 20260910-012851 passed before this correction and
is kept as earlier evidence rather than claimed as validation of the final code.
Climb exit acceptance is a consequence of restored mode 1 before support, not
proof of a grounded upper-platform jump, original full-route parity or an
intentional climb boost. Test the authored exit geometry and route separately.

Final-code climb restriction replay 20260910-013030 also passes in stock-64-MiB
XEMU: two rejected climbing presses and one accepted exit-frame press, with
exact PC state and jump/climb timeline matching over 180 ticks.


Authored jump-height loading (2026-09-10)

rf_game_jump_height_read finds the unquoted $Max Entity Jump Height field in
game.tbl using the existing bounded lexer/decimal reader. It skips comments
and quoted labels, rejects duplicate/missing/negative/nonfinite/malformed values,
and preserves output on failure. rf_game_jump_height_load uses one scratch
allocation capped by the caller's budget; campaign setup allows 65536 bytes.
The installed table is 2424 bytes. No table text or archive pointer is retained.
This is a narrow reader for the recovered 433dd0/433e94 field, not a complete
reconstruction of the original table grammar or every game configuration field.

Campaign setup computes the jump impulse once from the loaded height and the
same installed 9.8f gravity currently used by actor_tick. Jump requests reuse
that impulse. The previous literal 1.33 in the live jump path is removed; gravity
and sound/timestamp ownership still need their complete lifecycle integration.
The decimal reader avoids NXDK's assertion-only strtod/strtof implementations.

verify_jump_height.py passes 24 cases on PC and compiled NXDK, including the
installed table, altered decimal/exponent values, comments/case, quoted labels,
empty/missing/duplicate fields, malformed values and output preservation.
The PC archive wrapper passes exact scratch budget and one-byte-short rejection.
The shared reader is executed from compiled NXDK in Unicorn without substituting
its parsing logic. Reports include the executable fingerprints.

A separate ignored asset overlay at local/jump-height-override copied tables.vpp
and changed only the four-byte authored value 1.33 to 2.50. Other VPPs are read
through hard links; no installed original was modified. A 128-tick PC replay
with one grounded press reached 2.499972224 units and landed, proving the live
path consumes the changed value. Its report is artifacts/jump-height-override.
The standard 1.33 jump replay still passes on PC and stock-64-MiB XEMU
20260910-013412 with exact retained state/timeline matching. Altered-height
native Xbox execution and original full-arc comparison remain untested.


Shared gravity state and original runtime setter (2026-09-10)

The original scalar 5a00dc starts at binary32 9.8. Level setup pushes that same
value at 435aeb and calls 4a0e20 at 435af0. game.tbl only contains a comment
about Entity Gravity in the installed copy; no active field was found there.
4a0e20 writes the scalar and uses actual 42d840 to set vector 7c7058 to
(0,-gravity,0). Getter 4a0e50 returns the scalar. Runtime trampoline 4bcc00
reads object+2b8 and invokes the same setter. It is referenced from the table
at 589b40; its full event construction/dispatch has not been reconstructed.
Ghidra lacked a function at 4bcc07, so this path was inspected from raw x86
and executed directly instead of treating the failed export as evidence.

rf_physics_gravity_set now owns the scalar/vector update in shared C. It accepts
finite signed gravity, preserves signed zero in the negated Y component, and
rejects nonfinite input before mutation (a defensive API restriction). Scene
campaign setup resets one shared gravity state to 9.8; falling proposals and
initial jump-strength computation use that scalar instead of separate literals.
The setter deliberately does not recompute the separately initialized jump
impulse. Connecting runtime gravity events must preserve this distinction.

verify_gravity.py executes both original 4a0e20 and 4bcc00 for 1,024 finite bit
patterns, including +/-zero, subnormals, +/-9.8 and maximum finite values. It
compares full scalar/vector output and verifies 62f2c8 stays unchanged. PC and
compiled NXDK match all cases; three nonfinite rejection cases also preserve
output. Both targets build and the ordinary PC jump replay remains unchanged.
The fixture does not execute event registration, active-level event dispatch
or altered-gravity campaign traversal. Report: artifacts/gravity-verification.json.

Stock-64-MiB XEMU jump replay 20260910-013918 passes with exact PC state and
jump timeline matching after the shared-gravity integration.


Set_Gravity action recovery (2026-09-10)
--------------------------------------

Type 44 uses constructor 4beb20, 700-byte storage and vtable 589b3c.
The constructor leaves payload +2b8 untouched; loader initialization remains
required. Its virtual on handler 4bcc00 applies that payload using 4a0e20.
Actual common off handler 4b9f80 returns without altering gravity for this type.
Neither action changes event storage or the existing jump impulse.

rf_event_gravity_action provides these effects for the shared callback layer.
Action 2 leaves gravity alone so the caller can handle link propagation; invalid
actions and nonfinite on values are defensive errors. This API does not own
event registration, scheduling, links or the scene gravity state.

verify_gravity_event.py executes the actual original virtual actions for 1,024
finite payload bit patterns each, including campaign values 3, 4 and 9.8,
signed zero and negative values. All 2,048 effects match PC and compiled NXDK;
six extra shared API checks pass. verify_event_construction.py now covers
type 44 on zero/A5 storage (eight total constructor cases). Existing common
activation/timer regression remains green at 3,922 PC/NXDK cases. Reports:
artifacts/gravity-event-verification.json and event-construction-verification.json.
No new native XEMU or authored campaign event traversal is claimed here.


Authored gravity triggers and startup order (2026-09-10)
----------------------------------------------------

The inventory has four Set_Gravity events, each with one incoming Trigger Auto
and no outgoing links: L17S1 20046->20045 (4.0), L17S2 18651->18652 (3.0),
L17S3 18758->18759 (4.0), L18S1 10478->10479 (9.8). Their trigger flag bytes
are [0,0,0,1,0]; the auto runtime flag is bit 8. The event header bytes vary,
so they must not be treated as a substitute for the incoming trigger.

verify_gravity_trigger.py executes unmodified 4c01b0 -> 4c0220 -> 4c0320 ->
4b6760/4b6800 -> 4b8b70 -> virtual 4bcc00 -> 4a0e20, plus empty event-link
propagation and trigger cooldown, with no intercepted calls. Eight cases
cover all four payloads with/without runtime disabled bit 16. Objects and
handles are synthetically registered and runtime fields explicitly prepared;
this does not prove the loader or shared C/NXDK dispatch chain. The original
event object base is derived-event-base +4, an important handle lookup detail.

Accepted auto activation records actor -1 and trigger source handle, increments
the trigger count, sets cooldown deadline now+30000, records float-clock bits
and sets flag 64. Gravity changes during dispatch, before that cooldown is set.
The unchanged jump impulse remains verified. Report:
artifacts/gravity-trigger-verification.json.

Static call-site/decompiler evidence: 4316a0 calls auto sweep at 4316d3; its
only direct CALL found in executable sections is 4360d7 inside 435df0. That
function performs level startup, calls 4bd890 before 4316a0,
and ends by running levelstart.vcs. Direct export confirms 4bd890 is an empty
RET stub in this build, not an event post-load pass. Actual trigger/event link
conversion occurs earlier within level loader 460820. Exports 4316d3.c.txt and 4360d7.c.txt
include containing-function annotations. This establishes the intended startup
placement by static inspection, not full startup execution. The shared runtime
must register and resolve events before the sweep; do not apply gravity by
level filename, event-header byte, proximity or an assumed 30-second delay.

The gravity-trigger fixture now begins with authored UID links and executes
4611a1..461231 unchanged to resolve them through the synthetic original object
list before running the complete auto activation chain. All eight cases pass.
This removes the prior pre-resolved-link assumption but still supplies object
registration and trigger/event field initialization. No C/NXDK runtime wiring
is established by this original-code fixture.


Shared auto-trigger activation (2026-09-10)
----------------------------------------

rf_auto_trigger_fire and rf_auto_trigger_state in rf/event.h now reconstruct
the single-player auto sweep selection and activation bookkeeping. Auto flag
8 is required and disabled flag 16 rejects. Existing fired flag 64, cooldown
and activation limits do not prevent this startup sweep. Global/script
eligibility is supplied by the caller, which must traverse trigger-list order.
The callback receives actor UINT32_MAX and suppress-movers 0, sees old state
and may use the trigger handle to dispatch ordered links. Callback mutation
or releasing the state is outside this API contract. After dispatch, count
wraps as a 32-bit word; a positive cooldown updates the timer; the raw game
float-clock bits are recorded; flag 64 is set. Invalid timer inputs are
rejected before dispatch as a defensive shared API restriction.

verify_auto_trigger.py compares unchanged original 4c01b0/4c0220 and timer
helpers to PC and compiled NXDK for 360 cases. Only 4c0320 link dispatch is
intercepted, recording old state and actor/suppression arguments. Cases cover
active/disabled/non-auto/already-fired flags, count wrap, negative/zero/positive
cooldowns and timer wrap boundaries. Report: artifacts/auto-trigger-verification.json.
PC and NXDK builds pass, as do existing event activation (3,922 cases), gravity
action (2,048 original cases) and five CTest tests. This shared routine remains
unconnected to the scene registry/startup path; no native XEMU run is claimed.


Authored auto-trigger initialization (2026-09-10)
----------------------------------------------

rf_auto_trigger_init accepts the decoded rf_level_trigger, a registered handle
and current timer clock. It maps the five exact-equals-one flag bytes to
1/2/4/8/128, the box-only flag to 32, and nonzero tail_flag to disabled bit 16.
It initializes count to zero, deadline to now, and activation time to the raw
float -1 bits, following 4bf970. Header_byte is not a disabled flag. Shape
creation, activation limits and script ownership remain outside this compact
auto-trigger state.

The v180 loader multiplies binary32 timing by 1000 then truncates (46561a).
An integer significand/exponent conversion now reproduces that exact product
without ambient x87 precision dependence: authored 0.01f gives 9 ms, not 10.
Nonfinite/out-of-range timing is rejected before output mutation. Positive
cooldowns are limited to RF_TIMER_PERIOD by the shared timer contract.

verify_auto_trigger_init.py executes prepared original timing, flag packing,
constructor bookkeeping and disable blocks for all 2,367 installed triggers
and 243 ternary flag combinations. PC matches all 2,610 cases; compiled NXDK
matches each under 24-, 53- and 64-bit x87 precision. Full original parser
and object factory are not executed by this fixture. Report:
artifacts/auto-trigger-init-verification.json. PC/NXDK builds, 360 activation
cases and five CTest checks pass. Registry/startup scene wiring remains open.


Shared event type lookup (2026-09-10)
-----------------------------------

rf_event_type_id implements original 4bd700 against the 90-name table at
5a1a3c..5a1ba4. ASCII case variants match; unknown names return -1. NULL
returns -1 defensively. Names must be NUL-terminated. Type recognition makes
no claim about runtime action availability. The table is embedded in shared C
so Xbox lookup needs no original executable at runtime.

verify_event_types.py runs the original lookup and actual comparison helper
without interception, checking canonical/lower/upper names, added whitespace
and suffixes, shortened names and invalid input. All 544 cases match PC and
compiled NXDK. Both builds pass. Report: artifacts/event-type-verification.json.
The scene still owns movement regions only; event/trigger ownership, registry
insertion and startup dispatch are the next integration work.


Campaign event ownership and registration (2026-09-10)
--------------------------------------------------

rf_runtime_events_open now retains decoded authored records/links and a
separate array of common runtime objects. Each object has kind 6, a registry
handle, original type ID, authored delay and inactive deadline. Generic event
creation clears flags; actor/source/mode are deterministically zero here
instead of retaining original uninitialized bytes. Type-specific construction
and action payload ownership beyond the retained raw record remain open.

The caller owns the registry. Common objects are registered in authored order;
this is not a claim of original whole-world handle parity because other object
families are not yet registered. Budget preflight and record validation precede
insertions. Close removes handles before freeing objects and records, and is
repeatable. Level source archives need not stay open after successful loading.

Campaign scene setup now opens this owner with a 1-MiB cap and retains it for
the scene stream. Cleanup closes it on normal and error paths. Decoded events
and common objects share the cap; the registry adds 12,300 bytes on Xbox.
rf_scene_campaign_events / PC CAMPAIGN_EVENTS report count and both byte costs.
No trigger/event actions are dispatched by this integration yet.

verify_runtime_events.py passes all 4,446 installed events across 93 levels,
checking live handle lookup, removal, repeated close, exact-budget reopen,
one-byte-short rejection, and record access after archive close. Peak owner
cost is 219,436 bytes. The 128-frame jump replay passes on PC and native
64-MiB XEMU (replay-20260910-061740), with exact existing state/timeline matching
and 184 registered L1S1 events reported by guest memory. Both builds and five
CTest checks pass. Reports are artifacts/runtime-events-verification.json and
artifacts/xemu/replay-20260910-061740/report.json. No new visual behavior.


Campaign trigger ownership (2026-09-10)
-------------------------------------

rf_runtime_triggers_open retains decoded trigger records and ordered raw UID
links, initializes shared auto-trigger state, and inserts kind-5 objects into
the caller registry. It validates and initializes every record before inserting
any handles. Registered handles are copied into the activation state. Budget
checks include both owners and allocations; registry overhead is separate.
Close removes trigger handles before freeing storage and leaves event handles
intact. Full collision shape creation and script ownership remain open.

Campaign scene setup now loads triggers into the same registry as events;
cleanup closes triggers first. rf_scene_campaign_triggers / CAMPAIGN_TRIGGERS
expose count and bytes, and the native replay harness reads these from guest
memory. A 1-MiB cap currently applies to each owner; actual trigger peak is
58,680 bytes across the installed levels. Neither owner resolves links or
dispatches startup actions yet, so visible behavior remains unchanged.

verify_runtime_triggers.py passes 2,367 records across 93 levels with event
objects registered concurrently. It checks trigger initialization, handle
lookup/removal, event-handle survival, repeated close, exact-budget reopen,
one-byte-short rejection and record access after archive close. Stock-64-MiB
XEMU replay-20260910-062110 passes with the combined owners and exact existing
PC replay state. Both builds and five CTest checks pass. Reports:
artifacts/runtime-triggers-verification.json and the native replay report.


Runtime trigger link resolution (2026-09-10)
------------------------------------------

Each owned runtime trigger now has a separate target array, leaving authored
UID links intact. The owner budget includes these arrays; peak trigger storage
is now 60,736 bytes. rf_runtime_triggers_resolve rebuilds targets through the
verified rf_level_link_resolve object-first/key-fallback rules using caller
ordered views. Missing targets retain raw UID and kind 0. Re-resolution can
therefore include additional registered families later without reloading assets.

Campaign setup supplies current event-then-trigger registration views. This
is an incomplete world registry: no entity/mover-key views or entity backlinks
yet, and no claim that fixture handles/order equal the complete original world.
Temporary scene view storage is freed immediately after resolution and bounded
by registry capacity (at most 12,288 bytes). Startup dispatch is still separate.

verify_runtime_trigger_links.py checks all 4,471 authored trigger links across
93 levels against the registered family inventories. 2,809 resolve; 1,662 stay
explicitly unresolved. All four Set_Gravity links resolve to their corresponding
event handles. Owner/budget tests pass with the new arrays. Campaign memory
diagnostics include total/resolved/unresolved counts and an ordered target hash
over each raw UID and resulting value/kind/index. Stock-64-MiB XEMU jump replay
20260910-062500 matches these diagnostics and all existing PC state. Both builds
and five CTest checks pass. No new visual behavior or action execution claimed.
Reports: artifacts/runtime-trigger-links-verification.json and native replay.


Connected partial startup dispatch (2026-09-10)
--------------------------------------------

rf_runtime_startup_events now traverses active auto triggers in owned order,
looks up resolved targets in the registry, activates common event state, and
applies type-44 gravity through the recovered shared handler. Scene startup
calls it after player/jump setup and before animation streaming; jump impulse
is retained when gravity changes. Timer and float-clock inputs are currently
relative scene startup zero. Global mode remains the single-player fixture.

Unsupported effects are explicit report fields: triggers fired, events reached,
gravity actions, unsupported actions, unresolved targets, non-event targets,
nonempty script gates, undispatched outgoing event links and delayed events.
The report is not a success count for those pending effects. Nonempty script
eligibility is not guessed. Propagation and delayed ticking remain open; event
records preserve their scheduled state for future integration. Successful
known actions are not rolled back if a later action errors.

CAMPAIGN_STARTUP / rf_scene_startup_events plus rf_scene_startup_gravity expose
nine counters and the four gravity words. verify_runtime_startup.py checks the
partial dispatcher against all 93 authored graphs, including all four gravity
events. This is integration coverage of documented partial behavior, not full
original campaign parity. Native 64-MiB XEMU replay-20260910-062922 matches PC
startup diagnostics and existing jump state. Both builds and five CTest tests
pass. Native late-level gravity validation remains open.

PC headless spawn replay now accepts RF_REPLAY_ARCHIVE alongside existing
RF_REPLAY_LEVEL for testing levels3.vpp maps. replay_gravity_campaign.py renders
16 idle frames per gravity level: L17S1, L17S2 and L17S3 pass with gravity
4, 3 and 4. The initial L18S1 run failed before startup at rf_scene_world_open_retained,
RF_NOT_FOUND (-3), SCENE_STAGE 0 10; it was previously mislabeled RF_FORMAT.
That initial report was INCOMPLETE; the missing-mover fix below supersedes it. Assets
were not changed. Captures are empty spawn areas, not selected showcase images.
Reports: artifacts/runtime-startup-verification.json,
artifacts/gravity-campaign-live/report.json and the native replay report.


Levels without movers (2026-09-10)
--------------------------------

L18S1 has no section 0x2000; only 68 of the 93 inventoried levels have mover
sections. rf_geometry_movers_open correctly returns RF_NOT_FOUND for absence,
but rf_scene_world_open_retained incorrectly treated that optional section as
mandatory. It now accepts only RF_NOT_FOUND as an empty mover owner, accounts
for the empty owner size, and continues loading static world geometry. This
also prevents unsigned underflow in retained geometry byte accounting. Other
errors still propagate; present malformed sections are not silently ignored.

replay_gravity_campaign.py now passes all four 16-frame rendered PC checks,
including L18S1 with gravity restored to 9.8 through its authored auto trigger.
The three preceding maps remain 4/3/4. PC and NXDK builds pass. Subsequent native
64-MiB XEMU checks pass for all four maps after accepting optional missing groups
and movers in Xbox preflight; see XEMU-MEMORY.md for reports and scope.
The resulting L18S1 capture is an empty tunnel spawn,
so it is not a selected showcase screenshot.


Ordered event propagation loop (2026-09-10)
-----------------------------------------

rf_event_links_propagate reconstructs original 4b8b00 with ordered generic
on/off callbacks. The unchanged original 40a480/40a490 array helpers show that
both count and backing pointer are reread after each callback. Mode low byte
must equal exactly 1 for on (4b65c0); other values select off (4b6640), passing
suppress_movers=1. Source/actor arguments remain unchanged at this boundary.
The shared callback may replace count/backing storage or modify later handles.
Storage validity/lifetime is caller-owned. Counts above INT32_MAX and missing
nonempty backing storage are defensively rejected; invalid signed original
array lengths are outside the equivalence domain.

verify_event_propagation.py executes unchanged original loop/array access and
intercepts only generic target dispatch. All 700 cases match both PC probe and
compiled NXDK code: empty/single/multiple links, low-byte mode behavior, shrink,
growth, backing replacement and modification of the next handle. Target lookup,
recursive callbacks and scene wiring are excluded. Existing 3922 common event
activation cases and all five CTests pass; both builds pass. Report is at
artifacts/event-propagation-verification.json. No native XEMU run is needed to
claim this isolated compiled-code check; it is not a live campaign claim.

Next dispatch evidence from static original disassembly: 4b65c0 first resolves
an event via 4b6800, then trigger (4c08e0), mover (46afa0), and another object
family (45afe0). 4b6640 uses the same priority, skips the mover branch when
suppressed, and its event branch at 4b6652..4b665c pushes actor twice: both
source and actor become the input actor on off activation. This behavior still
needs execution verification before connecting generic target dispatch. Do not
replace it with assumed symmetric on/off argument handling. The new loop is
not yet called by startup; outgoing event links remain explicitly pending.


Original generic target routing execution (2026-09-10)
----------------------------------------------------

verify_event_target_dispatch.py supplies synthetic registrations and executes
original 4b65c0/4b6640, typed lookups 4b6800/4c08e0/46afa0 and real 40a0e0,
plus auxiliary linked-list lookup 45afe0. Event activation 4b8b70, mover action
46aba0/46b5b0 and auxiliary action 45b040/45b010 are intercepted at their entry
boundaries. Trigger bit operations 4c0200/4c0210 execute without interception.
All 720 fixtures pass: absent/unhandled/event/trigger/mover object kinds, an
optional colliding auxiliary identifier, on/off, suppression low-byte values
0/1/256/257, valid/stale/out-of-range handles and three trigger flag patterns.

Confirmed routing priority is event, trigger, mover, auxiliary. On events pass
(source, actor, 1); off events pass (actor, actor, 0), preserving the original
asymmetry. Trigger on clears only flag 16; off sets only flag 16. On movers
receive (registered_handle, source, actor); off movers receive their handle
only. Off suppression tests the low byte and skips mover lookup entirely,
but still attempts the auxiliary lookup. Typed event/trigger matches suppress
that fallback, as does a nonsuppressed mover match. Stale or out-of-range
object handles can still match an auxiliary identifier in the separate list.

Report: artifacts/event-target-dispatch-verification.json includes every case
and observed effect trace. This is original-code routing evidence, not shared
C/NXDK or live campaign equivalence. It supersedes the previous static-only
uncertainty about off arguments. Event links still need owned runtime target
resolution and recursive dispatch integration; startup pending counters remain.


Owned runtime event targets (2026-09-10)
--------------------------------------

rf_runtime_event now owns a target array beside the common state allocation,
with one rf_level_link_target per authored link. Open initially retains raw UID,
kind zero and index UINT32_MAX. The owner budget includes the arrays and pointer
fields; close frees them with the event objects after removing handles.
rf_runtime_events_resolve applies the existing ordered object/key resolver.
Scene loading resolves both trigger and event links using its current event-
then-trigger registry view. Other families and mover keys remain unregistered;
this view does not claim original whole-world handle order. Original authored
links remain available for later resolution when those families are registered.

verify_runtime_event_links.py passes all 4,901 links across 93 levels: 1,964
resolve and 2,937 remain unresolved. Event ownership tests pass for all 4,446
records, including exact budget, one-byte-short rejection, repeated close and
raw/runtime target access after archive close. Peak owner size is 222,560 bytes.
Existing trigger-link checks and five CTests pass; PC and NXDK builds pass.

CAMPAIGN_EVENT_LINKS and rf_scene_campaign_event_links expose total/resolved/
unresolved plus ordered FNV hash over raw UID, value, kind and index. Native
64-MiB XEMU replay-20260910-065321 passes, with L1S1 [199,47,152,1665296552]
matching PC. Its 184 events occupy 222,560 owner bytes plus the 12,300-byte
registry, and 10,820 physical pages are available at replay completion. This
is a short startup replay, not peak full-campaign memory evidence. The new
resolved arrays are not yet dispatched recursively; pending startup counters
remain unchanged. No new screenshot is warranted by ownership/resolution.


Recursive startup dispatch integration (2026-09-10)
-------------------------------------------------

Startup now follows owned resolved event targets depth-first, preserving the
source/actor passed into propagation. Generic event on keeps source; off uses
actor for both fields, matching the verified original dispatcher. Kind-5 target
on clears disabled bit 16 and off sets it. The auto sweep reads flags when each
trigger is reached, so an earlier chain can enable a later auto trigger. Only
flag mutation is permitted inside the auto-fire callback; its other bookkeeping
fields and lifetime remain protected by the existing callback contract.

Each recursive activation gets a separate context, so a child cannot replace
its parent's event pointer or corrupt later sibling traversal. Callback failure
is propagated independently of the common activation function's return value.
Immediate recursion is capped at 64 event activations with RF_RANGE; this is
an explicit defensive implementation limit, not recovered original behavior.
Effects before failure remain applied. Acyclic deeper graphs and dynamic link
mutation are not supported by this startup integration yet. The existing
rf_event_links_propagate loop remains the separately verified mutable-list
primitive; startup currently walks the owned fixed target arrays directly.

The partial graph oracle passes all 93 authored levels. Dedicated PC fixtures
verify sibling ordering (last gravity wins), enabling a later disabled auto
trigger, and a cyclic graph returning RF_RANGE after 64 activations. Existing
3922 PC/NXDK common activation cases and five CTests pass. Both builds pass.
Native 64-MiB XEMU replay-20260910-065717 (L1S1) and -065759 (L17S1) pass with
startup counters, gravity and player state matching PC. L17S1 activates 11
events and applies gravity 4. L1S1 activates nine events and retains gravity 9.8.

Other event actions still increment unsupported_actions; their effects are not
implemented, so propagation after those placeholders is only partial behavior.
Delayed events are scheduled but still not ticked. Missing targets are counted
in unresolved_targets; non-event/trigger targets in other_targets. pending_links
is now zero for completed traversal, including links reported unresolved through
those counters. Auxiliary fallback and mover actions remain unconnected here.
No full original campaign-chain equivalence, gameplay completion or visual
change is claimed from these short startup replays.


Invert event action (2026-09-10)
-------------------------------

Type 3 maps through the generic constructor case 4b75ca -> 4bee70, using
vtable 589c9c. Its base on/off dispatch entries 4b9070/4b9f80 select 4b9930
and 4ba330 respectively. These functions walk links themselves; the common
propagation predicate excludes type 3 to avoid duplicate traversal.
On sends generic off(target, current event source, -1, suppress_movers=0).
Off sends generic on(target, current event source, -1). The source field is
reread on each link; the stored actor is ignored. Generic off event routing
then changes both downstream source and actor to -1, as previously verified.
This differs from ordinary propagation off, which suppresses mover dispatch.

verify_invert_event.py executes unchanged common activation, real base virtual
dispatch, both Invert actions and array helpers, intercepting generic target
effects. All 192 cases pass, including disabled events, low-byte mode handling,
empty/multiple links and source mutation after each callback. That report is
original-code evidence, not a 192-case C/NXDK comparison.

The shared startup action now implements the recovered event/trigger routing.
Mover and auxiliary effects remain unsupported; the original suppression
exception is documented in the action so future target support must preserve it.
Five shared recursion fixtures pass, including single inversion preserving
gravity and double inversion applying gravity with downstream source -1.
All 93 authored startup graph checks and five CTests pass. PC/NXDK builds pass.
Native 64-MiB XEMU replay-20260910-070306 passes L1S1; its authored Invert UID
9956 is reached at startup, linking to UID 9957. Startup diagnostics and player
state match PC. This is partial campaign integration, not complete event effects
or full original recursive-chain equivalence. No new visual is claimed.


Delayed runtime event update (2026-09-10)
---------------------------------------

Original 433260 calls the ordered virtual event update 4b6720 at 4333ea,
after its physics update call 487a40 at 433326. Within 4b6720, events are
visited through array 856470 and vtable +12. Both base Invert and Set_Gravity
use 4b8ce0; its common delayed prefix was already verified, and its subsequent
special-case switch starts at type 52, excluding types 3 and 44. This supports
using the common tick for these types. Other virtual updates are not inferred.

rf_runtime_events_tick now visits owned events in registration order and runs
the recovered delayed prefix for types 3/44. Unsupported scheduled types remain
intact and counted. Expiring events use the same recursive action dispatcher
as startup. Reports describe one call; scene diagnostics accumulate effects.
Three focused PC fixtures test modes 0/1/2 before/at/after expiry, scheduling a
later event for the next millisecond, no duplicate expiry, preservation of a
type-50 pending deadline, and the original lack of a disabled-flag gate at tick.
Mode 2 executes the on action but propagates off, retaining the recovered quirk.

The scene runs this after each simulated physics step. Its clock is explicitly
an owned 60-Hz clock: floor((frame+1)*1000/60) milliseconds, wrapped using the
existing timer period with positive endpoint retained. This is not recovered
wall-clock, pause or whole-frame ordering parity. The final rendered frame has
no following physics step and therefore no additional event update. Clock and
report state are exposed through CAMPAIGN_EVENT_TICKS/rf_scene_event_ticks:
tick count, last milliseconds, current unsupported pending count, then nine
cumulative action-report words. No additional dynamic allocation is required.

All 93 startup checks, five CTests and existing 3922 PC/NXDK common activation
cases pass. PC/NXDK builds pass. Native stock-64-MiB XEMU replay-20260910-070755
passes with 15 updates through 250 ms, matching PC diagnostics and player state.
No currently reachable authored startup schedules a delayed Invert/Set_Gravity,
so this native run proves clock/update integration, not an authored expiry.
The focused fixtures supply expiry coverage. Remaining event types, their
custom updates, full clock integration and native expiry fixtures remain open.


Authored Delay event expiry (2026-09-10)
--------------------------------------

Delay type 48 maps to the generic constructor (4b75ca -> 4bee70), using base
vtable 589c9c. Base on switch 4b9070 selects the return at 4b9099; base off
switch 4b9f80 selects the return at 4ba008. Both actions are no-ops. Its complete
4b8ce0 virtual tick executes the common timer/propagation prefix and no special
tail (the tail starts at type 52). Startup now recognizes the no-op action and
runtime ticking includes type 48 alongside types 3 and 44.

verify_delay_event.py executes unchanged original common activation, actual
virtual actions, complete tick and link propagation. Generic target effects
are intercepted. All 72 cases pass for three delays, multiple link counts,
raw modes and source handles, with before/exact/after expiry checks and no
repeat dispatch. Shared delayed fixtures now include a Delay -> Set_Gravity
chain; four delayed and five recursion fixtures pass, along with all 93
startup graphs and five CTests. Both builds pass.

L1S1 startup schedules Delay UID 9495 for five seconds. Its sole link is UID
9466, type Explode, named Explode with charge_explode text and value 0.75.
A 320-frame idle replay crosses that deadline. Native 64-MiB XEMU report
artifacts/xemu/replay-20260910-071141/report.json is PASS and matches PC:
CAMPAIGN_EVENT_TICKS = [319,5316,4,0,2,0,1,0,0,0,0,0]. Thus 319 physics/event
updates reach 5316 ms, four unsupported deadlines remain, and the delayed
chain activates two events (Delay plus Explode), with one unsupported action.
The replay input is artifacts/delay-campaign-live/inputs.bin, consisting of
320 RFI2 idle records. Run it with xemu_replay_check.py --campaign-spawn.

This establishes native authored expiry and downstream activation with the
owned fixed-step clock. The Explode effect remains unimplemented, so it does
not establish an explosion, audio, damage, particles or Geo-Mod behavior. No
new screenshot is warranted. Original whole-frame wall-clock parity remains
separate from the existing verified timer and this replay integration.


Explode event request boundary (2026-09-10)
-----------------------------------------

Type 10 factory branch 4b6b4b allocates 0x2d0 and calls constructor 4be570.
That constructor installs vtable 58998c; on is 4bae20, off is base 4b9f80,
and tick is the common 4b8ce0. Original execution verifies off is a no-op.
On optionally calls 467020 when byte +2b8 equals exactly 1 and room word +4
is nonzero, then always calls 436490. Request arguments in stack order are:

467020(+2cc, -1, room, position_pointer, unit_x_pointer, 0, 1)
436490(+2c4, room, 0, position_pointer, +2cc, +2c8, 0)

Position lives at event +40; temporary unit_x is (1,0,0). Negative effect
index, zero room and zero parameter values still reach 436490. Names of the
numeric request fields are not inferred solely from decompiler float types.
verify_explode_event.py passes 288 cases over flags, rooms, effect indices,
parameter words and on/off, checking request order, raw arguments and unchanged
event storage. Complete original action/vector/room helpers execute; the two
request consumers are intercepted. This proves the event boundary, not their
effects or shared C/NXDK equivalence.

Static loader/factory trace: 46242c..46244a invokes 4b7db0 with position,
first flag, first float, second float and first text. Float reader stores at
4622ad/+14 and 4622bc/+1c identify the two floats. Wrapper 4b7db0 calls the
generic type-10 factory, writes flag to +2b8, first float to +2cc and second
float to +2c8, copies the text to +2bc and calls 4c1d00 to resolve +2c4.
For L1S1 UID 9466 those authored values are flag 1, 0.75, 0 and charge_explode.
Wrapper/lookup execution equivalence remains open; this mapping is static
instruction evidence, distinct from the 288 executed action fixtures.

Ghidra initially had no function defined at the virtual on target. ExportSelected
now disassembles/creates only an explicitly requested entry when neither an
existing function nor containing function exists, and records whether it did so.
The fresh successful manifest exports 4bae20, 436490 and 467020. The latter
consumers are larger effect/geometry paths requiring further reconstruction;
raw Ghidra output remains generated and untracked. No live scene behavior,
explosion image, sound or damage is claimed at this checkpoint.


Shared Explode request emitter (2026-09-10)
-----------------------------------------

rf_event_explode_action accepts a compact state containing geometry flag, room,
position, resolved effect index and the two parameter words. It emits a typed
request for the optional geometry call followed by the general explosion call;
the API documents their original fixed arguments. The geometry flag is tested
as a byte, so 257 acts like 1 while 256 acts like 0. Off emits nothing. The
callback receives a stack-local copy valid only during that callback. The
second request rereads state after the first callback. Consumers may change
state but must retain it; callback mutation is not covered by this verifier.

verify_explode_event.py now compares original intercepted request traces with
PC probe output and compiled NXDK rf_event_explode_action under Unicorn.
All 432 cases pass, including high flag bits, empty rooms, negative effect
indices and parameter patterns. Normalized request order/arguments match via
FNV over the 11 request words. NXDK input state remains unchanged in these
nonmutating cases. PC/NXDK builds, all 93 startup checks and five CTests pass.
The compiled check is not a live XEMU effect run.

This emitter performs no lookup, geometry change, rendering, sound or damage.
It is not yet connected to scene dispatch; unsupported-action accounting remains
honest. Recovering effect-name lookup and request consumers is still required
before calling a scene explosion implemented. No new screenshot is warranted.


Vclip name lookup and authored effect dependency (2026-09-10)
-----------------------------------------------------------

Original 4c1d00 resolves vclip names, not explosion.tbl names. It rejects an
empty query, scans 64 slots beginning at 858cb8 with stride e0, gets each
string through 4ff480 and compares with CRT 57c130, returning the first match
or -1. It scans all slots rather than consulting active-count 8568ac. The
shared rf_vclip_name_lookup follows that fixed-slot order for ASCII names,
with defensive NULL query/table handling and NULL slots treated as unused.
Non-ASCII locale behavior is outside the recovered API's verified scope.

verify_vclip_lookup.py passes 383 cases against unchanged original lookup,
string accessor and CRT comparison, on both PC and compiled NXDK: 63 authored
names with case variants, misses, empty input, duplicate first-match and last
slot. The table fixture is synthetic, populated from the read-only vclip.tbl;
this does not prove the original table loader or all numeric indices. In that
fixture charge_explode occupies slot 51. Both probe/NXDK builds and five
CTests pass. Report: artifacts/vclip-lookup-verification.json.

The installed vclip.tbl defines charge_explode with code_explode, an empty
VBM filename, Explosion Name rocket hit and Foley Sound Medium Explosion.
explosion.tbl has nine named explosion definitions; its rocket hit entry
references several central particle emitters and sparks. Thus a single static
sprite would not reproduce this authored effect. Vclip definition ownership,
emitter/particle definitions, playback and sound consumers must be connected.
This checkpoint adds lookup only; scene Explode remains unsupported and no
new visual behavior or native rendered effect is claimed.


Vclip loader fixed fields (2026-09-10)
------------------------------------

Ghidra exports identify table driver 4c1380 and per-definition loader 4c1460.
The driver reads vclip.tbl, expects #Vclips and repeatedly invokes 4c1460.
Per-record storage is e0 bytes at 858cb8 + count*e0, with count at 8568ac.
The record loader increments count before reading the required name. The
installed file has 63 names; a shared owner must enforce its 64-slot bound.

verify_vclip_defaults.py executes original 4c1460 control flow and field writes
with parser, string ownership and Foley resolution seams supplied. All 512
cases pass (256 presence masks at slots 0 and 63), using initially zeroed
record storage and no embedded particle block. Numeric/default offsets are:
+08 damage default 0; +30 flags default 0; +20 VBM glow default 0 when the
VBM branch is taken; +bc VFX radius default 20; +2c Foley index default -1;
+34 particle count default 0; +c0 explosion-name first byte 0 when absent.
The flag-name table at 5a2284 contains liquid_surface, radius_in_multiples,
no_z_check and code_explode in that order. Parser flag-mask semantics are not
executed by this harness; the mask value is supplied. A present VBM filename
controls whether its optional glow field is read. Present Foley text invokes
434cb0; this harness supplies index 17 rather than resolving a sound asset.

Name, VBM filename and VFX filename use string objects. Explosion name uses a
fixed 32-byte buffer. Optional particle count invokes 497590 to load an embedded
particle/emitter record at +38; that path remains outside these fixtures. The
installed file uses more particle labels than just the fixed vclip properties,
so a complete reader must handle the nested block rather than ignore it.

Report: artifacts/vclip-defaults-verification.json. This establishes fixed-field
initialization evidence only: actual token parsing, strings/allocations, resource
resolution, particle definitions and shared loader equivalence remain open.
No runtime source behavior or new visual changes at this checkpoint.


Embedded particle flag strings (2026-09-10)
-----------------------------------------

Original particle definition reader 497590 writes position/direction, velocity,
spawn timing/radius, emitter flags, alternating timings, particle lifetime/size,
acceleration/gravity, bitmap resource, colors and particle masks. Its vclip
embedding begins at vclip +38. The recovered flag-string portions are now
implemented by rf_particle_flags_read, returning emitter/particle/secondary
masks without resource loading or allocation. The original searches are
case-insensitive substrings, not independent flag tokens: collide_and_die also
sets collide. An initial case-sensitive implementation was rejected by original
execution on uppercase IMMEDIATE and corrected before commit.

Emitter string masks are immediate=2, continuous=4, dirdepend=8,
dont_move_with_parent=40h and accel_with_parent=80h. Boolean initially_on and
alternate_states add separate bits and remain outside this helper. Particle
strings map the recovered 16 labels into the two masks, including wind's
f0000000h field. Optional bounciness/stickiness/swirliness/damage_factor nibble
packing and scalar defaults remain separate loader work.

verify_particle_flags.py executes original spans 4976a4..49771b and
497946..497afe with the actual 4ff870 string search. Prepared register state
matches entry conditions (EBX zero for emitter, 16 for particle flags). All
598 cases match PC and compiled NXDK masks: each name, uppercase, truncation,
prefix/suffix embedding, complete lists and 512 deterministic combinations.
The caller supplies string storage; token parsing and other reader fields are
not claimed. Probe/NXDK builds and five CTests pass. Report is
artifacts/particle-flags-verification.json. Particle spawning/rendering is not
implemented by this helper, so no new visual behavior is claimed.


## Shared player feedback ownership (2026-09-11)

rf_scene_player_feedback resolves the registered local player entity and writes
campaign_camera_effect through rf_camera_effect_start. Force-region feedback
now calls this entry point too. Camera application/reset use that same state,
matching original actor+8b4/+8b8/+8bc ownership. No separate damage-only shake
or extra retained storage was introduced. A subsequent call replaces strength,
duration and deadline. Missing/stale/nonlocal entities return NOT_FOUND without
changing state.

Private scene tests verify replacement and ownership/error preservation. The
original comparisons pass 1,024 start cases, 600 timer/decay cases and 1,800
complete random/orientation cases on PC and linked NXDK code. Both builds and
all 11 CTests pass. The 120-frame PC L1S2 region3705 fixture retains its four
carry/turbulence/shake activations and RNG3250303071. This supplies the resolved
player camera-feedback entry point; player health and HUD damage feedback4a7520
remain separate unfinished owners.

Stock64MiB XEMU replay replay-20260911-153016 passes 120 frames with matching
PC force, camera/RNG and actor telemetry through the registered-player path.
