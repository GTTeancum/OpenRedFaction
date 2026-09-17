# Liquid conversion and mixed floating-point state

The pending moving-debris liquid crossing exposed a native-only precision drift.
Instrumentation isolates it to frame269, the first motion tick after a bit-identical
birth. Proposed positions and collision-query deltas match; only six fragments'
post-step vertical velocities differ. Neither render pass changes motion state.
x87 control is0x027f and MXCSR0x1fa0 on both platforms throughout the sampled phases.

The NXDK liquid-height cast lowers to fnstcw/fldcw/fistp/fldcw, temporarily selecting
truncation. The following gravity update uses scalar SSE divss/addss. Explicit
float stores for gravity, spin and contact offsets made no change; that experiment
was removed. Keeping mode registers unchanged is therefore not sufficient evidence
that the emulator's mixed arithmetic path remained equivalent.

The bounded double-to-integer height conversion now decodes the IEEE double sign,
exponent and mantissa with integer operations. Existing range checks restrict the
shift and signed result; conversion truncates toward zero without changing floating-
point control. The mathematical liquid height and external API are unchanged.
All225 original liquid-miss fixtures still pass, and the live five birth placements
and35 moving crossings still match original instructions bit-for-bit on PC.

Native evidence:

- `render-20260917-001551`: birth hash matches; motion diverges at269; rendering
  changes no motion hashes. Mode registers and RNG match.
- `render-20260917-001812`: full first-step vectors isolate the discrepancy to
  vertical velocity after gravity; before/proposed/delta vectors match exactly.
- `render-20260917-002209`: after replacing the conversion, all captured first-step
  vectors, all128 phase records and all16 sound-dispatch ledger rows match exactly.
  The unchanged DEBRIS_AUDIO acceptance check passes. This is strong evidence for
  an interaction with that mode-changing conversion in this XEMU path, not a claim
  about physical Xbox hardware or a general emulator defect independently reproduced.

The full pending water-crossing run is still NOT accepted: comparison proceeds
through27 groups then fails RIPPLE_VISUAL's emitted-vertex hash (Xbox3983100954,
PC1308216040). Both report16 active ripples,64 emitted faces and192 vertices.
Ripple geometry precision remains a separate open investigation; no hash comparison
was relaxed. This commit contains only the conversion fix, not the pending live
water-crossing integration or its temporary diagnostic instrumentation.
