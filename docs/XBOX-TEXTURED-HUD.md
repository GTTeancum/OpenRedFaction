# Ordered screen-space textured HUD

Source-written 2026-10-08 after the user-directed 19:27 UTC switch to HUD
implementation. Renderer worktree starts from the tested19:00 a687235 source.
No helper build, runtime, new fixture, screenshot or desktop automation was
performed; the parent owns the20:00 consolidated batch.

## Scope and callback

The Xbox compositor now supplies a dedicated `scene_hud_present` callback to
flash, combat HUD, blackout and endgame in their existing order. It keeps the
shared `rf_scene_particle_sink` signature. Solids remain image=NULL/mode0x18000.
Textured HUD calls use RF_PARTICLE_NORMAL_MODE (or its no-Z form) with a borrowed
native `rf_image`, screen-pixel positions, normalized UVs and ARGB tint. The
adapter selects depth scale1/bias0 and clears depth mode; it never applies
world reciprocal-Z conversion or camera scope scaling. Other mode families
are rejected rather than silently changing their blending.

The scene/asset owners supply logical sprite sizes as screen rectangles.
Padded POT dimensions are only the texture descriptor size. UV subrectangles
can cover logical NPOT art or atlas cells without any renderer resampling.
The renderer supports the existing native swizzled RGBA8 or packed1555 image
storage. HUD images are size-checked (<=4096 per dimension and exact storage
bytes); the existing upload descriptor also enforces nonzero POT dimensions.
It performs no new allocation, texture reduction or per-texel alpha scan.

The existing particle fragment shader samples projected UVs, multiplies texture
RGB/alpha by tint and adds supplied fog RGB. HUD calls disable fog and emit zero
fog color, so this is ordinary source-alpha sprite composition. No implicit
color key or opacity is invented. Asset decoding owns any source alpha policy.

## Ordered pipeline-state ownership

HUD state is now identified by mode, native pixel pointer and exact texture
format. A full state setup occurs on first use or any mismatch. Same-texture
HUD fans can retain their installed shader/texture state. The separate world
particle cache, standalone completion behavior and world projection are not
changed.

Before every HUD state setup, `hud_batch_close_primitive` appends END and
publishes all pending solid commands. Thus no independent `pb_begin` writer
can overwrite an unpublished packet. A fallback fan similarly closes/publishes
any solid list before issuing its own BEGIN. Uniform-color3/4-vertex batching
now explicitly requires a solid draw; textured input cannot accidentally take
the UV-omitting solid path. Texture fans retain the existing full per-vertex
color, fog, UV and position emission, preserving all interpolation and alpha
ordering. Returning to solids reinstalls the solid shader, disables texture
sampling and starts with an explicit first color.

Before first/changed HUD texture binding, an x86 `sfence` publishes any native
write-combined image stores. This also covers lazy first-use asset decoding
inside the HUD callback after the frame's earlier bulk vertex fence. Reused
images must stay unchanged through the whole HUD pass. The fence adds no GPU
wait or allocation and does not permit resource mutation while borrowed.

## Capacity, completion and error paths

A shader/state install is budgeted before its first command, using the existing
conservative8 control words +5 per vertex-program instruction +128 state words.
Repeated texture changes therefore cannot overrun the32,768-dword HUD segment
before the later draw admission check. The current generated solid/particle
state blocks are84/90 words, below that128-word allowance. Individual fallback
vertex packets retain20 words; solid packets keep their tested128-word limit.

Mid-pass reset closes the primitive, publishes the packet, waits for complete
idle and only then calls pb_reset. GPU state itself survives a command-buffer
reset, so an unchanged installed state remains usable. Begin/end of the HUD
scope invalidate retained texture identity. Final HUD completion still closes
and publishes, waits for full idle, and only then returns to framebuffer
publication, simulation or resource retirement, including error returns.

All validation failures before submission leave earlier pending draws intact;
the compositor always calls the same final HUD completion before returning an
error. Failed texture descriptors never borrow a new image. No caller vertex
pointer survives a draw call and no new pixel ownership survives HUD completion.

New native state is28 bytes: mode/format/pixel pointer plus four counters.
`rf_xbox_hud_texture[0..3]` reports textured fans, texture-state installs,
texture-state reuses and solid/texture state switches. Existing HUD input/vertex
counters still include every shape. Their fallback-fan count now includes
textured fans by design; that nonzero count alone is not a regression.

## Integration and parent verification

Renderer support alone does not load art or replace the placeholder HUD. The
asset worker owns original ui.vpp decoding, NPOT padding, font metadata and
resource budgets. The scene worker owns retained lifetime and live-state
composition. Their draws use the unchanged callback contract above.

At20:00, use the parent's existing bounded stock64MiB scene. Read the four new
texture counters alongside existing HUD/renderer counters, establish successful
textured calls and mixed state switches, and confirm clean completion and memory
bounds. Preserve the exact tested XBE before any restoration repack. No image
capture or independent per-slice fixture is needed. Actual performance and
rendered-asset behavior remain unverified until that batch.

##21:00 source continuation

`XBOX-HUD-TEXTURE-BATCH.md` extends the tested20:00 textured fan path to
uniform ordered triangle runs with explicit per-vertex UV/rhw and zero fog
state. Earlier fallback/full-vertex descriptions above record the20:00 baseline;
nonuniform/larger fans still use that original path. No ownership, projection,
blend or completion guarantee is relaxed.
