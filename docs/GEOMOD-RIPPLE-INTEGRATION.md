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
