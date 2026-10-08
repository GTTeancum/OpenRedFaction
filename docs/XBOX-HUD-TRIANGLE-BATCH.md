# Ordered solid HUD triangle batching

Source-written2026-10-08 against29a8660; integrated overa39fc09 with the
newer exact finite input guards retained, awaiting the parent's18:00 batch.
No helper build, test, new fixture, emulator run or image capture. The17:00
same-scene result was20.8078 XEMU presented FPS; HUD command generation/bounded
waits cost21.455ms, versus world submission0.228ms, model/CPU submission0.741ms
and geometry completion3.134ms. These precede the change; no gain is claimed.

## Actual redundant draw work

Each HUD rectangle/text run previously opened and ended its own TRIANGLE_FAN.
The64-fan batching delayed waits but did not combine those primitive draws.
`combat_hud_rect`, screen flash and blackout supply four vertices with uniform
ARGB. Their two fan triangles are exactly012 and023.

Inside the existing private solid-HUD scope, uniform-color3/4-vertex inputs now
share an open TRIANGLES list. A quad contributes exactly012023, in the original
rectangle and alpha-compositing order. Color is emitted once, followed by its
six positions in one35-dword block. The current immediate attribute value
persists for those vertices. No sorting, shape merging, pixel dropping or
text/font/layout change is performed. Uniform color avoids any difference in
flat-shading provoking-vertex convention between fan and list.

The solid fragment shader returns COLOR0 only. Its unused fog and UV attribute
writes are omitted in this admitted path. Position/depth conversion and the
same normalized ARGB values remain. All API input validation is still applied,
even to unused inputs. Nonuniform colors and larger fans close the pending
triangle list and use the complete original fan/emission path. Standalone and
textured particle/corona behavior is unchanged.

The native immediate-attribute behavior is also represented directly by XEMU's
`pgraph_allocate_inline_buffer_vertices` / `pgraph_finish_inline_buffer_vertex`:
changing a color preserves preceding vertices and each position commits the
current attributes. Primary reference, no emulator code copied:
https://github.com/xemu-project/xemu/blob/478b4f496102379c7eaa7f3ec10e714a703c4300/hw/xbox/nv2a/pgraph/vertex.c

## Bounded commands and lifetimes

The fixed64-fan reset threshold is replaced with an explicit32768-dword
(128KiB) ceiling inside the512KiB pushbuffer. Admission charges exact vertex
packets and BEGIN/END work, plus a conservative initial shader/state charge:
eight control words, five words per vertex-program instruction, and the
existing128-dword maximum state block. A packed quad block is35 words and a
triangle block20, each below the per-begin/end128-word limit. Fallback vertices
retain their original20-word packets.

The list is always ended BEFORE a guarded reset, fallback fan or final/error
completion. This is essential: never wait/reset with an unterminated primitive.
Full pre-reset idle guards remain because pb_reset has a void timeout path.
The final HUD fence still completes all draws before capture publication,
frame return or resource reuse. Commands contain copied values; there is no
borrowed vertex pointer, new heap/GPU allocation or cross-frame draw owner.

`rf_xbox_hud_batch[0..7]` reports input fans, input vertices, batched fans,
fallback fans, GPU primitives, emitted GPU vertices, resets and peak charged
command words. Expanded quad vertices are reported honestly; fewer primitives
must not be mistaken for fewer HUD shapes. The new state/telemetry costs only
40 bytes plus a six-byte immutable index pattern. Existing renderer phase
counters and actual presented FPS remain the performance evidence.
