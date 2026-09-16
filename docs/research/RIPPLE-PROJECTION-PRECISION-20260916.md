# Native ripple projection mismatch

Saved32-frame run render-20260916-083200 has matching count18, six emitted faces, colors, screenXY and material/tag words, but38 differing float words. Exact vertex hash comparison remains a failure. Neither hash omission nor tolerance relaxation is justified.

`artifacts/ripple-precision/depth-inference.json` derives camera depth from captured reciprocalZ. Only near-edge vertices1/2/3,7/8/9,13/14/15 differ; the opposing edges match. Xbox inferred depth is about0.86 to1.02e-6 larger. This is an inference from rounded reciprocal values, not a direct captured camera-depth measurement. UV-over-Z and reciprocalZ differences are coordinated, pointing first to denominator depth rather than independently changed texture coordinates.

The56-byte vertex is14 assigned32-bit fields with no padding. generate_source fills position/color/texture/material/lightmap fields; ripple overrides color and tag. Arbitrary padding bytes do not explain the hash.

Next capture must compare existing rf_scene_ripple_camera[12] and rf_scene_ripple_sources[16][6], then pre-projection world triangles/UVs, associated shot/mesh/face IDs and emitted count. Those inputs separate VFX/placement from camera projection. Persistent copies are needed because the scene/mesh owner is released before completed guest telemetry is collected. Use virtual words()/memsave, not physical framebuffer address masking.

Potential bounded fix site, not yet established: preview.c camera() has unstaged world-camera subtraction and three float += product sums. Its nearby clipping helpers already use volatile float rounding barriers. If camera/source inputs agree bitwise, explicitly rounding camera deltas/products/accumulations to binary32 should be tested before any change to comparison policy. Existing VFX decode/morph/UV code uses more explicit staged float/double operations, but that is not proof this particular asset sample is exact without its input dump.

No rendering changes were made in this audit. Original-game screenshots or captures are neither needed nor used.

## Source localization and rocket rendering correction

Run `render-20260916-083627` captures identical camera12 and ripple-source96 floats on PC/Xbox, but36 of180 populated preprojection floats differ. Source export captures12 admitted triangle faces (two-sided faces), while18 final vertices represent six visible faces. Therefore the first observed mismatch precedes projection. Do not change camera math or weaken the exact comparison based on the earlier reciprocal-only inference.

The original authored ripple uses non-morph transform sampling (flags40). `artifacts/ripple-precision/fpu_sweep.py` opens the actual WaterRipple01.vfx meshes through the existing compiled NXDK mesh loader and executes the existing NXDK sampler at captured effect frame7.75 in Unicorn. Its default CPU state reproduces all12 admitted PC world corners exactly. Sweeps of x87 precision/rounding and MXCSR alone do not yet explain the complete captured mismatch; world placement in these probes is a separately rounded Python addition, so these are isolation evidence, not whole-render CPU emulation. Capture `rf_scene_ripple_fp_state[4]` records frame+1, x87 control, MXCSR and supported before the first actual instance update; it never changes floating-point state. Native/PC capture comparison is pending.

Separately, the authored dm03 wet gameplay fixture exposed a real rocket-render failure at frame60. Tiny animated rocket triangles were translated to large world coordinates before binding through the strict terrain collision-plane validator. The rocket draw path now keeps rotated geometry relative to the rocket center and translates a copied camera by the opposite amount. This preserves geometry, view and culling while avoiding large plane constants and tiny-triangle precision loss. The terrain validator is unchanged. Primary reports the180-frame PC fixture now completes and reaches solid impact83; liquid contact/input pose verification remains separate. No Xbox validation of this correction is claimed here.
