# Liquid ripple VFX setup and reusable owner (2026-09-15)

`liquid_vfx_create.py` executes original4c17d1..4c18bc, supplying pool allocation,
VMesh resource creation and recording action calls. Five requested sizes-1,0,.5,2,10
produce identical original position and identity basis with null direction.
The instance starts action0, weight1, hold-last-frame0 after stopping all actions.
The stored instance+40 value is authored VFX radius3, independent of requested
size. Actual vector constructors/copies execute. No original game is launched.

Disassembly4c1e29..4c1e44 uses radius3 for visibility registration. Render callback
4c1dd0 creates default render parameters and passes position/orientation to503100;
it does not pass that radius as a mesh scale. Water contact passes null direction,
so use identity orientation and authored mesh coordinates, not the rocket basis
or contact size as a geometry multiplier.

Original4c1d50 checks action0 state through5033d0, deleting instances whose returned
value equals double constant589cc0, then calls503360 with current frame seconds,
instance position/orientation and LOD1 for survivors. This report does not claim an
executed action-lifetime boundary; duration must be derived from action timing
rather than inventing a fixed splash timeout.

Installed `WaterRipple01.vfx` is7153 bytes, version40006. It contains four SFXO
meshes and four global MATL records. Existing lower-level PC mesh/instance/material
bank probes successfully load all four, update geometry at effect frames0,.5,2,
and mark inactive at10000. Numeric outputs are retained in
`artifacts/crater-shading-re/liquid-vfx-owned.json` and mesh timing in
`liquid-vfx-meshes.json`. Each mesh has4vertices/4faces,17samples at10Hz, start0,
end1.6333333. The shared instance API takes15Hz effect-frame units; feed elapsed
seconds*15 without the rocket's modulo16 loop. Local Y is already approximately
+.000572 above the plane, so do not introduce an arbitrary extra offset without
checking depth behavior.

Global MATL bank is1497 bytes on32bit PC. Three referenceWaterRipple01.tga; one
referencesWaterRipple_Core01.tga. Every material has one brightness sample0 and
24 opacity samples at15Hz. Brightness is a minimum lighting floor in the recovered
color function, not a direct black multiplier. Opacity differs per layer and fades
to0; one authored terminal sample is slightly negative, handled by the existing
clamping sampler. There is no legacy embedded color_word in this format.

## Shared owner implementation

`rf_vfx_geometry_asset` now owns `material_bank` and records `version`. Legacy
SFXO-only paths remain; version4 accepts SFXO/MATL records, verifies mesh count
against the recovered header mesh-object count, bounds mesh IDs against the actual
material bank, and includes the retained bank in allocation budgets and cleanup.
The32-mesh limit applies to meshes rather than total record count. Unknown record
types still fail instead of silently dropping effect contents.

`tests/vfx_liquid_asset_tests.c` covers real ripple ownership after archive close,
exact/short peak budgets, out-of-range global material reference, header mesh-count
mismatch, unsupported chunk type, truncated final chunk, repeated close and legacy
rocket loading. Primary owns build registration/execution; this agent has not
built the changed owner. The scene/material wrapper integration remains primary's
scope. No animation/render behavior was added by this change.

## Executed lifetime boundary

Additional `liquid_vfx_lifetime.py` executes whole4c1d50 with actual5033d0 and
501bd0 action-state calculation; resource destruction and frame processing are
recorded services. For the actual header's24 effect frames and no particle
emitters, frame values0,2,23,23.999 retain/process the VFX;24,24.001,25 delete it
before processing. Completion sentinel589cc0 is double0.0. The action lifetime is
therefore24/15 =1.6seconds, independent of the per-mesh1.6333333 timing endpoint.
The effect owner should retire at this action boundary, not wait for every mesh
instance to become inactive. This closes the earlier unexecuted lifetime item.
