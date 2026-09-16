# Authored ripple integration (2026-09-16)

Water contacts now start WaterRipple01.vfx in a bounded16-instance pool. The owner supports V4 global materials and preserves legacy embedded materials. Size/orientation use authored coordinates and identity, opacity animates at elapsed_seconds*15, and lifetime is96 fixed60Hz ticks. Lighting currently uses scene ambient plus the authored brightness floor; point lighting remains open.

Inspection caught a black rectangular patch from ordinary alpha blending. Binary material-mode evidence requires SRC_ALPHA/ONE, depth read only. A separate additive fade tag now implements that on both backends without altering ordinary fades. PC render-only fixture output was inspected: the patch disappears, subtle bright ripple detail remains, and expiry matches control exactly. This is not an authored liquid collision acceptance test.

Validation:
- CTest vfx_liquid_assets, rocket_visual_resources and geometry_liquid_overlay pass. Overlay test covers eight replacements, four resets and rejected binds.
- tools/verify_water_ripple.py passes at24/96/97frames:14952 changed color channels at24, zero darkened channels; effect expires at97 and frame equals control.
- PC and NXDK builds succeed.
- Native stock64MiB32frame fixture artifacts/xemu/render-20260916-082530 renders correctly on inspection, but strict RIPPLE_VISUAL comparison FAILS: vertex hashes3332121627Xbox vs2836671916PC. All other ripple words match (one active, six faces,18vertices,80129ownedbytes, no error). Numeric vertex investigation remains open; do not label native parity passed.
- Owned native run exited and all24 staged disc entries were verified restored.

A real wet dm03 location/replay is documented in research/WATER-FIXTURE-DM03-20260916.md; live pose/loadout integration remains necessary. No original screenshots, desktop input or GitHub image uploads were used.

Checkpoint transport was added separately: bounded dual slots with pure validation callback, readback and corruption fallback. Its PC tests pass. It is not yet wired into RFDS or native HDD saves, and fflush/fclose do not establish hardware durability.

Estimate remains approximately49% overall,66% GeoMod. Current area: destruction/water effects and save foundations.

## Numeric comparison follow-up

The32frame native export at artifacts/xemu/render-20260916-083200 retains all18 final56-byte vertices. Of252 words,38 differ: perspective texture/reciprocal depth words differ by a few ULP and some24-bit depth values by one unit; screenXY, RGB, material and opacity tags match. This localizes investigation but does not establish the upstream cause or change the strict comparison. Bounded persistent copies and PC/QMP export now preserve evidence after scene teardown. All24 disc entries restored. No original screenshot reference is used.

The terrain history checker now builds a candidate through the same prepare path as restore and always aborts it after a synchronous visitor. Focused tests verify successful and rejected visits, all truncations, budgets, unchanged live terrain and another cut matching uninterrupted control. Existing transaction/interior regressions pass. This enables pure save selection without publishing candidates; complete RFDS validation and persistent HDD integration remain open.

## Wet gameplay and ambient correction

PC authored dm03 tests now establish liquid contact64, floor impact66, one real-contact ripple and expiry; see tools/verify_water_gameplay.py. Projectile-relative rendering fixes a real large-coordinate rocket triangle validation failure. The close blast kills the player and obscures later visuals; safer placement and wet Xbox/audio are open. Ambient-only VFX now applies the recovered2x gain through rf_vfx_lighting; four numeric tests pass, as do the asset tests and render fixture. Directional lighting retains its explicitly unverified fallback. NXDK compilation passes.

The expanded native084420 dump shows x87CW027f and MXCSR1fa0 on both PC/Xbox. Camera and ripple center/time match;36 preprojection XYZ/UV words differ. Therefore the difference begins before projection and cannot be attributed merely to different current FP control. Linked native sampler/caller probes run in isolation match PC; live source remains open. Strict native comparison stays failed, with all24 staged disc entries restored. This small precision investigation continues alongside gameplay implementation.
