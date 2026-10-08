# Ordered textured HUD triangle batching

Source-written 2026-10-08 from tested11bfc4d for the parent's21:00 batch.
No helper build, runtime, fixture or image operation was performed.

## Measured baseline and reason

20:00 completed240 neutral original L1S1 frames in stock64MiB at42.761682
actual XEMU FPS. Atlas residency was2,107,168 bytes and the recorded loading
peak2,181,880 bytes; all240 frames used original HUD art without asset fallback
or draw errors. The last frame had492 HUD input fans:484 solid triangle-batched
fans and8 textured fallback fans. These emitted2,936 GPU vertices across9
primitives. The8 texture fans used one state setup and7 exact state reuses.

Richer subtitles/messages and weapon overlays increase textured glyph calls.
The existing fast path already preserves uniform quad order and command bounds.
Extend it to the same admitted texture state so glyphs do not require one GPU
primitive apiece. No current or future gain is claimed before the hourly run.

## Exact admission and vertex attributes

- The private HUD scope already admits only solids or ordinary no-Z textures,
  with fog disabled. Inputs still pass all existing finite and storage checks.
- Uniform-color3/4-vertex fans use the triangle path for either kind. A quad
  emits exactly012/023, with each original vertex's own position and UV/rhw.
  Nonuniform colors or larger fans retain the complete old fallback path.
- Every textured emitted vertex writes all four TEXCOORD0 input components:
  `(u*reciprocal_w,v*reciprocal_w,0,reciprocal_w)`, before its XYZ position
  commits the vertex. This uses exactly the existing multiplication expression;
  no UV value is cached, inferred, clamped, resampled or taken from another glyph.
- Position still uses the tested native XYZ method with w=1. The same position
  depth expression, shader, source-alpha blend and no-Z state remain in force.
- The textured fragment shader adds COLOR1 fog RGB even when fixed-function fog
  is disabled. Its state setup now explicitly writes zero to that immediate
  attribute once, so texture batches may omit repeated per-vertex zero writes.
  The HUD rejects enabled fog; fallback textured vertices also write zero.
- Exact color reuse only lasts while the current primitive is open. A state,
  image, fallback, reset or pass boundary closes it and forces first color.

The existing atlas loader supplies normalized logical UV rectangles with a
one-texel replicated gutter. Batching retains those caller UVs and the same
sampler settings; it does not change filtering, alpha or atlas ownership.

## Bounds and completion

A solid vertex remains4 words. A texture vertex is9 words:5 UV/rhw plus4 XYZ.
Thus an admitted textured quad needs at most59 words including a color write;
that fits the128-word command-packet helper, which can combine two such quads.
Budget admission conservatively includes color, BEGIN/END and pending closure;
actual charged words track copied commands. The32,768-word segment bound is
unchanged. The additional5-word zero-fog state method raises the current
textured state block from90 to95 words, still below its128-word allowance.

Before a shader/texture owner writes, the same existing close routine appends
END and publishes pending commands. No batching crosses an exact state-key
mismatch or changes draw order. Fallback/reset/error/final paths retain their
published-END and full idle guards. First/changed-texture sfence and all image
lifetime rules from XBOX-TEXTURED-HUD.md remain. No new heap, GPU or static
storage is introduced.

## Diagnostic interpretation

The existing `rf_xbox_hud_texture` still counts textured input fans, state
installs/reuses and solid/texture switches. The batched count now includes
admitted textured fans; fallback counts only genuinely unbatched shapes.
GPU emitted vertices legitimately increase from4 to6 per newly batched quad.
This does not mean extra HUD shapes or altered geometry coverage.
`rf_xbox_hud_reuse` actual/former packets describe the batched command path
with/without packet packing; shader state and fallback packets stay excluded.

For the unchanged20:00 final HUD content, expect492 input fans,2,952 emitted
vertices and zero fallback fans. Consecutive texture glyphs may share one
primitive, separated from the preceding solid list by its existing state
boundary. Scene/font changes can legitimately change those counts at21:00;
verify the current content, atlas status, successful completion, bounded memory
and actual presented FPS together. Parent owns all runtime validation.
