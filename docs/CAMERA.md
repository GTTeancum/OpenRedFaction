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
