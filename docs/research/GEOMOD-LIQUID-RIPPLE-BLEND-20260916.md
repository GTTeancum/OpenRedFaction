# Ripple blend correction (2026-09-16)

The opaque dark rectangle reported by primary has a concrete source mismatch:
ripples require alpha-additive blending, while the ordinary fade tag used alpha
replacement blending. Black RGB texels with opaque alpha therefore darkened the
floor in the port; additive blending leaves the destination unchanged there.

`artifacts/crater-shading-re/liquid_vfx_blend_mode.py` executes original50be70 and
411e00 without substitutions, constructing global17756b8 =0x06110c41. Its six
5-bit fields are1,2,3,2,1,3. The actual ripple MATL word2/offset8 equals1.
Executing553ee0's selector554617..554667 with that value reaches5159a0 with
3vertices, vertex format5, and this exact mode. The probe stops at submission;
it does not execute a D3D device or claim a stock visual capture.

The retained renderer mode definitions in
`local/alpine-reference/game_patch/rf/gr/gr.h` identify those fields as wrap,
vertex-times-texture RGB, vertex-times-texture alpha, alpha-additive blend,
depth-read-only and no fog. The retained D3D11 state implementation maps
alpha-additive to SRC_ALPHA/ONE. This is consistent with the original named mode;
a full original D3D7 device-state execution was not needed to distinguish it
from the port's explicit SRC_ALPHA/ONE_MINUS_SRC_ALPHA implementation.

## Implemented change

New reserved `RF_PREVIEW_ADDITIVE_FADE_TAG`0xfffffc00 carries scalar opacity.
Existing ordinary0xfffffd00 fades retain their behavior. The common fade predicate
recognizes both for vertex RGB, alpha scaling and half-open triangle edges.

PC raster computes saturated `source*alpha + destination` for the new tag and
never writes its depth, including alpha255. Xbox uses SRC_ALPHA/ONE and read-only
depth; changing modes explicitly restores both factors for ordinary rendering,
including when retained model draws invalidate renderer caches. Ripple emission
alone selects the new tag. No sampler or arbitrary brightness gain was changed.

Primary owns builds and reconstructed captures; this change is not visually
validated by this report. Ambient-only lighting, original depth-list ordering,
and exact opacity-byte rounding remain separate limitations. Linear wrapped base
sampling is already the matching intended mode, and does not explain the black
rectangle. Authored reverse faces rely on the existing CPU front-face rejection;
no additional GPU culling change was introduced.
