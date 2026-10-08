# Original weapon scope overlays

Source-written for the21:00 parent batch on2026-10-08. The parent batch passes
stock64MiB resource admission: both scope images are loaded with131120 resident
bytes and139312 accounted peak bytes. The neutral run does not activate zoom,
so scope draw count is zero; active-scope presentation is still unverified.
No helper build, test, emulator run, screenshot or image generation was performed.

## Resources and ownership

`rf_hud_scope_assets_open` optionally owns two original128x128 RGBA images:

- Sniper: `maps2.vpp/scope_zoom_corner256_red.tga`
- Scoped assault rifle: `maps1.vpp/ass2_scope_circle.tga`

Original `0x4a9980` loads these into `0x5a0b38` and `0x5a0b34` respectively.
They remain unmodified, full-resolution images. The owner borrows no archive
after loading and does no per-frame I/O or allocation. Requested admission is
256KiB, including131072 pixel bytes, owner metadata and8KiB loader scratch.
Allocator/page-rounding overhead remains outside the byte counters. The base
2MiB HUD atlas is unchanged. Failed admission leaves NULL and preserves the
existing zoom/reticle fallback.

## Source-derived static composition

`0x4ac3b0` and `0x4ac7a0` query the viewport and source bitmap dimensions. For
640x480 they draw four240x240 quadrants spanning x80..560, y0..480. Top-right
mirrors U, bottom-left mirrors V, bottom-right mirrors both. This preserves
the original source pixels; geometric presentation scaling is intentional.

Drawing order:

1. Full-screen source-alpha tint: sniper RGB(180,0,0), alpha90; assault
   RGB(0,180,0), alpha90 (`0x4ac447`, `0x4ac83f`).
2. Four mirrored texture quadrants, white tint for sniper and green for assault.
3. Opaque black horizontal margins (`0x4ac562..0x4ac599`, `0x4ac94e` onward).
4. Fixed black aiming axes. Sniper uses full-viewport2px axes with alpha64
   (`0x4ac5dc..0x4ac615`). Assault uses1px axes with alpha128: vertical height
   0.7H at y=cy-0.35H; horizontal width0.2W at x=cx-0.1W
   (`0x4ac98a..0x4aca17`). Source binary32 operands/truncation are retained.

The ordinary reticle routine `0x43a401..0x43a415` exits when zoom is positive.
Suppress the generic/normal reticle while this optional owner is presenting;
keep it if scope resources were not admitted. The composer does not change
the existing practical two-state zoom, sensitivity, firing, ammo or lock state.

Variable reticle marks, source13-frame `scope_numbers.vbm`, four-frame
`scope_light.vbm` and `ass2_scope_numbers.tga` are deliberately not loaded or
drawn. Their live-state/readout semantics have not been integrated. No zoom
number, range measurement, lock notification or magnification label is faked.
The original circle decorations beyond the fixed aiming axes are deferred.

## First-playable scope status, reviewed after21:00

The current scope feedback is sufficient for the first-playable code slice.
Existing fresh-edge alternate input toggles zoom only for an owned Sniper Rifle
or Scoped Assault Rifle while alive and on foot. The existing scope owner drives
the real FOV/projection and look-sensitivity scaling; the corresponding original
mask and fixed aiming cross distinguish the two weapons. Losing selection or
being unable to use the weapon resets zoom. Cinematics suppress scope drawing.
The mask is submitted before readable messages and ordinary meters. Optional
resource-load failure preserves the existing scope/reticle fallback.

No additional zoom label or invented measurement is needed to communicate this
two-state aiming behavior. Variable-magnification numerals, animated scale/light
art, range labels and lock indicators without a real bound gameplay state remain
deferred fidelity work. They are not blockers for this first-playable slice.
Ammo, reload and low-health clarity remain separate scene-owned HUD work.

Evidence: `src/core/weapon_scope.c`, the scope step in `src/diagnostic/scene.c`,
`scene_hud_scope.inc`, and the parent
`artifacts/hourly/20261008-2100-optimized/run-result.json`. Resource admission
must not be described as a successful active-zoom draw or visual parity check.

## Parent integration hooks

The isolated `src/diagnostic/scene_hud_scope.inc` is supplied to avoid taking
ownership of scene/message composition. Apply these narrow hooks after the
message worker's changes:

- Add `#include "rf/hud_scope.h"` near the HUD asset include.
- Add `rf_hud_scope_assets *hud_scope;` beside `scene_stream.hud_assets`.
- Include `scene_hud_scope.inc` immediately after `scene_hud_assets.inc`.
- After `scene_hud_open_optional(stream,tables_path,maps,map_count)`, call
  `scene_hud_scope_open_optional(stream,maps,map_count)`.
- In `rf_scene_draw_combat_hud`, after armed/vehicle/mounted state is known
  and before `campaign_draw_subtitle`, call
  `scene_hud_scope_draw(sink,context,armed&&!vehicle&&!mounted)` and propagate
  a nonzero status. The helper itself gates cinematic/dead/unscoped states.
  This keeps tint and masks behind all ordinary readable HUD layers.
- Before closing the base HUD owner on scene teardown, call
  `rf_hud_scope_assets_close(stream->hud_scope);stream->hud_scope=NULL;`.
- In `scene_hud_original`'s ordinary reticle condition, add
  `!(scene_scope.active && particle_draw_stream->hud_scope &&
  (campaign_equipped_slot==6 || campaign_equipped_slot==15))`.
- Add `src/core/hud_scope.c` to the parent-owned build source lists.

Telemetry `rf_hud_scope_diagnostic[8]` records attempts, signed load status,
loaded flag, resident/peak bytes, drawn frames, draw status and selected kind.
The no-input neutral benchmark can establish resource admission but cannot
establish scope drawing: a bounded existing sniper/scoped-AR check must toggle
the actual alternate-input path for that coverage. No fixture is created here.
