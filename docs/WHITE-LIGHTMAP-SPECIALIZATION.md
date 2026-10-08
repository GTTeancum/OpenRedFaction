# Held retained-model constant-white lightmap specialization

Selected by parent for17:00 after material source review. Base0892a7b; no build, emulator,
pixel capture or helper test. Root owns integration and hourly measurement.

Every retained rigid/skinned model emits TEXCOORD1=(0,0,0,1) and binds the
renderer-owned1x1RGBA0xffffffff fallback to clamp-to-edge texture unit1. Its
sample is exactly RGB1. The generic fragment path still performs that texture
lookup for every fragment.

NV2A register-combiner ONE is ZERO(source0) with UNSIGNED_INVERT(mapping1).
The original stage1 color input word0xC4C90000 reads V0(source4) and TEX1
(source9), both signed-identity mapping6. Replacing only B with ONE produces
0xC4200000. Both existing combiner iterations, stage1 SHIFTLEFTBY1 and output
clamping remain. Stage0 color/alpha, stage1 discarded alpha and final alpha
source remain unchanged. This does not algebraically collapse stages or alter
intermediate precision.

At each retained-model shader-run boundary the code selects texture stage1
NONE, disables unit1 and writes that exact stage1 color operand. There is no
state cache spanning unrelated draws. Before any following CPU geometry the
normal stage program0x21, unit1 enable and original stage1 input are explicitly
restored; its existing binding cache is already reset. Each next frame still
installs the full normal fragment setup before world drawing. Particle/HUD
passes keep their independent full setup. The fallback image remains alive
for primary untextured draws and other passes.

Primary semantics: bundled nxdk NV097 register masks and XEMU
pgraph/psh_regs.h PS_REGISTER_ONE = PS_REGISTER_ZERO | PS_INPUTMAPPING_UNSIGNED_INVERT.
No emulator code was copied. The constant case is narrowly proven by the
existing shader outputs, sampler state and owned fallback payload, not by
assuming arbitrary real lightmaps are white.

Expected effect is one fewer texture sample for retained-model fragments;
actual cost and complete runtime behavior are unmeasured. World lightmaps,
models, geometry, colors, alpha, depth, visibility, simulation and pacing are
intended unchanged. Parent17:00 owns the first runtime measurement of this specialization.
