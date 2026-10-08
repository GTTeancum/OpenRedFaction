# Visible-only retained-world index submission

Source-written2026-10-08 againstdf4066d for the17:00 parent batch. No helper
build, test, emulator run or new fixture. The measured16:00 input was17.35086
XEMU presented FPS with3003 world vertices/575 faces,196 array ranges and24
material/lightmap groups. Rendering plus GPU waits still cost30.121ms. Those
measurements precede this change; no gain is asserted here.

## Command semantics

The bundled nxdk `nv_regs.h` defines ARRAY_ELEMENT16/32 at0x1800/0x1808 and
non-incrementing method packets. XEMU's primary source at commit
478b4f496102379c7eaa7f3ec10e714a703c4300 confirms:

- ARRAY_ELEMENT16 appends low16 then high16; ARRAY_ELEMENT32 appends one index.
- BEGIN/END preserves the resulting element sequence.
- The OpenGL array path issues a multi-draw array call; its indexed path issues
  one indexed draw. Vulkan issues one draw per array range versus one indexed
  draw for the assembled group.

Primary sources (reference only; no emulator code copied into the engine):

- https://github.com/xemu-project/xemu/blob/478b4f496102379c7eaa7f3ec10e714a703c4300/hw/xbox/nv2a/pgraph/pgraph.c
- https://github.com/xemu-project/xemu/blob/478b4f496102379c7eaa7f3ec10e714a703c4300/hw/xbox/nv2a/pgraph/gl/draw.c
- https://github.com/xemu-project/xemu/blob/478b4f496102379c7eaa7f3ec10e714a703c4300/hw/xbox/nv2a/pgraph/vk/draw.c

This is ordinary NV2A indexed geometry, not a dummy command or emulator-specific
shortcut to force a backend case. The complete original visible index sequence
is submitted to the GPU.

## Implementation

`retained_world_indices.h` builds an optional uint16 index list from the exact
existing `retained_face_visible` decisions and sorted descriptor order. Each
visible face contributes its unchanged triangle fan's uploaded vertex indices.
Hidden faces contribute no indices. Same-material/lightmap faces can now share
one indexed group across invisible gaps; the original triangle and group order,
UVs, colors, depth/blend state and texture bindings stay unchanged.

Up to120 packed index pairs are written in one non-incrementing packet, within
the existing128-dword begin/end command limit. Odd group tails use one ordinary
ARRAY_ELEMENT32 containing a valid16-bit index. Each group has a complete
triangle count and one BEGIN/END. No mixed array/element tricks, GPU readback,
new GPU vertex buffer or frame allocation is used.

The CPU list is reused only when camera-position bits and every relevant room's
visible decision match exactly. Orientation/zoom still update GPU constants
normally; if they change portal visibility, the room snapshot invalidates the
list. Plane tests depend on position and immutable face planes, not orientation.
New geometry/level/retained-world teardown frees the cache. The cache stores
source indices, never simulated objects or an alternate visibility policy.

## Bounds and fallback

The optional index list, group table, room snapshot and bookkeeping are charged
against the same4MiB retained-world cap, in addition to existing GPU vertices
and face metadata. Allocation failure leaves the original array path usable.
Worlds above65535 vertices use that path too. No vertex/index value is truncated.
The full selected index command stream, including conservative common setup,
is bounded to128KiB; larger views use the unchanged array path.

`renderer-world-indices-off.flag` selects the original sorted array submission
without changing geometry, materials or visibility. Existing world-grouping-off
and command-batching-off modes also retain their old array behavior. No flag is
needed for ordinary indexed play. No synthetic scene or benchmark-only switch
turns this optimization on.

## Existing-scene evidence to collect

`rf_xbox_world_indexed[0..7]` gives active mode, charged cache bytes, visible
indices, indexed groups, packets, rebuilds, reuses and fallback reason. Reasons
are0 none,1 index domain,2 budget,3 allocation,4 command bound,5 explicit mode.
`rf_xbox_retained_world[4:6]` retains visible vertex/face counts; its slot6 is
array draw ranges or indexed groups according to the active mode.
`rf_xbox_world_groups[0]` is material groups and slot1 preserves the equivalent
contiguous visible span count, even when indices bridge those gaps.

The next natural-scene run must retain3003 vertices and575 visible faces at the
same captured view; a smaller submission count is not evidence of fewer visible
objects. Actual presented FPS and total timing determine whether the extra index
commands beat the reduced host draw ranges. No global floating-point precision,
model culling policy, resolution, draw distance, pacing or gameplay is changed.
