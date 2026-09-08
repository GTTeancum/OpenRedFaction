# Sources and reconstruction evidence

## V3C structural reader

`src/core/model_file.c` and `tools/inspect_models.py` are newly written structural
readers. Layout leads came from the pinned Open Faction
`common/include/formats/v3d_format.h`; no implementation source was copied.
Original 0x51ce60 confirms section dispatch and ignoring SUBM lengths, 0x53ae5f
confirms material records, and 0x5696f0 confirms the signed submesh version check,
LOD limit and bounds envelope. The remaining LOD layout leads are validated
against installed file boundaries; they are not a claim that all geometry and
attachment semantics have been recovered. Unknown payloads stay opaque.

## Original input

Read-only game installation: `D:/Programming/GitHub/OpenRedFaction/Installed_Game`.
The active source workspace is `D:/Programming/GitHub/OpenRedFaction`.

`RF.exe` SHA-256: `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
SHA-1: `f94f2e3f565d18f75ab6066e77f73a62a593fe03`.
`tables.vpp` SHA-1: `ded5e1b5932f47044ba760699d5931fda6bdc8ba`.
Both SHA-1 values match the RF 1.20 NA baseline constants in Dash Faction's
`launcher_common/PatchedAppLauncher.cpp` at the commit below.

Ghidra's initial analysis identified 7,248 functions, including runtime/library
code and possible analysis artifacts. This is not a count of verified game functions.
Generated databases, raw decompiler output, inventories, and original resources
are ignored by Git. Raw decompiler C is evidence, not compilable reconstruction.

## Community references examined on 2026-09-08

| Reference | Pinned commit | Use and limits |
| --- | --- | --- |
| [Open Faction](https://github.com/rafalh/openfaction) | `e8a4a885ba866fc472702b3dc8a9e8208f9b91e4` | GPL-3.0 project; unfinished client and partial server; file-format documentation helps identify VPP/RFL layouts. No source copied into this project's runtime. |
| [Dash Faction](https://github.com/rafalh/dashfaction) | `b2d61d9f66623b188907aae749c4c47c9e40ca25` | Primarily MPL-2.0; executable patch rather than standalone engine; addresses and hash constants serve as analysis leads. No source copied into this project's runtime. |
| [NXDK](https://github.com/XboxDev/nxdk) | `fb5a9a7a58a431e8d70a9e7da87898059df376c0` | Existing local SDK; component licenses remain with SDK. Public headers, build rules, and hello sample establish platform interfaces. |

The newly written bounded VPP reader uses the documented v1 disk layout and
has been checked against the actual installed archives. This is an implementation
of the format, not a claim to reconstruct every behavior of the original VFS.
The layout lead is Open Faction's `common/include/formats/vpp_format.h`:
16-byte header, 2048-byte blocks, 60-byte names plus little-endian 32-bit sizes.
Original `0x0052bd40` independently confirms 64-byte entries and the size word
at entry offset 60. Mount precedence and archive name collision behavior remain open.

## Reconstructed function register

| Original address | New implementation | Evidence | Verification / limits |
| --- | --- | --- | --- |
| `0x0052be70` | `src/core/checksum.c:rf_filename_checksum` | Ghidra decompilation; original CRT lowercase helper disassembly at `0x005752bc`; Dash Faction supplied the address lead. | Compiled 32-bit C matches emulated original x86 for 9,671 inputs, including every 8,410 archive entry name, null, empty, high bytes, and seeded random cases. Default C locale only; original routine's other locale paths are not reconstructed. |

The checksum comparison runs the original instructions inside Unicorn CPU
emulation. It never launches RF.exe or invokes original Windows imports.
The emulator is a development-only dependency, not part of either game target.

## Local harness reference

The user pointed to Unreal Tournament X; its actual directory is
`C:/Programming/GitHub/UnrealTournament_1.40`. Inspected
`UT99-Xbox/Tools/poll_xemu_ram_log.py`, `run_jailbreak_xemu_soak.py`, and the
configuration generator for the technique of resolving guest telemetry from
linker maps and launching isolated emulator instances. The new harness is written
locally using QMP and does not import or run those scripts. Desktop capture and
window-input paths in the old stress harness are not applicable to this project.

RFL section-layout leads in `tools/inspect_levels.py` come from Open Faction's
`common/include/formats/rfl_format.h`. The tool independently reads the installed
binary boundaries; section names are reference annotations, not proof that all
payload semantics have been reconstructed.

`src/core/level.c` is a newly written bounded v180 file-format implementation,
not a decompilation of the original complete level-loading routine. It validates
directories and loads spawn data only. The spawn row order (disk rows 2,0,1)
is documented in Open Faction's `shared/CLevel.cpp` player-start case. PC output
was checked against independent original-file reads for all 94 levels, including
bit-exact float round trips. Original engine-coordinate semantics and camera
integration still need to be recovered and verified.

## Static geometry representation

`src/core/geometry.c` is a new bounded implementation of the observed v180 static
geometry layout. Layout leads were checked in Open Faction's
`shared/CStaticGeometry.cpp` and `common/include/formats/rfl_format.h`; no source
was copied into the runtime. It is not yet a decompilation of the original
complete geometry loader or Geo-Mod engine.

The representation retains the full original geometry payload plus offset tables
for texture names, rooms and faces. Vertices/corners/planes are decoded using
explicit little-endian fields. Uninterpreted trailing records are retained.
The independent Python inspector records their sizes; do not treat its known-field
coverage as proof of all geometry semantics. Scroll records, room effects/liquids,
lightmap transforms, portal relationships and unknown fields are still payload
data rather than implemented engine behavior.

## Provisional geometry preview

`src/core/preview.c`, `tools/pc_preview.c`, and the Xbox renderer/shaders are new
diagnostic scaffolding, not recovered original renderer code. They use observed
geometry/spawn fields, conventional frustum clipping and a provisional camera.
NXDK's local pbkit headers, implementation and shader build rules supply hardware
API/register details. pbkit.c set_draw_buffer restores CONTROL0 to 0x00110001;
the preview restores Z mode afterward because it supplies projected Z with W=1.
Flat normal-based colors are diagnostic only and have no original material meaning.

## TGA decoder

`src/core/image.c` is a new bounded file-format implementation, not decompiled
original bitmap-loader behavior. Layout follows the Truevision TGA specification
(https://paulbourke.org/dataformats/tga/), checked against installed headers and
independent Pillow pixel decoding. It supports true-color types 2 and 10 with
24/32-bit pixels, both origin bits, and 0/8 attribute bits. Unspecified alpha is
made opaque; the original engine's treatment of those bits remains unverified.
Palette images and interleaved storage are explicitly rejected. Extension-area
color management and original texture conversion/mipmap policies remain open.

`src/core/material.c` is new loading infrastructure shared by PC and Xbox. The
caller specifies archive precedence; matching uses the existing case-insensitive
VPP lookup. This is not evidence of the original game's archive override order.
Missing slots remain explicit and found malformed/unsupported images fail loading.
It does not yet implement animated VBM materials or the USERBMAP special case.

Xbox texture upload/sampling are new diagnostic code using local NXDK NV097
definitions. ARGB8 interleaving handles rectangular power-of-two images. Explicit
border-color source avoids assuming stored border texels. This does not establish
equivalence with Red Faction's original texture pipeline.

The lightmap inventory uses layout leads from Open Faction's client/CLightmaps.cpp
and common/include/formats/rfl_format.h: count, width/height, packed RGB pixels.
Exact section consumption was independently checked against all 94 game levels.
No reference engine code was copied; original lighting semantics remain unknown.

`src/core/lightmap.c` is a new bounded implementation of that observed layout.
It expands stored RGB triplets to RGBA with opaque alpha, retaining row order;
it is not decompiled original lightmap allocation, conversion or blending code.

The mapping accessor follows the observed first uint32 in each 96-byte geometry
mapping record, with the same layout lead in Open Faction CStaticGeometry.cpp.
All installed mapping indices have been checked against their level image count.
Open Faction selects EMT_LIGHTMAP_M2 for ordinary lightmapped surfaces; doubled
modulation is only a research lead until checked against original RF renderer code.

Original renderer follow-up: docs/RENDER_STATE.md records Ghidra output for
0x54f160 and independently emulated original mode initialization. It confirms
conditional modulation/doubled modulation paths; the runtime selector is not yet
explained. tools/analyze.ps1 -SkipAnalysis reuses the existing analyzed database
for targeted exports without repeating whole-program analysis.

Further original-binary xrefs resolve 0x1cfcc1d as the D3DTEXOPCAPS_MODULATE2X
capability bit. See docs/RENDER_STATE.md for writer/source addresses and logger
corroboration. The shared preview now propagates the observed corner lightmap UVs
through clipping and resolves image indices; it still does not sample lightmaps.

Camera investigation is recorded in docs/CAMERA.md, with raw original function
exports and directly read binary32 projection constants. The current preview
camera remains explicitly provisional while the entity eye transform is traced.

src/core/eye.c is reconstructed from RF.exe 0x4194e0 and instruction-verified
vector helpers 0x4faa90, 0x40a030 and 0x40a070. It implements only the non-linked
eye update; animated model tag evaluation is explicit unsupported work. See
docs/CAMERA.md and artifacts/eye-verification.json for original-execution evidence.
`rf_motion_apply_controller` in src/core/motion.c is reconstructed from RF.exe
1.20 NA instructions 0x41f2b6..0x41f3f3, after selector dispatch. Ghidra's
0x41f270 export was checked against x87 instructions and direct original-code
execution with tools/verify_motion_controller.py. No original selector or
entity-gate implementation is included. See docs/CAMERA.md for field mappings,
precision, sequencing and verification scope.
The logical state request and membership functions in src/core/motion.c are
reconstructed from RF.exe 1.20 NA 0x42a580 and 0x42a650. Original instruction
execution verifies the fallback, retargeting and midpoint rules; the half
constant was read at 0x5893c0. See tools/verify_motion_request.py and
docs/CAMERA.md. No community implementation was copied for these functions.
`rf_motion_select_movement` comes from RF.exe 1.20 NA 0x41f7c1..0x41f94f,
checked against original instructions and its unmodified callees. The source
includes predicates 0x42a0a0/0x429fc0/0x429ff0/0x42a060 and finite vector
comparisons 0x416270/0x4162b0. See tools/verify_motion_movement.py; earlier
selector branches and physics side effects are explicitly excluded.
`rf_motion_select_priority` is reconstructed from RF.exe 1.20 NA
0x41f400..0x41f5ad, with predicates 0x429f90/0x42ac80/0x42a020/0x40a130 and
velocity magnitude 0x40a000. The duplicate first-occupant call is preserved;
original-code tests resolve synthetic entities through unmodified handle lookup.
See tools/verify_motion_priority.py and docs/CAMERA.md for scope and evidence.
Shared remaining-time and action-activity queries are reconstructed from
RF.exe 1.20 NA 0x51c270 and 0x428d10, including type-two wrappers 0x5033d0 and
0x501bd0. Time constants were read at 0x589e18 and 0x5898e4; the file-end
getter is 0x53a850. tools/verify_motion_action.py executes original callees.
Expanded Ghidra targets document dependencies of candidate helper 0x41f9f0;
its side effects are not yet reconstructed.
`rf_motion_start_action` is reconstructed from RF.exe 1.20 NA 0x428c90,
through loaded type-two character wrappers 0x5033b0/0x501b50 and restart
0x51c1c0. Sound-class dispatch is returned to the caller; original resolver
0x434da0 and audio call 0x5056a0 remain unreconstructed. See
tools/verify_motion_action_start.py and docs/CAMERA.md for evidence and scope.
The shared timer module is reconstructed from RF.exe 1.20 NA clock/timer
instructions 0x4fa2d0..0x4fa45d, excluding the random-range setter. No original
callee replacements are used by tools/verify_timer.py. See docs/TIMERS.md for
individual addresses, explicit input bounds and turn-helper timer call sites.
src/core/movement.c reconstructs RF.exe 1.20 NA 0x427450 and flag predicate
0x40a210. Mutable override sources 0x59458c/0x594590 are explicit API inputs;
the response formula preserves original operation order. Original-code evidence
is generated by tools/verify_movement.py; docs/MOVEMENT.md records source fields.
