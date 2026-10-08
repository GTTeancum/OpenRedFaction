# Identical texture-binding omission

Source-written2026-10-08 against92033be, awaiting the parent's17:00 run.
No helper build, test, fixture or runtime measurement. No gain is claimed.

## Grounded redundant work

XEMU's NV097 texture OFFSET and FORMAT handlers mark that texture unit dirty
even for an identical write. Its OpenGL binding path has a fast reuse branch
when the binding is not dirty and the underlying resource has not changed;
otherwise it repeats surface/cache lookup work. The ordinary NV2A optimization
is simply to retain unchanged register values, not to alter emulator state.

Primary source at478b4f496102379c7eaa7f3ec10e714a703c4300:

- https://github.com/xemu-project/xemu/blob/478b4f496102379c7eaa7f3ec10e714a703c4300/hw/xbox/nv2a/pgraph/pgraph.c
- https://github.com/xemu-project/xemu/blob/478b4f496102379c7eaa7f3ec10e714a703c4300/hw/xbox/nv2a/pgraph/gl/texture.c

No emulator implementation was copied into the project.

## Exact scope

Indexed-world drawing tracks the actual26-bit GPU pixel address and texture
format independently for units0 and1. First use always binds both. Consecutive
groups omit a binding pair only when both values are exactly unchanged. The
cache lives on the draw function's stack and cannot escape that pass. Material
constants and each group's BEGIN/END/indices still execute in the same order.
The existing array fallback is unchanged.

Retained-model drawing derives the previous binding from the immediately
preceding queued request only when `install_program` is false, which proves an
uninterrupted same-kind draw run. First request, shader change or CPU-mesh
boundary forces the original writes. Comparisons use GPU address plus format,
not a material ID or texture descriptor address. No new cross-frame pointer
cache is introduced. Different material alpha/blend/depth values, object pose,
shader constants, vertex addresses and palette writes remain live per draw.

Scene image contents are unchanged during these private render runs. Later
frame/level/image changes start fresh bindings; existing upload and lifetime
fences remain. No texture quality, caching lifetime, visibility or memory-budget
policy changes.

`rf_xbox_texture_reuse[0..3]` counts world binding pairs written/reused, then
retained-model unit0 pairs written/reused. It resets with the existing frame
request batch. A binding pair means OFFSET plus FORMAT, not a draw. This adds
only16 bytes of telemetry and small automatic comparison state, no heap/GPU
allocation. The retained-model white lightmap already follows the prior
same-kind invariant policy and is not counted as a new omission here.
