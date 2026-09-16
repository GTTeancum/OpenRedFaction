# Ripple lighting callback audit

The existing mixed-light `rf_vfx_lighting` is the appropriate numerical shader,
but the original ripple visibility callback explicitly disables its per-object
sphere light selection. Do not copy the terrain lightmap lookup or assume that
the authored visibility radius3 is a light-selection sphere.

## Newly executed boundary

Original RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Run `python artifacts/crater-shading-re/liquid_vfx_light_dispatch.py`.
Its JSON output is `artifacts/crater-shading-re/liquid-vfx-light-dispatch.json`.

* Actual4c1e10 iterates a supplied one-instance list and registers precisely
  `(instance,position,position,3,4c1dd0,1,0,0,0,0,1)` through4d3560.
* Executing4d367a..4d36aa with those captured arguments writes node bytes
  +14..18 as `[1,0,0,0,1]`. In particular +17 is zero; +18 is one.
  The registration allocation/frustum prefix is not executed by this probe.
* Whole4d3460 with the resulting node and a valid room calls the draw callback
  directly, without4d99c0 or4d9fa0. A positive-control node with +17=1 calls
  sphere selection, draw, then reset. Null-room and disabled-global controls
  also pass. Selection/draw/reset are recording stubs, not executed shaders.

The precise field writes are4d369d (+17 from arg10) and4d36a0 (+18 from
arg11). The lighting gate is4d347a..4d3499; reset gate4d34ae..4d34c1.
This is evidence against inventing radius3 selection, not proof that no lights
ever affect a ripple: active-list ownership above/below this callback remains
to be traced. The callback's503100 routes through502b20; its type3 VFX branch
uses54d0a0 ->53ee90 ->53ef70 ->516f50 ->553ee0. The initial report incorrectly
associated it with the separate type1 model path516ec0 ->52fa40; the followup
below corrects that static inference. Default parameters are500f80/500fb0.

## Retained shader evidence and current gap

Original553ee0 computes an averaged, normalized normal from each edge's
incident faces and calls4daff0 with ambient enabled, that edge's vertex
position, its normal, and gain2. Mesh flag0x10 selects white instead. Material
brightness is then a minimum floor, followed by tint. These math components
are already reconstructed in `rf_vfx_lighting`, `rf_vfx_material_color`, and
`rf_vfx_face_normal`; the cache/selection/mixed-light pipeline has retained
1024-case original/PC/NXDK coverage in `tools/verify_vfx_cached_lighting.py`
and `docs/VFX-RENDERING.md`. That older matrix was not rerun for this audit.

`scene_ripples_draw` currently converts the level ambient directly to bytes,
uses the same RGB for all three corners, and never calls the mixed-light
shader. It therefore cannot reproduce spatial or normal-dependent light
variation. Conversely, substituting the ground lightmap would be a different
shader: original VFX evaluates lights at vertex positions and does not read
the contacted terrain face's lightmap in553ee0.

## Minimal integration shape

Use a small scene callback accepting world position, averaged normal, and a
caller-owned prepared-light context; call `rf_vfx_lighting(position,normal,
ambient,.25f,sources,count,rgb)`, then `rf_vfx_material_color`. The .25 scalar
is original5a38e0's initial value, also used by existing scene terrain helpers;
`scene_stream.light_directional` is a level Boolean and is not that scalar.

Prepare sources once per effect draw, then shade each unique edge/corner.
Reuse the owned `s->lights->pool`, ordered `rf_vfx_light_pool_cache` and
`rf_vfx_lights_prepare` once the actual active-list selection is established.
Do not select directly over raw pool slots if original list order matters.
The terrain helper's room0 assumption, 63-light shadow masks and ground
lightmap visibility policy are not a drop-in VFX context. Use the contact room
and room ambient override once their caller policy is proven; retain room ID
on each ripple if needed. No per-frame heap allocation is required.

A callback alone is insufficient: the present geometry emitter assigns one
uniform RGB after clipping. Correct per-point shading also needs incident-face
normal averaging and interpolation of RGB for newly clipped vertices. The
flat authored ripple may simplify normals, but no general all-edges-equal
assumption was executed in this audit. Keep the ambient-only implementation
explicit until these ownership and interpolation details are supplied.

Recommended next bounded proof: follow active-list countc9687c and ambient
5a38d4..dc around sorted ripple submission to553ee0. Establish whether
the list is cleared, inherited, or selected inside renderer dispatch before
enabling additional live lights. No shared source edits, builds, native runs,
or original screenshots were performed here.

## Followup: actual VFX route and bounded ambient correction

`liquid_vfx_ambient_lifecycle.py` now passes four executed cases plus an
active-count inheritance control. This is a composition of bounded original
instruction ranges, not a whole original rendered scene.

* Non-directional level initializer461950..461a02 uses authored RGB multiplied
  by float0.003921568859368563 (5895dc), then calls4d8ce0 to set global ambient
  5a38d4..dc. There is no half factor. Its sole direct call to the setter is
  4619fa. Room binder4d3350 changes room/fog, not these ambient globals.
* Whole4d9fa0 clears active countc9687c and selection flagc96890. The ordinary
  backend room renderer55f5e0 ends by calling it at56142d. Main4d45d0 invokes
  this renderer through516d10 before looping visible rooms and4d4c60 effects.
  That outer ordering is statically inspected; the full geometry renderer and
  all intervening callbacks were not emulated.
* Actual53ee90 ->53ef70 ->516f50 reaches553ee0 with the supplied active count
  and ambient unchanged for active type0 mesh, flags0x40, backend0x66. The
  shader endpoint is recorded. A zero-count input stays zero; a sentinel count7
  also survives. Thus this route does not choose additional lights itself.
  Existing4d3460 proof shows the ripple callback also omits its optional sphere
  selection; callbacks that enable that selection reset afterward.
* Full unhooked4daff0/4da8b0 with lighting enabled, no active lights, ambient
  enabled and gain2 produces RGB32/64/128 from authored16/32/64, and
  128/192/254 from64/96/127. Inputs128/192/255 saturate to255/255/255. Current
  direct ambient conversion instead outputs the authored byte values.

The supported immediate correction is to call existing `rf_vfx_lighting`
with the unhalved level ambient and zero sources for the explicitly supported
non-directional ambient-only ripple path, followed by material brightness/tint.
No new normal averaging or clipping interpolation is needed when every vertex
gets this constant result. This removes the demonstrated missing gain2 while
leaving future spatial lighting separate. Do not use room ambient overrides or
the terrain helper's half ambient: that would change the proven input.

For a future general callback, the exact shader context is current global
ambient; ordered active source list; original lighting-enabled gates; position
and averaged normal in the same current transformed space; directional scalar;
ambient enabled; null visibility weights; gain2.553ee0 calls4d9fd0 at554075 to
transform existing sources, not to select them. Its alternate-view flag88fd1c
instead forces mesh0x10 white. Those cases need caller-state support.

Remaining limits: no full-scene guarantee that every preceding callback leaves
the list empty; no alternate/skyview claim; directional level option4619a2
creates a directional source through4d8d40 instead of setting this ambient,
so passing directional-level RGB as ambient would be unsupported. The previous
suggestion to use contact-room ambient was premature and is superseded here.
