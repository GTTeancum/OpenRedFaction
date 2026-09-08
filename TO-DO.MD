# Red Faction Xbox / PC — open work

## Shortlist

- [ ] Extend the shared level-directory loader into a complete campaign world loader.
- [ ] Recover the gameplay camera and entity eye transform instead of using raw spawn coordinates.
- [ ] Draw the loaded static geometry and resolve its materials, lightmaps, visibility, and unknown records.
- [ ] Fingerprint the original executable and reconstruct its subsystems as portable C/C++ using Ghidra and documented evidence.
- [ ] Implement streaming asset access and level loading within a stock Xbox's 64 MiB unified memory.
- [ ] Build an Xbox renderer and corresponding PC renderer that preserve at least PS2 visual parity.
- [ ] Restore the complete single-player campaign, including Geo-Mod, AI, physics, scripts, audio, menus, and saves.
- [ ] Validate performance and correctness in 64 MiB XEMU before USER hardware testing.
- [ ] USER: Review visual comparisons against the PS2 version when representative scenes are available.
- [ ] Consider multiplayer only after single-player completion.

## Current camera evidence

Shared C now traverses V3C section structure directly in VPP archives, including
zero-sized SUBM chunks, while leaving LOD blobs on disc. Bone verification uses
structural boundaries instead of signature searches. Decode LOD geometry,
attachments and animation data next; the directory alone does not load a model.

The suspected bone remapping is a stable depth-sorted evaluation order, now
reconstructed in shared C with 200 exact original-instruction comparisons
(95 asset skeletons plus 105 synthetic cases). It does not change file indices.
Use this order when integrating animation poses; full V3C parsing and animation
sampling/blending remain unfinished.

Parent transform multiplication at 0x51c620 is reconstructed in shared C and
matches 2,852 original-instruction comparisons bit-for-bit on PC. Original bone
remapping and full hierarchy/animation evaluation remain open; isolated matrix
products do not yet establish a correct animated eye pose.

Local bone normalization and transform construction are now reconstructed in
shared C; 2,852 PC results match the original instructions bit-for-bit, covering
all 2,452 decoded asset bones and 400 scaled synthetic quaternions. Continue with
parent composition, animation evaluation and proper V3C traversal before wiring
the reconstructed eye pose into gameplay.

The shared raw BONE payload decoder now preserves fields across 95 detected
installed sections (2,452 bones), with no runtime allocation and validation of
finite transforms and acyclic parent indices. Finish structural V3C section
parsing, name processing and original quaternion conversion before pose use;
the verification tool's signature scanning is only diagnostic discovery.

Player creation now identifies the default `miner1` class, and unhooked original
filename resolution confirms `miner.vcm` -> `miner.v3c`. The asset audit resolves
45 of 46 entity-table character references; investigate `edf_ship.vcm` use and
fallback before treating its missing .v3c as a blocker. Parent-chain pose
evaluation is traced to 0x51b2e0/0x51b500 but remains unreconstructed.

Character tag lookup is now reconstructed in `src/core/model.c` and included in
PC/NXDK builds; 306 comparisons against original 0x51d5b0 and its unhooked
default-locale comparator pass. This covers ordered lookup, not model loading
or animated poses. Continue into 0x51b2e0, asset name resolution and player model
selection; runtime pointer layouts must not be used as file layouts.

Camera evidence update: original offset initialization at 0x423bd0 samples the
model's `eye` tag into class standing/crouching fields using identity rotation
and zero origin. Eight isolated original-instruction cases pass in
`tools/inspect_eye_setup.py`, with explicitly synthetic model hooks. Missing
tags and the info +0x724 flag 0x20000 behavior are verified. Recover player model
selection, animation indices, asset parsing and pose evaluation before feeding
these offsets into the campaign camera; no guessed eye height is authorized by
this evidence. See `docs/CAMERA.md` for addresses and remaining uncertainties.

## Accepted requirements

- Original Xbox is the primary platform; only stock 64 MiB hardware is supported.
- A PC build must share the Xbox-compatible game implementation and resource limits.
- C/C++ reconstruction and recompilation using NXDK; Ghidra is available for analysis.
- Single-player campaign first; multiplayer is a stretch goal.
- Resource reductions are allowed only while maintaining PS2-level visual parity.
- XEMU is the primary emulator; CXBX-R may assist with diagnosis.
- Never control the desktop or send host input; use source, files, logs, non-interactive commands, and process-contained test harnesses.

## Milestone 0 — evidence and toolchains

- [x] Set the persistent project goal.
- [x] Inspect the initial workspace: only an empty Git repository was present.
- [x] Locate NXDK at `C:/nxdk` and Ghidra at `C:/Programming/ghidra_11.3.2_PUBLIC`.
- [x] Locate the game, record file hashes, PE architecture/version/imports, and asset archive inventory.
- [x] Record compiler, SDK, emulator versions and reproducible commands.
- [x] Evaluate Open Faction and Dash Faction as references, recording provenance and reuse terms.
- [x] Compile and validate minimal PC and Xbox artifacts; distinguish scaffolding from recovered game behavior.

## Milestone 1 — reconstruction foundation

- [x] Create a reproducible headless Ghidra project from a hash-identified executable.
- [ ] Map startup, filesystem/archive loading, memory allocation, game loop, rendering, audio, input, and level transitions.
- [x] Start a recovered-function register with original addresses, evidence, dependencies, confidence, and behavioral tests.
- [x] Implement bounded archive access and inspect the first campaign level's section boundaries.
- [x] Load level directories and spawn transforms in shared C, validate all 94 installed levels on PC, and validate Live Mines in 64 MiB XEMU.
- [x] Load budgeted resident static geometry in shared C, validate all 94 installed levels, and verify Live Mines geometry in 64 MiB XEMU.
- [ ] Reconstruct level payload parsing and original archive mounting/override semantics.
- [ ] Establish native emulator visual capture without any desktop capture or window input.
- [ ] Establish memory accounting that includes executable, heaps, stacks, GPU surfaces, audio, and transient loads.

## Milestone 2 — playable campaign slice

- [ ] Load and draw the first campaign area on both platforms.
- [ ] Restore collision, player movement, controller mapping, weapons, enemies, events, and Geo-Mod.
- [ ] Restore HUD, sound, dialogue, cutscenes, saves, and level transitions needed by that slice.
- [ ] Compare matched PS2 scenes and capture native emulator output for review.

## Milestone 3 — campaign completion and validation

- [ ] Extend subsystem coverage through every campaign level and document completion evidence.
- [ ] Run memory stress, loading, save/reload, destruction, AI, and frame-time regression scenarios.
- [ ] Validate on 64 MiB XEMU, then prepare a reproducible USER hardware test package.
- [ ] Fix hardware findings and finish PC regression coverage.

## Current status

Original inputs are at `D:/Programming/GitHub/OpenRedFaction/Installed_Game`;
source work is in this repository on D:. RF.exe and tables.vpp match the
Red Faction 1.20 NA baseline. Inventory: 39 files, 15 VPP archives, 8,410 entries.

Both a 32-bit PC archive diagnostic and an NXDK XBE/XISO compile. The shared C
VPP reader matches the complete installed archive directory inventory and passes
malformed-input/bounds tests. One function, the filename checksum at `0x0052be70`,
has been reconstructed and verified against original x86 for 9,671 test inputs
in the C locale. Ghidra identified 7,248 candidate functions including libraries;
this is not a measure of completed reconstruction.

The diagnostic boots and validates tables.vpp on 64 MiB XEMU with both installed
4627 debug and Complex 4627 v1.03 BIOSes using the xemu rendering backend.
The headless `-display none` path faults before entry in this emulator build;
its exact cause remains a non-blocking harness investigation. The harness uses
isolated config/EEPROM, temporary HDD writes, and monitor-only guest telemetry.
UT99 reference harness location resolved to
`C:/Programming/GitHub/UnrealTournament_1.40/UT99-Xbox/Tools`.

All 94 installed RFL level files have been scanned for section boundaries.
The first campaign level is `L1S1.rfl` (Live Mines), version 180, 3,572,594 bytes,
with 29 payload sections plus an end marker. Static geometry is 1,637,649 bytes
and lightmaps are 1,130,684 bytes on disk; neither number is a runtime memory estimate.

No playable game, PS2 visual comparison, or hardware validation exists yet.

### Latest progress

Moved the authoritative source project, references, and analysis databases to
`D:/Programming/GitHub/OpenRedFaction` after the user's location correction.
Original inputs remain in its `Installed_Game` subfolder and are ignored by Git.
The C-drive task workspace contains relocation pointers; its old build outputs
are retained under `local/legacy-build-c` on D:.

The shared C level loader now validates v180 section directories, bounds, header
offsets, end markers, and finite spawn transforms without loading whole RFL/VPPs.
All 94 installed directories and spawn transforms agree with independent binary
reads; two CTest suites pass. Live Mines' directory, geometry/lightmap section
sizes, and exact spawn position are verified in 64 MiB XEMU from a disc containing
the unmodified 68.8 MB levels1.vpp archive.

Static geometry now retains original payload bytes and builds texture/room/face
offset indices under an explicit allocation budget. Live Mines has 6,658 vertices,
7,418 faces, 29,110 corners and 54 rooms, using 1,667,605 requested bytes for this
geometry representation. All 94 installed levels load on PC; the largest requests
2,131,016 bytes. Accessor-bound and allocation-budget checks pass, as do nine
corrupted geometry cases. XEMU verifies counts and first/last decoded positions
with geometry resident. This is not yet a rendering, collision, or gameplay test.

The initial untextured static geometry preview now renders on PC and Xbox from
the stored Live Mines spawn. Three frames execute in 64 MiB XEMU; a native guest
framebuffer capture matches the PC reference within the diagnostic edge tolerance
(191 of 307,200 pixels exceed three channel values). Fixed a W-buffer override
after back-buffer selection. This provisional camera and flat face coloring do
not establish original rendering fidelity; textures, lightmaps and gameplay remain.

The shared C TGA decoder now reads 24/32-bit raw and RLE images directly from VPP
entries with a fixed 4 KiB input buffer and explicit output allocation budget.
All 16 Live Mines TGA references resolve across maps1/maps2/maps3/maps4/maps_en;
RGBA8 base images total 2,117,632 bytes. USERBMAP remains unresolved. Pixel output
matches Pillow for 41 installed images covering all supported observed header
variants and every RLE image; 17 synthetic origin/truncation/packet cases pass.
One installed paletted TGA remains unsupported.

The shared material loader now uses bounded geometry-name access and explicit
archive search order. It allocates 2,118,040 bytes including 17 material slots;
the exact budget passes and one byte less fails with cleared state. It loads the
16 images inside stock 64 MiB XEMU and retains USERBMAP as an explicit missing
slot. Guest pixel checksum 0xa3aeb67d matches independent decoding. The material
images remain resident alongside geometry and the three-frame GPU diagnostic.
The shared preview now clips source UVs with geometry and carries u/z, v/z, 1/z
and material indices. Its vertex allocation is now 1,460,280 bytes. The PC reference
samples original textures with perspective division and nearest/repeat sampling;
artifacts/live-mines-textured-pc.png shows the first base-textured tunnel view.
Xbox now uploads swizzled base textures and samples projective UVs. Selecting
border-color source fixes initial corrupt sampling because images contain no
border texels. The textured tunnel is visible, but the strict PC comparison fails:
29,235/307,200 pixels exceed three channel values (mean maximum-channel error 1.346).
Resolve sampling differences before claiming PC/Xbox equivalence. Lightmaps,
filtering, animation and archive override rules remain open. Fixed stale Xbox
header dependencies caused by Windows/POSIX path mismatch; a rebuilt XEMU frame
with the expanded vertex stride passes the untextured comparison.

Next: bind decoded materials to geometry on both renderers, load lightmaps, recover portal/room visibility,
and investigate trailing records that the community geometry loader leaves opaque.
Original geometry-loading function semantics still require Ghidra reconstruction;
the new loader implements the observed file layout and preserves unknown bytes.

Sampling diagnosis: optional RF_PREVIEW_SNAP_1_16 quantizes only the PC diagnostic
screen coordinates; it reduces the textured mismatch to 1,222 pixels (0.398%),
still FAIL against the existing tolerance. This is an unverified raster precision
hypothesis, not a hardware rule or a relaxed acceptance threshold. Default PC
output remains unchanged. A 1/32 sample-center shift was less effective and removed.

Lightmap inspection now covers exact image boundaries in all 94 installed level
sections. Live Mines has 23 RGB lightmaps, expanding to 1,507,328 RGBA bytes;
largest installed level expansion is 3,670,016 bytes. The shared C lightmap loader
now loads all 94 levels under exact output/record budgets, rejects one byte less,
and clears allocations on failure. Live Mines' 23 decoded pixel hashes match
independent expansion of original RGB bytes. PC/NXDK builds pass. Geometry mapping
indices now resolve through the first word of each 96-byte record and all indices
are in range across all 94 levels. Live Mines' lightmaps remain resident in 64 MiB
XEMU alongside base materials and geometry; the base-textured frame is unchanged.
Lightmap UV propagation, blending rules and GPU sampling remain open. Open Faction
uses doubled modulation, but original-binary investigation shows this is conditional.
Original initializer 0x515730/constructor 0x411e00 emulates to solid mode 0x400024
at 0x1808328. Original state function 0x54f160 uses modulation in texture-source
case 4 and doubled modulation in case 5, with runtime byte 0x1cfcc1d selecting a
4-to-5 remap. Trace that flag and lightmap conversion before fixing the lighting
equation. See docs/RENDER_STATE.md; no unconditional lighting pass was substituted.

The runtime flag is now identified as TextureOpCaps MODULATE2X: original write
0x545f46 extracts bit 0x10 from 0x1cfcb58, and the original logger calls it D3Dm2x.
The shared mesh now clips lightmap UVs and carries projected coordinates plus
resolved image indices (56-byte vertices, 2,044,392 bytes for Live Mines).
The second texture stage now samples lightmaps with linear clamping and combines
them with linearly wrapped base textures using doubled modulation. Draws split at
material/lightmap boundaries. The first lit tunnel renders on PC and 64 MiB XEMU.
Strict comparison still FAILS: 5,248/307,200 pixels exceed three channel values
(mean maximum-channel error 0.609). PC/NXDK builds and existing CTest checks pass.
Original texture conversion, fog, alpha, mipmaps and PS2 comparison remain open.

The shared projection now uses the same explicit 1/16-pixel grid on both targets;
the former PC-only diagnostic flag has been removed. The latest lit-frame
comparison PASSES: maximum channel difference 1, zero pixels over the existing
three-value tolerance. Schema-7 XEMU telemetry measures GPU image requests
3,624,964 bytes and vertices 2,044,392 bytes, with 44.25 MiB available after uploads.
This frozen-scene snapshot is not a full-game peak or PS2 parity result.

Camera investigation now identifies first-person entry at 0x40ddf0, camera
position access at 0x40d760 and projection setup at 0x547150. First-person entry
uses the entity's computed eye position/orientation; the raw spawn lacks that
transform. Original projection constants are fingerprinted in camera-evidence.json.
No guessed eye-height or original near/far interpretation was introduced. See
docs/CAMERA.md for the entity/model and projection work still required.

Camera update dispatcher 0x40d850 is now exported; first-person mode copies the
player eye point to camera position before additional effects. Corrected thiscall
metadata for original vector helpers 0x409f40/0x409f70 makes that data flow explicit.
An original-instruction operand scan records candidate eye-field references for
further tracing; the standing/crouching eye writer is still unresolved.

The writer is now identified as 0x4194e0. Its non-linked branch is reconstructed
in src/core/eye.c: states 8/9/10 select crouching, offsets are transformed by the
entity orientation and blended during transitions, then added to position.
400 deterministic cases match original x86 execution exactly in the PC test.
The animated eye-tag branch returns explicit RF_NOT_FOUND; model-derived offsets,
entity state loading and actual camera integration remain open.
