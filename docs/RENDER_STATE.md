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
