# Exact HUD XYZ emission

Source-written 2026-10-08 on top of the ordered HUD packet/color reuse change,
for the parent's 19:00 batch. No helper build or runtime was performed.

The admitted solid HUD emits every GPU position as `(x,y,depth,1)` using
`NV097_SET_VERTEX_DATA4F_M`. NV097 also provides `SET_VERTEX3F`, which assigns
x/y/z to the same position attribute, supplies w=1 and completes the vertex
when z arrives. Use that method only in the uniform solid HUD triangle path.
The copied x/y values, evaluated depth expression, immediate color and triangle
order are unchanged. Every input finite check still executes before emission.
Nonuniform/larger fallback fans, textured particles and standalone calls retain
the former four-component position method.

Primary source evidence, no code copied:
- XEMU `DEF_METHOD_INC(NV097, SET_VERTEX3F)` sets position attribute 0 and w=1,
  then calls the same inline-vertex completion operation used by DATA4F:
  https://github.com/xemu-project/xemu/blob/478b4f496102379c7eaa7f3ec10e714a703c4300/hw/xbox/nv2a/pgraph/pgraph.c
- Hardware-oriented primitive tests exercise `NV097_SET_VERTEX3F` submission:
  https://github.com/abaire/nxdk_pgraph_tests/blob/b5949789e2ec1f946614b89228f93de977c30f58/src/tests/three_d_primitive_tests.cpp
- Local NXDK defines the method in `lib/pbkit/nv_regs.h`; its existing
  `pb_push3f` writes one method header and exactly three float words.

Each admitted vertex now consumes four command words instead of five. The
18:00 HUD's 3,168 emitted vertices therefore save exactly 3,168 dwords
(12,672 bytes) per equivalent frame, without reducing submitted geometry.
Each quad costs 24 position words plus five when its color changes. One BEGIN,
one color and five same-color quads fit in 127 words. Admission/bookkeeping
now use the four-word method size; maximum shape reservation is 29 words and
the existing 128-word packet / 32,768-word pass limits remain intact.
All END publication, fallback, reset and completed-frame fences are unchanged.
No allocation or additional persistent state is introduced.

The parent's existing neutral original L1S1 stock-64-MiB run should still show
528 fans and 3,168 emitted vertices at the same HUD state, with lower charged
words. Actual FPS, clean completion and all batching counters must be read at
19:00 before claiming a gain. Source equivalence is not runtime validation.
