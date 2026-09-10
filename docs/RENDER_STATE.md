# Original renderer evidence

Applies only to RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

## Solid mode initialization

`tools/inspect_render_state.py` executes original instructions at 0x515730 and
the constructor at 0x411e00 under isolated Unicorn x86 emulation. They write
0x00400024 to 0x01808328. The constructor packs six five-bit fields; the resulting
values are [4, 1, 0, 0, 4, 0]. This establishes the initializer's result, not every
later mutation or draw-path choice. Ghidra identifies reads in functions 0x4f0c00
and 0x561650. Disassembly and emulation result are in artifacts/render-state.json.

## Texture-stage state

Raw Ghidra export of 0x54f160 is in artifacts/analysis/rf_b8fb9ab4c9bf/54f160.c.txt.
The vtable +0xfc calls match D3D8 SetTextureStageState. Constants were checked in
the local MinGW d3d8types.h, not inferred from a modern renderer.

- Texture-source case 4 selects stage-0 texture color and stage-1 MODULATE (4),
  except color-source 3 uses ADD (7). Stage-1 arguments are TEXTURE (2), CURRENT (1).
- Case 5 uses stage-1 MODULATE2X (5).
- At entry, texture source 4 can be changed to 5 when byte 0x01cfcc1d equals 1.
- The multitexture paths set min/mag filtering to LINEAR (2), with base UV wrapping
  and lightmap UV clamping in the relevant paths.

The writer and meaning of 0x01cfcc1d still need tracing. Texture conversion may
also change lightmap intensity before sampling. Therefore Open Faction's use of
EMT_LIGHTMAP_M2 is not sufficient evidence for unconditional doubled lighting.
Do not claim complete recovered render-state semantics from this partial export;
fog, alpha, blending, format conversion and alternative passes remain open.

## Capability selector resolved

Ghidra xrefs identify a write at 0x545f46 in 0x545d50:
`flag = (TextureOpCaps >> 4) & 1`. The source DWORD at 0x01cfcb58 is offset 0x90
from the D3DCAPS8 object at 0x01cfcac8 (the TextureOpCaps field). Local D3D8 headers
define D3DTEXOPCAPS_MODULATE2X as 0x10. Function 0x5463d0 logs this flag under
the original `D3Dm2x` label; 0x546a00 checks it when deciding mode support.
Thus the earlier unknown flag is a device capability, and doubled modulation
is the supported two-texture route. Texture conversion/intensity quantization
and exceptional render modes still need independent verification.

## Particle blend, depth and fog state recovery

`tools/verify_particle_render_states.py` executes the complete original 0x54f160
with texture mode 1 and an empty batch. A fake COM device records calls at
vtable offsets 0xc8 and 0xfc; no GPU or desktop input is involved. The original
mode getters, branches and cache comparison execute unchanged. Across 3780
cases, `rf_particle_render_decode` matches the ordered render-state writes and
color/alpha/fog selector values on PC and compiled NXDK x86 under Unicorn.
Another 3780 original calls verify that an unchanged packed mode skips state
updates even after the supplied blend capability bits change.

The six five-bit fields are texture, color, alpha, blend, depth and fog, at bit
positions 0, 5, 10, 15, 20 and 25. The C helper reconstructs the state-selection
tail only; callers must implement the preceding cache, flush and texture-stage
logic separately. It emits at most ten original D3D8 state/value pairs into a
96-byte caller-owned record, with no heap allocation. The backend must translate
these values explicitly; they are not NXDK GPU register encodings.

Blend mode 0 disables blending without rewriting factors. Modes 1, 2/4, 3,
5, 6 and 7 select factor pairs (2,2), (5,2), (5,6), (9,1), (10,1) and (9,3).
Mode 3 without capability bit 0x10 writes source factor 12 only. Mode 5 without
bit 0x100 emits no blend writes. These preserve-state cases must not be replaced
with an assumed default. The capability word comes from original 0x01cfcaf4.

Depth modes 0 through 5 reproduce the original enable, write and comparison
calls, including mode 3 disabling depth testing while enabling depth writes.
Mode 5 adds alpha testing with reference 16 and comparison value 7. Depth kind
at 0x017c7c4c chooses enable values 1/2/0 for kind 0/1/other, and comparison
7 for kind 0 versus 4 otherwise. Unknown modes emit no writes for their category.
Fog modes 0..2 follow the low byte of 0x017c7c20; mode 3 disables fog. Vertex
fog is enabled only when fog is enabled and the kind at 0x005a7df8 equals 2.

This is not a native GPU render test. Particle default packed modes at
0x017c7c58 and 0x01775b30, polygon clipping, billboard depth bias, texture-stage
semantics for particle passes, residency and scene/backend integration remain
open. The existing world/actor renderer has not been switched to this helper.
