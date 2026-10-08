# Original HUD resources

Source-written 2026-10-08 for the first playable HUD. No build, test, emulator,
image capture or image generation was performed by this helper. Parent owns
integration and the 20:00 stock-64-MiB validation batch.

`rf_hud_assets_open` owns one 512x1024 RGBA atlas with one-pixel edge gutters,
40 original sprite rectangles and glyphs from three original fonts. It reads
original VPP entries directly, closes no caller-owned archive, and retains no
archive or source-byte references after loading. Every sprite preserves its
authored dimensions, orientation, colors and alpha. No resizing is performed.
The narrow `rf_image_tga_into` entry point streams the existing supported
24/32-bit TGA types into a validated atlas rectangle without allocation.

The requested 2304-KiB loader budget includes the 2-MiB atlas, owner metadata,
temporary source/packing metadata, all three bounded font payloads and an
8-KiB decoder scratch allowance. Resident/peak accessors expose accounted bytes;
allocator overhead and platform page rounding remain outside those counters.
This is extra residency beyond world materials, not an increase to the world
texture budget. Failure releases the entire partial owner and leaves output
NULL so scene integration can retain its existing HUD fallback.

## Source evidence

- `ui.vpp`: eleven health images, eleven envirosuit images, five reticles,
  three ammunition bars, two signal images and generic bullet icon. The seven
  specialized bullet icons come from maps1 through maps4. `ammo.tbl` identifies
  the authored ammunition/icon relationships.
- Original RF 1.20 NA `0x437df0..0x437e8c` inserts `_0` before HUD image
  extensions; health/suit initial loading is `0x439b50` and ammo loading is
  `0x43a240`.
- `tables.vpp/hud.tbl` supplies all eight colors and rows0..47 under `#640x480`.
  This module exposes those values; scene composition decides how to use them.
- `smallfont.vf` and `bigfont.vf` are VF0 numerical HUD fonts with69 glyphs,
  starting at code32, and27/49-pixel authored line heights. Their letters are
  intentionally much smaller than their numerals. `rfpc-medium.vf` is the
  general-text VF1 font with221 glyphs and12-pixel line height.
- Original `0x51fb30` reads the font header,16-byte glyph metrics, pixel data
  and optional trailing1024-byte palette. `0x51f730` packs glyphs into an atlas.
  Original numerical HUD usage is confirmed by `0x439f58..0x43a10a` (smallfont)
  and `0x43a6f8` onward (bigfont).

## Deliberate first-pass limits

Only the installed zero-kerning VF0 coverage and VF1 palette format are accepted.
Glyph widths, advances, bounds and unsupported characters are checked; unknown
character codes use the font's question mark. VF0 coverage reproduces the
original neutral-gamma 4-bit alpha conversion, but there is no original global
gamma-table binding. VF1 retains the original per-channel4-bit quantization.
Font width measurement supports newlines; actual layout/wrapping is scene-owned.

No vehicle-health art, damage-direction strips, persona portraits, menu panels
or new scope mask are included in this bounded owner. Scene resource admission,
live-state selection, texture/solid ordering and frame lifetime remain with
their respective scene/renderer owners. This source change is unverified until
the parent batch, and it does not claim pixel-perfect retail HUD parity.
