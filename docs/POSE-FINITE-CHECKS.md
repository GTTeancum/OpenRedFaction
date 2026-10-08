# Exact finite predicates in pose and motion hot paths

Source-written 2026-10-08 against 92033be. No helper compilation, test or
emulator run. Parent owns the17:00 consolidated check. No FPS gain is claimed.

## Grounded cost

The parent16:00 profile records4.892ms in NPC pose advance/evaluation. Existing
Xbox objects, produced by that parent build, show an avoidable cost inside it:
NXDK's isfinite macro calls the external PDCLib __fpclassifyf/__fpclassify
functions. Their implementation executes x87 fxam and fstsw.

The existing rf_model_compose_transform object has two classifier calls in its
12-iteration input loop and12 unrolled output calls:36 dynamic classifier calls
for each valid composition. make_transform, blend validation, motion weights
and decoded position keys also issue those calls. This is source/object
inspection, not a new benchmark or proof of how much time those calls consume.

## Change

New include/rf/finite.h supplies a typed float predicate using an
alias-safe representation copy and an IEEE exponent mask. Compile-time
assertions check the supported PC/Xbox binary32 size/radix/precision
assumptions. All NaNs and infinities fail; both signed zeros, all subnormals and
all ordinary finite values pass. There is no floating-point arithmetic in the
predicates and no relaxed-math compiler option.

Scope is limited to:
- Float checks in the model transform, skeletal blend/evaluation and tag block
- Typed float checks throughout the motion playback/sampling translation unit
- Float checks in decoded motion-track/key access

Every guard, original error return and interpolation/math operation remains.
This does not remove validation, approximate trigonometry, change tick cadence,
change poses or introduce a runtime cache/allocation.

## Compiler and precision boundaries

NXDK passes -ffreestanding -fno-builtin, and existing object code also shows
ordinary fixed-size memcpy calls remain out of line. The Clang/GCC branch
therefore spells the fixed representation copy __builtin_memcpy; MSVC retains
memcpy. The parent must inspect resulting target code to confirm constant-size
copies lower as intended and the transform no longer calls the classifier.

All original double isfinite guards deliberately remain, including motion
phase 'advanced' and packed rotation output 'value'. In current x87 object code their
external calls impose binary64 spills before later arithmetic/int conversion;
removing that boundary without a parity check could change excess-precision
lifetime. This patch does not force new rounding or alter those boundaries.
The angle guard is also left unchanged, keeping this slice float-only.

No claim is made that incidental floating-point exception/status flags remain
identical; the finite/nonfinite guard results and gameplay math are preserved.
Ordinary masked IEEE operation is the existing supported execution environment.

## Pending parent checks

Existing model/motion cases include NaN, infinity/range rejection, signed
values and transform/interpolation behavior. Use bounded existing checks and
inspect the new Xbox objects, then compare the same64MiB neutral L1S1 scene and
actual presented FPS. Source inspection alone does not establish the gain.

## Integration precision exclusions

The combined17:00 source conservatively retains the original classifier calls
in make_transform's rotation/position guards, pose_interpolate's output guard,
controller elapsed-output comparison, phase-loop weight guards and position
interpolation's output guard. Those calls may force live double/float locals
to storage between arithmetic steps on x87. No new rounding is imposed. The
large composition input/output validation loops and isolated memory-value
predicates retain the bit-test optimization. Parent target-code inspection
and the scheduled runtime pass remain pending.
