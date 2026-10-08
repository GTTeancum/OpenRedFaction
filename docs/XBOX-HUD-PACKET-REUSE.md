# Ordered HUD command-packet and color reuse

Source-written 2026-10-08 after the parent's 18:00 batch, for the 19:00 batch.
No helper build, runtime, fixture, screenshot or image operation was performed.

## Measured motivation

The 18:00 original L1S1 neutral-spawn, stock-64-MiB run of d0b1b10 completed
240 frames. The unpaused observer measured 37.10831899 XEMU presented FPS:
149 frames in 4.015272156 seconds after warmup. This is the integrated result,
not an isolated attribution to any one change or a hardware measurement.
HUD draw generation now averages 3.205 ms, versus 21.455 ms at 17:00. The last
frame still emits 528 uniform quads with 2,112 input and 3,168 GPU vertices.
They share one GPU triangle-list primitive, with zero fallback fans, one reset
and 18,640 peak charged command words. These are existing baseline readings:
`artifacts/hourly/20261008-1800-optimized/{performance,hud-batch,measured-fps}.json`.

The 18:00 implementation publishes each quad through its own `pb_end`, plus
one BEGIN and END: 530 triangle-list command packets, excluding shader setup.
The list also repeats all four normalized color components for each quad even
when consecutive text rows have the exact same ARGB value.

## Written change

- Use the existing `renderer_command_batch` helper for the private solid HUD.
  It publishes a packet before admitting commands that would exceed 128 dwords.
  The diagnostic `rf_xbox_command_batching_disabled` control remains honored.
- Copy every position and changed color immediately into owned pushbuffer
  storage. No caller vertex pointer survives the draw call. Adjacent calls may
  share a packet, but no primitive, shape or pixel is merged, skipped or sorted.
- Reuse the immediate color only when the same triangle list is still open and
  the complete 32-bit ARGB value matches. A new primitive always writes its
  first color. Therefore every fallback, reset and pass boundary invalidates
  reuse without depending on global shader or particle state.
- On a changed color, retain exactly the previous float normalization and
  `NV097_SET_VERTEX_DATA4F_M` command. On a match, skip only that identical
  five-dword method. Position/depth arithmetic and all finite checks remain.
- Keep exact fan expansion (012 and 023) and the original alpha-compositing
  order. The original shader, draw topology admission and fallback path remain.

For a same-color run, one BEGIN, one color and four quads fit in 127 dwords.
Color changes may reduce that to three quads; the helper uses actual word
counts rather than assuming a shape count. Each shape consumes at most 35
words and therefore fits below the existing 128-dword helper contract.

## Completion, capacity and ownership

`hud_batch_close_primitive` appends END when needed and then publishes any
pending command packet, including on an otherwise-empty close. This happens
before the old idle wait/reset, before fallback fan submission, and at final
HUD completion after either success or error. No open `pb_begin` packet reaches
an independent command writer, framebuffer publication, simulation or owner
retirement. The HUD callbacks route all GPU work through this private sink.

The separate 32,768-dword pass-segment bound is retained. Admission still
conservatively reserves space for a color write and BEGIN/END; bookkeeping
charges only actual written commands, including currently unpublished words.
Reset closes and publishes the packet, waits for full idle, then recycles it.
The existing void-timeout `pb_reset` preguard is unchanged. No allocation,
borrowed data lifetime or persistent GPU resource is introduced.

New stock-Xbox storage is 32 bytes: three command-batch pointers, one retained
color and four counters. `rf_xbox_hud_reuse[0..3]` gives emitted colors, omitted
identical colors, actual packets and former packets, for the batched triangle
path only; shader setup and fallback fan packets are excluded. The existing
`rf_xbox_hud_batch` shape/vertex/reset/capacity counters remain unchanged.

## Parent-owned verification

At 19:00, use the same scene and actual-FPS observer and also read four words
from `rf_xbox_hud_reuse`. Expect 528 source fans and 3,168 emitted vertices if
the same final HUD state is reached. Packet reduction must not be reported as
shape reduction. Confirm clean completion, no fallback/regression, bounded
peak words and the final fence. No performance improvement is claimed until
that consolidated run; no independent test was added or executed.
