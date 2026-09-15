# Crater shading: face-property inheritance audit (2026-09-15)

Bounded read-only RE of RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

No new shading correction was established. In particular, the original face inversion does **not** add directional shading, change a material, or brighten an excavated face.

## New executable evidence

`tools/verify_geomod_face_properties.py` executes original code in isolated Unicorn:

- `4e0254..4e0268`: the actual six-DWORD property copy in face clone `4e0240`, starting after allocation with a supplied destination. All 24 bytes at face+28..3f are copied verbatim.
- Complete `4e2a00`, including its actual `4e3b00`, `40a3f0`, and `409f40` arithmetic callees: a three-corner ring reverses from [0,1,2] to [2,1,0]; plane (.25,-.5,.75,2) becomes (-.25,.5,-.75,-2). All six property DWORDs remain unchanged.
- Fourteen flag patterns include actual template flags0x100, individual low flags and all bits set. Every case passes. Results: `artifacts/crater-shading-re/face-properties.json`.

Existing independently generated template evidence in `artifacts/geomod-holey01-original.json` gives all16 template property records `[0x100,0,0xffffffff,0xffff0000,0xffffffff,0]`. Thus these tested clone/invert operations do not transform those template properties into a brighter/fullbright material. Raw Ghidra `4dd780` calls clone4e0240 then inversion4e2a00 for its reverse-result branch; this caller selection was inspected, not executed here.

## Implication and limits

Do not add a normal-sign-based brightness multiplier or change face properties when subtracting terrain based on the assumption that original inversion did so. The tested code does not. This narrows that explanation; it does not prove every later flag mutation, the full original CSG pipeline, renderer output or original visual parity. No stock visual reference was obtained or claimed, and no shared-source modification follows from this audit.

Known noise-fill, dirty8, texture-scale and MODULATE2X results were read for context and not counted as new evidence. The strongest next visual investigation remains a verified stock-game comparison at matching geometry/view; current evidence alone cannot identify the dark appearance as a specific shading regression.
