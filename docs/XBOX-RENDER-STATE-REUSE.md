# Exact redundant Xbox render-state writes

Written2026-10-08 after the retained-world reorder, for the parent's16:00
batch. No helper build, test, new fixture or runtime measurement. This only
omits consecutive identical GPU state writes within existing draw boundaries.
No draw order, scene content, simulation, detail, resolution or pacing changes.

## Retained models

The `preview` loop already supplies `install_program` to `retained_model_render`:
it is true at the first retained draw at a CPU-mesh offset, and whenever the
rigid/skinned kind changes. The loop performs no intervening rendering while
it is false. The new conditions reuse that existing boundary, without adding
another model-state owner or cache.

These values are invariant during that same-kind run and are written only when
`install_program` is true:

- Shader constantsc4/c5/c6: depth parameters, fixed tint and literal values.
- Back-face selector; per-part cull enable and per-draw front-face remain live.
- The same white fallback bound to texture unit1.
- The16 disabled vertex-attribute formats and the active rigid/skinned formats.

`retained_model_cache` derives stride solely from rigid/skinned kind. Every GPU
source address still updates per draw. Per-draw view/projection, material,
texture unit0, alpha blend/depth mask and all per-part bone palettes remain
unchanged. Bone palettes start atc8 and cannot overwrite the fixedc4–c6 range.
Returning to CPU geometry or changing shader kind forces the full setup again.
The omitted commands total57 dwords per same-kind rigid draw or61 per skinned
draw; this is a source count, not measured time or FPS.

## Particles and coronas

Inside the existing bounded streaming particle pass, a setup is reused only
when exact mode, GPU pixel address and texture format match the preceding draw.
Those inputs completely determine the setup block. Fog, color, UV and depth
conversion remain in immediate vertex commands and are always regenerated.
Mode or image changes install the complete original state in the same order.

The cache is invalidated at every GPU drain and pass exit, including errors.
Standalone synchronous calls and existing HUD batching are unchanged. Added
retained state is16 bytes on Xbox; no image copy, allocation, extra image
lifetime, generic pipeline cache or cross-frame reuse is introduced. Existing
`rf_xbox_particle_batch[3]`, previously reserved, counts reused setups.

The provided baseline contains only four particle/corona fans, so that part is
not presented as a large baseline-FPS gain. The retained-model case removes
identical work from ordinary object drawing; its measured effect remains for
the parent comparison. No model counters were captured in the baseline report.

## Separate post16:00 tail omission

Prepared against the frozena653795 source without changing that checkout.
After a retained draw run at the final CPU-mesh offset, the old renderer
reinstalled the preview shader, constants and vertex attributes, then broke
without issuing any CPU draw. The existing end-of-mesh break now occurs before
that unused restore. Intermediate runs still restore all state exactly as
before; particles/HUD and the next frame keep their own complete setup. This
is a source-written separate delta, not part of the accepted16:00 freeze, and
no performance or visual result is claimed.

## Separate post16:00 clear/submission overlap

The same held post16 input replaces only the CPU `pb_busy` spin immediately
after the depth/color clear with a queued `NV097_WAIT_FOR_IDLE` before the draw
commands. GPU clear completion still precedes drawing, while the CPU can prepare
the following commands without waiting for the clear to finish. No reset, CPU
framebuffer read, buffer write/reuse or resource release occurs across this
boundary. Lightmap/vertex lifetime fences, the final draw/HUD drains, VBlank
and `pb_finished` are unchanged.

Source basis: nxdk `pbkit.c::pb_erase_depth_stencil_buffer` and
`pbkit_draw.c::pb_fill` append clear methods to the same pushbuffer; `pb_end`
submits asynchronously unless optional trace mode is enabled. `pb_busy` polls
both DMA positions and PGRAPH busy status. `nv_regs.h` defines the GPU
WAIT_FOR_IDLE method0x110, already used by the frame-end path in `pb_finished`.
The nxdk triangle/mesh examples also wait on the CPU after clearing; their
wait was not assumed unnecessary hardware ordering, so the GPU barrier remains
explicit in this change. The required full depth clear is also retained.

The parent baseline phase4 (clear plus VBlank) is1.643ms; its actual CPU-wait
share is unknown. Time can move from phase4 to phase5 without improving FPS.
Only the same-scene total frame/presented-FPS measurement can establish a gain.
No build or execution was performed; this is not in the accepted16:00 freeze.
