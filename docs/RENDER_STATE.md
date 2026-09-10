# Original renderer evidence

Applies only to RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

## Solid mode initialization

`tools/inspect_render_state.py` executes original instructions at 0x515730 and
the constructor at 0x411e00 under isolated Unicorn x86 emulation. They write
0x00400024 to 0x01808328. The constructor packs six five-bit fields; the resulting
values are [4, 1, 0, 0, 4, 0]. This establishes the initializer's result, not every
later mutation or draw-path choice. Ghidra identifies reads in functions 0x4f0c00
and 0x561650. Disassembly and emulation result are in artifacts/render-state.json.

## Texture-stage state

Raw Ghidra export of 0x54f160 is in artifacts/analysis/rf_b8fb9ab4c9bf/54f160.c.txt.
The vtable +0xfc calls match D3D8 SetTextureStageState. Constants were checked in
the local MinGW d3d8types.h, not inferred from a modern renderer.

- Texture-source case 4 selects stage-0 texture color and stage-1 MODULATE (4),
  except color-source 3 uses ADD (7). Stage-1 arguments are TEXTURE (2), CURRENT (1).
- Case 5 uses stage-1 MODULATE2X (5).
- At entry, texture source 4 can be changed to 5 when byte 0x01cfcc1d equals 1.
- The multitexture paths set min/mag filtering to LINEAR (2), with base UV wrapping
  and lightmap UV clamping in the relevant paths.

The writer and meaning of 0x01cfcc1d still need tracing. Texture conversion may
also change lightmap intensity before sampling. Therefore Open Faction's use of
EMT_LIGHTMAP_M2 is not sufficient evidence for unconditional doubled lighting.
Do not claim complete recovered render-state semantics from this partial export;
fog, alpha, blending, format conversion and alternative passes remain open.

## Capability selector resolved

Ghidra xrefs identify a write at 0x545f46 in 0x545d50:
`flag = (TextureOpCaps >> 4) & 1`. The source DWORD at 0x01cfcb58 is offset 0x90
from the D3DCAPS8 object at 0x01cfcac8 (the TextureOpCaps field). Local D3D8 headers
define D3DTEXOPCAPS_MODULATE2X as 0x10. Function 0x5463d0 logs this flag under
the original `D3Dm2x` label; 0x546a00 checks it when deciding mode support.
Thus the earlier unknown flag is a device capability, and doubled modulation
is the supported two-texture route. Texture conversion/intensity quantization
and exceptional render modes still need independent verification.

## Particle blend, depth and fog state recovery

`tools/verify_particle_render_states.py` executes the complete original 0x54f160
with texture mode 1 and an empty batch. A fake COM device records calls at
vtable offsets 0xc8 and 0xfc; no GPU or desktop input is involved. The original
mode getters, branches and cache comparison execute unchanged. Across 3780
cases, `rf_particle_render_decode` matches the ordered render-state writes and
color/alpha/fog selector values on PC and compiled NXDK x86 under Unicorn.
Another 3780 original calls verify that an unchanged packed mode skips state
updates even after the supplied blend capability bits change.

The six five-bit fields are texture, color, alpha, blend, depth and fog, at bit
positions 0, 5, 10, 15, 20 and 25. The C helper reconstructs the state-selection
tail only; callers must implement the preceding cache, flush and texture-stage
logic separately. It emits at most ten original D3D8 state/value pairs into a
96-byte caller-owned record, with no heap allocation. The backend must translate
these values explicitly; they are not NXDK GPU register encodings.

Blend mode 0 disables blending without rewriting factors. Modes 1, 2/4, 3,
5, 6 and 7 select factor pairs (2,2), (5,2), (5,6), (9,1), (10,1) and (9,3).
Mode 3 without capability bit 0x10 writes source factor 12 only. Mode 5 without
bit 0x100 emits no blend writes. These preserve-state cases must not be replaced
with an assumed default. The capability word comes from original 0x01cfcaf4.

Depth modes 0 through 5 reproduce the original enable, write and comparison
calls, including mode 3 disabling depth testing while enabling depth writes.
Mode 5 adds alpha testing with reference 16 and comparison value 7. Depth kind
at 0x017c7c4c chooses enable values 1/2/0 for kind 0/1/other, and comparison
7 for kind 0 versus 4 otherwise. Unknown modes emit no writes for their category.
Fog modes 0..2 follow the low byte of 0x017c7c20; mode 3 disables fog. Vertex
fog is enabled only when fog is enabled and the kind at 0x005a7df8 equals 2.

This is not a native GPU render test. Polygon clipping, billboard depth bias, residency and scene/backend integration
remain open. Particle defaults and texture-source 2 are covered below. The existing world/actor renderer has not been switched to this helper.

## Default particle passes

`tools/verify_particle_modes.py` executes startup initializers 0x50be10 and
0x50be40 with the original constructor 0x411e00. Their resulting words are
0x00118c42 at 0x017c7c58 (ordinary) and 0x06110c42 at 0x01775b30 (glow).
In [texture, color, alpha, blend, depth, fog] order these are [2,2,3,3,1,0]
and [2,2,3,2,1,3]. The original selection span 0x494c8f..0x494cbc chooses glow
when particle flag 2 is present. Flag 0x2000 calls 0x496a30 to clear only the
five-bit depth field. Selection matches C on PC and compiled NXDK for 1031
cases, including arbitrary caller-supplied replacement modes and unrelated bits.
The public defaults describe startup values, not a claim that globals can never
change later.

Both defaults use texture-source case 2 in 0x54f160. Its twelve ordered
texture-stage calls match PC/NXDK across all 32 color-source values, all 32
alpha-source values and four raw LOD-bias patterns (4096 cases). The first
write passes original global 0x005aa7f0 to stage 0 MIPMAPLODBIAS (state 19).
U/V addressing is CLAMP (3); min/mag filtering is LINEAR (2). Default color and
alpha operations each MODULATE (4) texture (2) with diffuse (0), and stage 1
color operation is DISABLE (1). State names/constants were checked in local
MinGW d3d8types.h. The helper passes LOD bias as raw bits without conversion.
Thirty-one unsupported texture sources preserve output and return RF_NOT_FOUND;
these are API guards, not original behavior claims.

Combined with the recovered render states, ordinary particles select SRCALPHA /
INVSRCALPHA (or the original capability fallback) and environment-controlled
fog; glowing particles select SRCALPHA / ONE and disable fog. Both normally
test depth without writing it. The no-Z flag disables depth testing and writes.
No texture binding, pixel shading, native GPU output or scene effect execution
is established by these function-level checks.

## Billboard classification and submission depth

`rf_particle_billboard_prepare` joins the recovered corner geometry with
original depth bias and clip masks in a caller-owned 108-byte packet. It does
not allocate memory. `tools/verify_particle_billboard_prepare.py` executes
0x555230 through 0x5554c9, including the original vertex constructors, geometry,
0x518660 and 0x5475d0. Only bitmap dimension lookup is supplied, with the center
marked already projected. All packet bytes match PC and compiled NXDK in 2048
cases: 832 wholly rejected, 32 crossing clip planes and 1184 inside/no clipping.
Eight invalid-input guards preserve output (API behavior, not original claims).

Submission depth is center Z minus camera scale Z times radius, but only when
that product is strictly less than center Z. The product remains at the verified
53-bit x87 precision for the comparison and subtraction, then rounds to float.
Corner Z stays at the original center Z. Original 0x5587c0 projects the corners
before replacing Z and reciprocal Z for depth override; moving the corners
closer before projection would incorrectly enlarge the billboard.

Clip classification at 0x5475d0 uses strict side-plane inequalities: x > z is
bit 8, y > z bit 32, x < -z bit 4 and y < -z bit 16. When depth classification
is enabled, z <= 0 adds bit 128; the optional far plane adds bit 2 when z exceeds
the far distance. Global switches are tested as bytes and nested: disabled
clipping suppresses every bit; disabled depth classification also suppresses
the far-plane check. The packet exposes AND/OR masks across all four corners.
A nonzero AND rejects the quad; a nonzero OR may require polygon clipping.

Actual polygon clipping (0x549e00), projected/depth-adjusted vertex submission,
backend texture residency and visible scene effects still need integration.

## UV-only billboard polygon clipping

`rf_particle_billboard_clip` reconstructs 0x549e00/0x549bd0 and the relevant
0x549310 intersections for packets from billboard preparation. It processes
planes 2, 4, 8, 16 and 32 in ascending order. Classification never emits plane
1 or 64 for these billboards; the helper rejects those unsupported masks.
Bit 128 is retained, not converted into an invented near-plane intersection.
The result retains an AND mask: a nonzero value means the polygon is rejected.
The caller still performs the original trivial rejection before invoking the
clipper and controls whether polygon clipping is enabled.

Each pass starts at vertex 1, wraps through vertex 0, and computes crossings
from the inside endpoint toward the outside endpoint. Interpolation retains
double precision until position/UV stores. Side-plane intersections derive Z
from the rounded matching X or Y coordinate with the appropriate sign. A fixed
48-slot temporary pool and index references preserve original 0x549270,
0x5496e0 and 0x5492d0 reuse order. This matters: a previously freed record can
still be referenced by a neighbor in the current pass. Replacing references
with value copies removed duplicate intersections and disagreed with RF.exe.

`tools/verify_particle_clip.py` executes the unchanged original clipper and
allocator on 2048 prepared billboards, matching all ordered position, UV, mask
and count bytes against PC and compiled NXDK C. Output counts are 0:734,
3:210, 4:690, 5:370, 6:41 and 7:3. Six unsupported/nonfinite input guards preserve
output. The output record is 304 bytes with room for twelve vertices. Local
arrays use 52 compact vertex records, 28 indices and 48 free indices (1552
bytes), plus the 304-byte result and scalar/compiler stack overhead. Allocation
is bounded and uses no heap. Capacity overflow returns RF_RANGE preserving
output rather than writing outside those arrays.

This coverage is the UV-only particle pass (original draw flag 1), not clipping
of arbitrary world geometry with per-vertex color, lightmaps or custom planes.
Connecting clipped vertices to projection, depth override, native backends and
live campaign effects remains open. No new GPU output is claimed by these tests.

## Projected particle submission

`rf_particle_billboard_project` joins trivial rejection, optional polygon
clipping, point projection and the final billboard depth override in the order
of original 0x5587c0. Its caller-owned output is 388 bytes, holding up to twelve
32-byte vertices (camera XYZ, screen XY, reciprocal Z and UV). Rejected
polygons return an empty result. Invalid input preserves the output.

`tools/verify_particle_submission.py` executes the full unchanged 0x5587c0,
including original polygon clipping, projection and temporary-vertex cleanup.
The 0x551900 call records its submitted vertices instead of drawing. All
submission fields and draw/reject decisions match PC and compiled NXDK C in
2048 cases: 1178 submitted and 870 rejected. Counts among submissions are
3:7, 4:1079, 5:82 and 6:10. Cases vary projection clamp low-byte behavior,
projection depth offset, clipping enable, far plane, position and orientation.
No scene camera or renderer is assumed by this fixture-driven test.

Clipping is gated by the projection clamp byte and the original OR mask;
trivial AND-mask rejection occurs regardless of that gate. Each surviving
point is projected using its unbiased position, then camera Z and reciprocal
Z are replaced with the billboard override. The screen coordinates and UVs
remain unchanged. Zero override depth retains the original infinity behavior
under the verified masked floating-point environment. The helper does not
reproduce original pointer identity or temporary flags in its public output.

Backend conversion, texture residency/binding, color/fog conversion, batching,
GPU rasterization and live campaign emitter execution remain open. This
submission evidence does not claim that visible particles are integrated.

## Particle draw-vertex conversion

`rf_particle_vertex_encode` reconstructs the UV-only flag-1 path of 0x551900.
It produces a separate 32-byte particle record: screen XY, depth, reciprocal W,
ARGB, fog in the high byte and UV. The original record stride is 40 bytes, but
this path leaves the last eight secondary-UV bytes untouched. The world vertex
ABI is unchanged. Texture binding and batch index submission remain separate.

`tools/verify_particle_vertex.py` executes original 0x551900 with state binding
supplied and index submission suppressed. The original 0x550780 color transform,
0x52fc70/0x52fcb0 fog conversion, integer clamp and depth helper execute unchanged.
All first 32 output bytes match PC and compiled NXDK across 4096 cases; the
original untouched eight-byte tail is checked too. Cases cover arbitrary current
RGBA, low-byte color/alpha/transform switches, positive and negative transform
scales, fog clamps and half-step rounding, and depth/reciprocal/UV scale factors.

Current color bytes become ARGB with the original channel order. Disabled color
or alpha selects 255 for the relevant channels. The optional transform sums RGB
once, multiplies that sum by each channel's scale, truncates and clamps to 0..255.
Fog first stores float(255 - fog_scale * camera_Z), clamps to 0..255, adds
12582912 and takes the low byte of the rounded float representation. This retains
the original rounding behavior under the verified 0x027f environment. It does
not substitute truncation or an arbitrary modern fog formula.

The helper rejects nonfinite environment values used by the path and invalid
integer-conversion ranges, preserving output. Original per-vertex color lookup,
other draw flags, texture binding/residency, native GPU batching and live scene
effects are outside this helper. Function-level GPU-record agreement does not
establish a rendered particle image.

## Xbox particle backend scaffold (native pixels unverified)

`rf_xbox_particle_draw` and the new particle Cg shaders compile with NXDK. The
pass uses immediate attributes and borrows the native image's contiguous,
swizzled pixels; it allocates neither a vertex buffer nor a texture copy. It
sets its own shaders, texture sampling, blend, alpha-test, fog and depth state,
then waits for GPU completion before returning. The caller owns pbkit startup,
the current render target and later state restoration. It must run after world
submission and before presentation; it is not yet wired into that loop.

The supported mode domain is the verified default ordinary/glow particle mode,
with normal or disabled depth testing. Other modes return RF_NOT_FOUND.
Ordinary blending uses SRC_ALPHA / ONE_MINUS_SRC_ALPHA; glow uses SRC_ALPHA /
ONE. Depth writes remain disabled. Fog is supplied through the particle shader;
hardware fog is disabled to avoid applying it twice. The RGB fog parameter uses
red in its low byte. Texture coordinates are prepared for projective sampling.
Native sampling, interpolation, blending, fog and depth behavior still require
framebuffer evidence; shader compilation does not prove those behaviors.

The reconstructed depth value and diagnostic world depth have different
representations. The adapter therefore accepts an explicit affine conversion
`target_depth = depth_bias + depth_scale * reconstructed_depth`, targeting the
existing forward-Z buffer with LEQUAL. Choosing those parameters belongs to the
camera/backend integration, not the reconstructed core. Nonfinite backend
vertices are rejected even though the core preserves some original infinities.

Next: add isolated XEMU pixel probes for ordinary/additive blending, fog and
world-depth occlusion, then connect the pass and its PC equivalent to bounded
texture residency and campaign emitter execution. No screenshot or rendered
particle validation is claimed by this scaffold.
