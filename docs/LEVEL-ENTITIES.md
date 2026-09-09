# Level entity records

Xbox model previews now accept a `model-skin.txt` disc file containing a miner1
skin name (up to 63 bytes including any trailing CR/LF). Missing file selects
base materials; empty, oversized or embedded-NUL selections fail. Both PC and
Xbox use `rf_entity_assets_load`, which loads entity.tbl into temporary heap
storage under a caller-specified cap (512 KiB here), reads metadata, and frees
the table before material loading. Failure preserves the caller's output.
The probe checks all 168 selections against the buffer reader, exact table-size
budgets, one-byte-short rejection and missing-skin output preservation.
PC/NXDK builds and four CTest checks pass.

Diagnostic ABI version 9 adds skin-name checksum and replacement count in words
56/57; `xemu_smoke.py --skin Parker` checks both and derives the selected GPU
texture allocation. PC `--model-skin-last` produces frame 63 for comparison.
Stock 64 MiB XEMU run `20260908-223040-356250` passes all 64 submitted frames,
Parker checksum 0x19d75f2e, 12 replacements and 794,628 GPU image bytes. Its
native framebuffer was visually inspected and passes PC comparison: 4 pixels
over error 3, maximum channel error 114, mean maximum error 0.004235. Repeat
`20260908-223124-961060` passes without capture. Base-selection control
`20260908-223011-295569` also passes without capture. This remains a posed
inspection; no combined level/actor rendering or original skin-switch parity.

Initial stack-local metadata builds hit XEMU's NV2A surface/DMA-limit assertion,
including a base-skin control. Keeping the 4,164-byte selection in static
diagnostic storage instead produced the successful runs above. The executable
reserves a 64 KiB stack; this reduces nested stack demand but does not establish
the assertion's root cause. Audit the animation/preview stack and GPU transition
before claiming the failure fully explained. The harness now gives QMP a
bounded 30-second response timeout and tolerates close errors so an emulator
disconnect cannot prevent process cleanup and writing its diagnostic report.

`rf_model_materials_open_skin` adds bounded ordered primary-texture substitution
to the shared material loader. Nonempty selections must match the complete
SUBM material count; names must terminate within 32 bytes. Zero count uses base
materials. Secondary maps and other serialized fields are retained, while
deduplication and alpha classification use the selected images. The existing
resident/peak budget and failure cleanup apply. This is port-owned skin-loading
scaffolding, not a reconstructed original runtime skin switch. One original
`$Skin:` parser at 0x40f4f0 passes capacity 12 to 0x512e20 at 0x40fa55; that
static observation does not prove the material substitution algorithm.

`tools/verify_model_residency.py --skins` checks all five miner1 variants
(b/c/d/e/Parker), 60 complete runtime material records, selected texture slots,
secondary maps, alpha flags and exact Win32 memory accounting. Each bundle uses
797,800 resident bytes and 799,000 peak bytes. Five insufficient-budget cases,
five missing-texture cases and 20 invalid selection count/name cases fail with
empty outputs. The original base path still passes all 95 models/733 records;
PC/NXDK builds and four CTest checks pass.

The PC diagnostic accepts `--model-skin meshes.vpp motions.vpp output.ppm
tables.vpp skin maps1.vpp ...`. It reads miner1 metadata from the supplied table
(512 KiB input cap), verifies its skeletal filename matches the diagnostic's
miner geometry, then applies its skin list. This is explicitly a miner pose
inspection, not arbitrary entity spawning. Missing skins fail before rendering.
Parker's first-frame preview at `artifacts/miner-parker-pc.png` was visually
inspected; comparison with the same base pose changes 465 pixels, confined to
the face rectangle [283,142)-(309,163). The faceplate remains translucent.
Xbox selection, level placement and original skin-switch equivalence are open;
this new visible result is PC only.

`rf_entity_skeletal_filename` reconstructs the `.v3c` specialization of original
filename helper 0x5142d0, called at 0x51ce8f by skeletal loader 0x51ce60. Its
callee 0x514330 finds the last dot using the unchanged CRT helper at 0x573b10,
copies the preceding bytes and appends the requested extension. This includes
dots in directory names; case is preserved. No dot means append, while empty
input becomes `.v3c`. The caller must therefore skip entities without a model.
The port requires termination within 64 input bytes and a result of at most
63 bytes plus NUL, supports in-place conversion and preserves output on failure.
Those capacity guards are added port policy; the original helper is unbounded.

`tools/verify_entity_model_filename.py` executes the original helper and its
unchanged callees with `.v3c`, comparing all 64 output bytes for 1,960 accepted
fixtures. Another 41 cases validate capacity rejection/output preservation;
the probe self-test also checks in-place conversion and null input. PC/NXDK
builds and all four existing CTest tests pass. This establishes filename
conversion, not file loading or model-type selection.

Of 50 table classes authored with `.vcm`, 49 resolve to names in `meshes.vpp`;
`edf_ship.v3c` is absent there and needs investigation. Twelve other model
declarations use `.v3d` and remain outside this skeletal resolution path.
For example, the archive contains `sturret_head.v3m`, not `sturret_head.v3c`.
The table parser at 0x41b910 distinguishes extension types around 0x41ba35;
recover that dispatch and the static-model path before treating this as a
general entity model resolver. Ghidra export records extension references in
`model-extension-xrefs.tsv`. No runtime scene integration or new image yet.

`rf_entity_assets_read` now selects authored model names and ordered skin
texture lists from caller-owned `entity.tbl` text without allocating memory.
It supports the installed quoted-string, whitespace, parentheses and `//`
comment syntax, with ASCII-insensitive class/skin selection. Limits are 255
bytes per token, 63 per asset name and 64 replacements. Missing class or skin
returns NOT_FOUND; failures preserve the output. An empty model is valid for
a class with no authored mesh, such as the freelook camera. Other table fields
remain uninterpreted. This is metadata-reading scaffolding, not a decompilation
of the original general table parser; unsupported syntax is not claimed.

`tools/verify_entity_assets.py` independently extracts installed declarations
and checks all 63 classes and 168 base/skin selections against the compiled C
reader, including commented-out skins and original replacement ordering.
The probe self-test covers comments/case, missing selections, an unterminated
quote, replacement overflow and unchanged output on failure. PC/NXDK builds
and four CTest checks pass. The caller-side probe caps its table load at 512 KiB;
the runtime API consumes an existing buffer and owns no table storage.

`miner1` names `miner.vcm` and has five authored skin variants (b/c/d/e/Parker),
each with 12 replacement names. Its installed geometry is `miner.v3c`; the reader
preserves the authored extension, and the separate skeletal filename helper
now converts it to the compiled name. Runtime model selection and skin
replacement application are still open.
The installed level classes `camera1` and `Bucket Bot` do not directly match
the table class declarations; do not invent a model or silently alias them.

`rf_level_entities_begin` and `rf_level_entity_next` traverse the installed
v180 entity section (0x30000) without allocating its payload. Each result
contains UID, class/script names, position, reordered orientation, state
animation and skin, plus the section-relative raw record span. Other behavior
fields are traversed to locate the next record but are not interpreted or
applied to gameplay. The class name is not assumed to be a mesh filename.

Strings retained in the public result are bounded to 255 bytes plus terminator;
unretained strings are skipped with section bounds checks. Nonfinite transforms,
invalid optional-range tags, impossible counts and trailing bytes fail. The
iterator and caller result remain unchanged when a record fails. NOT_FOUND
means all declared records consumed exactly the section. The caller keeps the
level/archive open throughout iteration. There is no entity spawning, AI,
inventory, model/skin resolution or game-state initialization here.

The layout lead is Rafal Harabien / wardd64's
[rf-reversed RFL specification](https://github.com/rafalh/rf-reversed/blob/master/rfl.ksy),
published as GPL-3.0-or-later. The downloaded reference on 2026-09-08 has SHA256
`78d4cdf16a66a1fd991405786f553a86e539663c011d8b0eaae43169ab0b9e2b`.
This project adds its own bounded C reader and independent byte-inspection
tool; no generated Kaitai parser is used. Field names are format-reference
annotations, not evidence of original executable loader semantics.

`tools/verify_level_entities.py` reads all installed entity-bearing level
sections and compares every exported field and record span against the compiled
C probe. All 1,610 records in 66 levels pass, with exact section exhaustion.
The report at `artifacts/level-entities-verification.json` includes class counts
and Live Mines placement candidates. In L1S1, miner1 UID 9858 is at approximately
(-92.402, -3.191, 49.761), near the stored player start; the nearest entity is a
Grabber, so proximity alone must not choose the actor's model.

The level tests add orientation-order validation, 155 truncation boundaries,
nonfinite transform, overlong retained string and invalid optional-range-tag
checks. PC/NXDK builds and all four CTest tests pass. Verification covers file
decoding and bounds, not execution of the original entity loader or gameplay.
No new visual output was produced.
