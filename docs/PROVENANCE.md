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
src/core/turn.c reconstructs the combined effects at RF.exe 1.20 NA
0x41fbdc..0x41fc83 using separately verified shared action, timer and movement
components. tools/verify_turn_effects.py executes original callees with absent
sound classes; docs/TURN.md describes excluded decisions/reset and audio work.
`rf_turn_direction` reconstructs RF.exe 1.20 NA 0x41fa7c's direction gate,
with normalization 0x4fab30, dot product 0x40a0b0 and transform 0x4faa30.
tools/verify_turn_direction.py compares original instructions without callee
replacement; docs/TURN.md records the precision-sensitive boundary evidence.
`rf_turn_finish_candidates` reconstructs RF.exe 1.20 NA branches from 0x41fc84
through the end of helper 0x41f9f0, using recovered movement settings and action
controls. tools/verify_turn_finish.py executes original callees with absent
sound classes. See docs/TURN.md for boundaries and excluded upstream decisions.
`rf_turn_update` assembles RF.exe 1.20 NA helper 0x41f9f0, leaving 0x41ae70
behind a required callback and returning audio requests. Original complete
function tests use absent weapon entries and sound classes, not patched callees.
The target-distance calculation follows 0x4faed0/0x409fa0/0x40a000 with threshold
0x58956c. See tools/verify_turn_update.py and docs/TURN.md for verification limits.

`tools/inspect_animation_names.py` reads original initializer instruction triples
at 0x418030 (23 state names, destination 0x62f208) and 0x4181d0 (45 action names,
destination 0x5caee0). It checks each call to string constructor 0x4ff3d0 without
executing or replacing the constructor. This establishes sidestep/roll action
names used in the runtime diagnostic; no third-party code is involved.
The expanded `src/diagnostic/animation.c` is scripted integration scaffolding,
using locally supplied RFA assets and the existing miner rig. Its original-code
comparison executes 0x41f9f0 and all reached callees unchanged, with absent roll
mappings and sounds. See docs/TURN.md and docs/VALIDATION.md for current scope.

`rf_locomotion_choose_candidates` follows the RF.exe 1.20 NA selector block
0x41f61d..0x41f728, including 0x41f9f0 and post-helper velocity comparison at
0x41f6bf..0x41f6e8. The .3 threshold multiplier is the binary32 at 0x5894d8.
Its resolved combat input represents the exact calls at 0x41f678, 0x41f685
and 0x41f692. `tools/verify_turn_update.py --candidates` executes these original
callees without replacement; no third-party implementation is used.

`src/core/entity.c` reconstructs read-only decisions from RF.exe 1.20 NA
0x40a0e0, 0x426fc0, 0x408dc0, 0x40a2a0, 0x427da0, 0x48aaf0, 0x41f950,
0x408e90, 0x427020, 0x428e60 and 0x429f90. Compact views replace original
layout/pointer containers; stable-input traversal and bounded cycle handling
are documented in docs/ENTITY.md. Original-code tests replace no callees.

`rf_locomotion_prepare` follows RF.exe 1.20 NA 0x41f5ae..0x41f61c, including
mode predicate 0x42a060, weapon descriptor predicate 0x4c91b0 and timer 0x4fa360.
Valid reset effects remain owned by the explicit 0x41ae70 adapter. See
tools/verify_locomotion_prepare.py and docs/TURN.md for original-code fixtures
and the limited invalid-index adapter used by the diagnostic.

`src/core/weapon.c` reconstructs RF.exe 1.20 NA 0x41ae70's reset control flow
after type-zero entity resolution. Descriptor/flag mappings and required
external-operation adapters are documented in docs/WEAPON.md. The original
read-only 0x4c90f0 call at 0x41afcc discards its result; it is not a local-release
call. `tools/verify_weapon_reset.py` compares full state to original completion
or observation boundaries, without replacing callees. Runtime diagnostics now
use this reset for valid weapon 0 instead of the earlier invalid-index adapter.

Disassembly of 0x48aa90 establishes a read-only first-local-player lookup, not
a local-release operation. The unused return at 0x41b00e requires no callback;
the earlier dependency has been removed and its tests corrected.
`src/core/effect.c` follows 0x48f130 with 0x4973b0/0x4973d0 and timer 0x4fa360.
See docs/WEAPON.md and tools/verify_effect_switch.py for field mappings and
original-code coverage; effect ownership/rendering are still separate work.

`rf_weapon_reserve` and `rf_weapon_choose_available` follow RF.exe 1.20 NA
0x42add0 and 0x4a6e50, including ownership predicate 0x403250 and descriptor
flag predicate 0x4c9a70. Original disassembly of 0x4a6f10 and its string at
0x5a05e0 identifies empty-ammunition handling as the remaining player callback's
role. See docs/WEAPON.md and tools/verify_weapon_inventory.py for scope and
original-code verification; actual switching/presentation is not reconstructed.

`rf_weapon_decide_empty` follows RF.exe 1.20 NA 0x4a6f41..0x4a70db, after the
current-weapon/presentation call. It returns commands corresponding to 0x4a4e80,
0x4383c0 and 0x4a4a50 rather than silently executing substitutes. Original tests
run unchanged passenger 0x42acd0, projectile 0x4c9e30, linked-class, ammo and
replacement callees and observe the final call boundaries. See docs/WEAPON.md
and tools/verify_weapon_empty.py for detailed input mappings and exclusions.

Weapon-selection queue: shared `rf_weapon_queue_selection` is reconstructed
from RF.exe SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836,
0x4acd50..0x4acd68 and unchanged tail callee 0x4fa3e0. Original-machine-code
comparison covers 640 complete cases; no original code or data is distributed.
Expanded local Ghidra exports clarify 0x4a4e80 firing behavior and 0x4a4a50
selection checks. Raw exports remain ignored; these larger routines are not
claimed as reconstructed by the queue primitive.

Weapon selection tail: `rf_weapon_finish_selection` follows RF.exe 1.20 NA
SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836
0x4a4c91..0x4a4db4. Predicate evidence includes 0x42a6b0 (entity +1428 bits),
0x403250 (ownership), 0x4c9070 (descriptor +264 bit 18), 0x4a68d0 (player
+10 bit 4), and 0x4ace90 (player +f94 byte). Local original-code verification
executes these unchanged and the queue/timer callees, stopping before formatted
message construction, 0x4aa0b0 and 0x4ad8a0. These adapters and earlier gates
remain open; no original code/data or generated decompiler output is tracked.

Selection followup correction: original 0x4ad8a0..0x4ad8b8 contains only stores
to player +f94, +f95 and +f98. Shared `rf_weapon_clear_followup` now replaces
the unnecessary external callback and runs within selection-tail verification.
Original 0x4aa0b0, 0x4aa080 and 0x4ab180 were also inspected to identify the
remaining local transition dependencies; their complete behavior is still open.
Evidence uses the same fingerprinted RF.exe and ignored local Ghidra exports.
