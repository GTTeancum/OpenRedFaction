# Readable authored HUD messages

Source-written after the parent20:00 pass on2026-10-08. No helper builds,
tests, images or captures. Parent owns the21:00 consolidated Xbox check.

## Established base, not a new measurement

Parent reports tested11bfc4d passed240 original neutral L1S1 frames on stock
64MiB at42.761682 XEMU presented FPS. Atlas telemetry was
[1,0,1,2107168,2181880,240,0,0], establishing actual admission and240 art frames
with zero fallback/draw errors. This message implementation is newer and has
not been runtime-verified. Overall implementation remains approximately89%.

## Original font and layout

The already-admitted rfpc-medium.vf TEXT font now replaces the procedural5x7
font whenever the optional original HUD owner is present. It preserves native
12-pixel height, case, character advances and authored glyph pixels. Installed
kerning is zero. Blank glyphs still advance; unsupported codes keep the existing
resource getter's '?' fallback. No extra atlas, file read or heap allocation
is introduced during a frame. The procedural renderer remains on the
asset-admission-failure path.

hud.tbl #640x480 provides:
- Row37: centered non-persona message width320 and upperY340.
- Row36: cinematic subtitleY360; X is ignored/centered. The existing552-pixel
  usable subtitle width remains a port policy, not an inferred persona box.
- Row39: countdown anchor17,133.
- Existing message/background/countdown palette entries now tint these paths;
  countdown warning thresholds retain the previous behavior.

Messages wrap by the actual sum of glyph advances, with word boundaries,
explicit newlines, CRLF handling and bounded hard-wrap of unusually long
words. Per-call scratch is16x128 character bytes plus small widths metadata.
Visible rows are bounded by the viewport; true overflow gains an ellipsis and
telemetry rather than drawing offscreen or shrinking the entire paragraph.
One measured background panel replaces per-row solid bars.

Pickup and save/load notices use two short lanes above the authored story
anchor so concurrent notices cannot obscure an active mission instruction.
That stacking is explicit first-playable port policy. The owner does not carry
persona identity, so no portrait or persona is guessed from a voice filename.

Short death/recovery/defuse/endgame/scanner labels center by glyph width. Long
mission-failure descriptions now wrap inside568 pixels and the reserved
Y138..398 area, removing the old negative-X placement for54+ character lines.
This is first-playable readability, not a claim of full original menus/persona
UI or exact retail subtitle line breaking.

## Numeric glyph correction

Original439f58..43a10a and43a6f8 center SMALL/BIG numerals horizontally at the
authored regionY, without height-fitting their bitmap. SMALL has27 rows but
visible numeric rows1..18; BIG has49 rows but visible rows3..32. The previous
height-fit therefore shrank actual digits. Numeric drawing now keeps native
height andY for ordinary values, retaining width-fit only for unusually long
values that exceed their box. Bitmap pixels and atlas storage are unchanged.

## Preserved ownership and input

No message content, event dispatch, latest-message-wins rule, duration,
deadline, voice playback, pickup timer, save status, countdown arithmetic,
damage fade, cutscene gate or inventory state changed. Only presentation
reads those existing owners. Expiry is still handled by campaign_draw_subtitle
before entering the new drawing helpers. Overlay pass order is unchanged.

Scope is already reachable through the existing Xbox Left Trigger alt_fire
mapping (axis>3855). On-foot living owners of Sniper slot6 or Scoped Assault
Rifle slot15 toggle on a fresh press; release/press toggles again. D-pad
left/right cycles owned weapons and Right Trigger fires. Existing switching,
death and mount gates clear scope; held input across selection does not create
a fresh toggle. main.c selects the controller poller in live player-control
mode without player-replay.bin. This is source verification only; no new input
binding was necessary or added, and no controller test was run by this helper.

## Parent proof hooks

rf_hud_text_diagnostic[8] is exported/reset with each HUD admission:
0 successful-entry native TEXT calls;1 emitted nonempty TEXT glyphs;
2 story/subtitle frames;3 wrapped rows;4 truncated paragraphs;
5 pickup/save notice frames;6 countdown draws;7 last drawing status.
SMALL/BIG numeric-only draws do not inflate TEXT-call/glyph counters.
Inspect positive TEXT/story counts and zero errors in the ordinary L1S1 run;
use the existing scheduled coverage for longer messages and timers as useful.
Source review is not a runtime/visual pass. No additional per-slice fixtures
or tests were created.
