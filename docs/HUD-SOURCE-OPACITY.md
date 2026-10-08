# Original HUD opacity and numeric colors

The user-supplied original-game reference and the existing20:23 Xbox capture
showed that the current health/suit overlay was excessively bright. Source
inspection establishes missing draw modulation independently of the renderer's
separate blend/combiner review. No new runtime, screenshot or image modification
was performed for this change; parent owns21:00 verification.

## Exact source policy

- Original `0x439f65..0x439f76` sets RGB255,255,255 with alpha120 before drawing
  the health image at `0x439fbb` and suit image at `0x43a009`. This is47.06%
  opacity on an otherwise opaque source texel. The source draw order is health,
  then suit. Current scene composition had full alpha255 and reversed order.
- Original `0x4375e0` constructs `0x6373b0` as white alpha70. Ammo backplates
  and signals use that alpha at full fade (`0x43a5a1..0x43a5bd`, `0x43a90c`,
  `0x43ae36`). Their steady opacity is27.45% before source texture alpha.
- Actual ammunition icons explicitly restore white alpha255 at full fade
  (`0x43a643..0x43a664`, `0x43a9e9..0x43aa0a`). They must not share a blanket
  dimming factor with their backplates.
- Parser `0x4378f5..0x437928` binds the full/mid HUD table colors to globals
  `0x6373b8` and `0x6373bc`. The installed table supplies full RGBA(0,255,0,105)
  and mid RGBA(255,255,0,150).
- Health numerals select full at `0x43a011`; armor selects mid at `0x43a094`.
  Loaded ammunition selects full at `0x43a688`; reserve selects mid at
  `0x43a75b`; clipless reserve selects mid at `0x43af12`. The old scene used
  opaque default green for every category.

## TGA/atlas findings

Both health100 and enviro100 are original32-bit TGA images with descriptor8,
which explicitly declares8 alpha bits. Their bright full-value source RGB is
intentional. Their alpha contains both0 and255 plus antialiased intermediate
coverage. `rf_image_tga_into` preserves that alpha and converts BGR to RGB; it
does not replace it with255 or bake a tint into the shared atlas. No decoder
or original asset modification is needed for the demonstrated scene-state bug.

The scene patch applies opacity per component, uses the original full/mid
numeric colors including alpha, and restores health-before-suit draw order.
The renderer must multiply sampled alpha by vertex alpha for this to work.
Ordinary text, reticle hit feedback, scope overlays and original asset pixels
are unchanged. Original selection/pulse and global HUD fade behavior are still
not reconstructed here; these constants describe the steady full-fade state.

## Reticle neutral modulation

Original `0x43a4c3..0x43a4d7` supplies white RGBA255 for the normal reticle;
the locked-art path `0x43a45e` does the same. Green is in the source image:
nontransparent `reticle_0.tga` pixels have RGB(0,244,0) and fractional alpha.
The rocket-lock resource instead contains red pixels. Neither path uses the
full/mid numeric text colors. The follow-up sets the caller's neutral tint
to white255 rather than gray238, leaving explicit surface/hit feedback
overrides untouched. No texture recoloring or alpha replacement is performed.
