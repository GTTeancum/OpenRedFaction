# Authored Xbox menus and optional PS2 menu overlay

Current status October15,15:08 UTC: all integrated menu cache/motion changes
compiled and passed ordinary startup10 simulation/10 presentation frames in
Actual15 on670bdf36. See HOURLY-20261015-1500.md. Prior failed batches remain
historical failures. No fresh visual capture or full functional coverage is claimed;
source-stage pending-build wording in earlier sections records integration history.

Prior accepted milestone: authored menu integration passed stock64 native Xbox compile/link/package
and ordinary startup on October 15 at 04:06 UTC, built commit
e6290c269003688857a51e468cd28847af15d96e. See HOURLY-20261015-0400.md.
Main and Options native captures were delivered from the preceding build.
No broader gameplay, settings-persistence or audio-output functional coverage
is claimed. The earlier Actual03 timeout remains preserved as a failed run.

## Scope

The cold frontend and Options pages use original menu images and proportional
RF fonts, with the stock PC category structure and explicit Xbox adaptations.
Game, Audio, Controls and button mapping reach existing settings owners and
retain explicit save behavior. Video is visibly fixed for the console because
no equivalent native video-setting consumer exists. Button mapping retains all
ten existing actions. Campaign work and NPC dialogue changes are paused.

Main layout follows original 442bb0/442ce0/442db0/443270/4435f0: virtual 640x480,
full main_title canvas at (125,58), 204x61 category buttons at (11,58+61*n),
12-pixel authored medium font, and additive selected artwork (4577a0/50bc60).
Options titlebar is at (125,0), title at (265,12), body at (125,58). The titlebar
Y global 63c070 is zero-initialized; the unrelated value 33 is not its Y offset.
Controls slider/checkbox and binding viewport follow 4503d0; Game uses the
453130 cycle geometry for the Xbox language setting. Source addresses and
private inspection artifacts are retained separately from distributable source.

## Asset overlay and provenance

User-selected reference: PS2 Main Menu by HeyItsDuke,
https://www.factionfiles.com/ff.php?action=file&id=5733 (2021-08-06).
The downloaded release contains image/audio replacements, not menu code.
The original VPP remains unchanged and outside tracked source. It is admitted
for private tester packaging only with RF_MENU_OVERLAY_VPP and SHA256
2905b36544373c156e2671f2edb936cce25d8071bb4196811ff40e8374e2e05a.
No asset redistribution license was included in the archive. Asset bytes must
not be committed or uploaded to the public source repository.

Exact member lookup prefers the optional overlay, then stock ui/audio/music
archives. Only missing archives/members permit the documented stock fallback;
a present malformed override is an error. Stock-only packaging remains usable.
The mod's 640x478 Mars image is unscaled with a black two-pixel remainder;
the stock 1920x480 panorama uses its initial 640x480 crop. Full transparent
title canvases are preserved. Sidebar widths come from the resolved assets.

## Ownership and bounded storage

Menu pixels have a separate transient owner, fenced before page mutation or
release. Cold gameplay admission rejects retained menu resources. Paused
Options may acquire and release its separate image owner without borrowing
world HUD textures. Checked archive-close failures remain terminal.

The fixed menu image ceiling is 3584 KiB, including pixel owners, metadata,
font inputs and bounded scratch. Calculated stock peak before the final popup widget was 3,560,362 bytes;
overlay peak was 3,300,266 bytes. The final popup adds only bounded rectangle
metadata within the same image and total ceilings. Source images stream into a single 1024x512
canvas and a 512x512 glyph/widget atlas; no source-sized panorama allocation.
These are payload/metadata/scratch source calculations, not a measured
whole-program or allocator-backing peak.

Cold-menu audio exclusively owns the existing backend after successful scene
retirement. The eight overlay effects total 638,544 bytes, each below 128 KiB,
within the existing 1280-KiB PCM ceiling. Music1 streams through the existing
1012-frame decoder and 32-KiB native queue; its 45-MB file is never preloaded.
PCM16 stereo 44.1/48-kHz support preserves the existing MS ADPCM branch.
Native retirement precedes PCM free and archive close; uncertain retirement
retains ownership and blocks gameplay/Dashboard transitions. Pause Options
never replaces the scene's audio backend.

## Historical presentation scope at the accepted 04:00 milestone

Settled menu layouts are implemented first. Stock panorama scrolling and panel
entry/sweep animation are deferred. Load uses the recovered 599x366 panel at (33,58), with the existing two
save-source choices in its authored list. Exit uses the original two-button
popup geometry, preserving safe Cancel as selection zero. Difficulty remains
an explicitly adapted functional layout; no original dedicated difficulty
page is claimed. Xbox-only save/status/controller
hints are explicit adaptations. The existing 32-label localization cache is
unchanged; additional category/title/audio labels currently use English fallback. The new main and Options screenshots confirm their rendered presentation.
Other menu pages retain source-only visual coverage.

## Post-capture refinement

The visible main Exit label is shortened to EXIT so it fits the mod button
face; its explicit Dashboard confirmation is unchanged. Difficulty now reuses
the same authored button widget for the four existing values and Back. This
is an Xbox adaptation with unchanged routing, not a claimed original PC
difficulty page. This refinement is source-reviewed and compiled in the accepted Actual04
hourly batch; the delivered screenshots precede it.

## Continued menu motion implementation (October 15, 11:24 UTC)

The accepted release above is a milestone, not the end of menu work. Following
user direction, stock panorama and independent entry tracks are source-integrated
and compiled in Actual11. That batch failed its startup check; no new capture
is claimed. Later additions and the Actual12 outcome are recorded below.

Original 442ce0 advances the background at 15 logical pixels per second and
resets to zero after source width minus 640. The 640-wide overlay stays still.
Original 443620/44f69a move title panels from x637 to125 at 2048 pixels/second;
44369c moves the category rail from 11 minus resolved button width to11 at
four button widths per second. Load body uses its separate x637 to33 track
(44008b), with title and body widgets attached to their own origins. Options
body changes do not restart the title or category tracks. Xbox-only controls
follow the same body origin, without changing input/action/save routing.

The existing canvas is recomposed only when its integer crop or panel positions
change, after the native GPU fence. Raw TGA/VBM reads seek to visible source
rows; RLE remains bounded sequential decoding with original orientation and
alpha semantics. No second image owner, atlas enlargement, label growth or
budget increase is introduced. New scalar state is included in sizeof-owner
budget accounting. The legacy prepare_page API remains a settled-layout path.

Historical first-slice boundary: this motion slice implemented entry and background scrolling. Original
out-then-in Options body replacement and outgoing main-menu sweeps remain
subsequent presentation work; no exact full transition parity is claimed.

### Follow-on integration compiled in Actual12

The first motion pin compiled in Actual11; its ordinary startup check failed
with a QMP response timeout (see HOURLY-20261015-1100.md). After that terminal
batch, all four menu input loops gained timed redraw requests. Settled
640-wide overlays stay idle; entry/departure tracks request presentation ticks,
and stock panorama requests a redraw on integer crop changes.

Menu ownership now retains its two read-only checked archive handles until
retirement and lazily caches nine fixed page-source entries. This removes
per-frame directory validation/name scans; small image-header reads remain.
The archive pointers refer to the stable menu owner. Checked-close failure
invalidates readiness, attempts both closes once, retains the pixel owner and
blocks gameplay admission. Failed unpublished opens retire handles before
freeing pixels. Retained stdio/scratch remains inside the existing32-KiB reserve;
no additional reserve is charged to the peak.

Outgoing Main/Options shell sweeps and Options body departures are integrated.
They start at the currently presented coordinates and use original linear
velocities. The old page's widgets travel with its panel before the existing
navigation executes once. The presentation drain services controller latches
and cold menu audio but dispatches no actions, settings or saves. The modal
Exit path and gameplay admission paths retain their existing behavior.
These follow-on changes compiled in Actual12; its startup failure is preserved
separately from compilation and is not a runtime pass.

### Popup underlay and localized menu banks, compiled in Actual12

The inactive Main rail is now drawn beneath the Exit popup using the normal
font/sprite path. Popup top400x119 and bottom400x183 are split into four
alpha-preserving crops with one-pixel gutters in the unused right strip of the
existing1024x512 canvas. Backing, bottom and top draw after the rail and before
popup text. The selected overlay contains neither popup replacement, so these
members resolve from stock ui.vpp. Unexpected dimensions are rejected before
writing. No additional image, glyph atlas space or interactive Main route is
introduced while the popup is active.

The unchanged32x64-byte localization cache now selects a page-family ID bank.
Options reuses seven unneeded Main/difficulty slots for original IDs114,
784-787 and179-180; Load reuses two mapping slots for933/987. Selected-language
or original English fallback parsing is unchanged. Profile equality prevents
per-frame reparsing, including after a failed optional load. Default IDs are
restored for Main/Difficulty/Exit and after menu retirement before gameplay.
Xbox-only prompts retain explicit English wording; no translation is invented.

### Immediate adapted navigation and accepted-action notice correction

After Actual12, Main-to-Difficulty commits its real destination immediately
without the added outgoing drain. Difficulty remains an explicit Xbox-specific
adaptation, not a claimed original PC page. Incoming presentation remains.
Options/Load retain their source-derived outgoing sweeps.

Once a frontend action is accepted, a same-page loading/exit notice reuses
its last successful ready artwork instead of advancing animation/reopening
assets. A changed page or prior preparation error still requires preparation.
The notice is drawn normally before the existing ownership handoff. This
correction changes neither input receipts nor observer limits. It compiled in
Actual13 and remains included in Actual14; it does not establish either timeout cause.

### Geometry-only Options and adapted Difficulty title motion

Actual13 compiled the immediate-navigation/loading-notice correction and reached
Difficulty, but timed out during a neutral redraw before New Game acceptance.
See HOURLY-20261015-1300.md for the preserved terminal failure.

The next source correction caches the512x64 titlebar as two alpha-preserving,
guttered pieces in the existing canvas right strip:382x64 at641,1 and130x64 at
641,67. Options and Difficulty have no body artwork; their title/rail/widget
motion now updates coordinates without clearing the canvas or decoding assets.
Original linear timing and text offsets remain. Stock panorama crop changes
still recompose background and cache under the existing renderer fence.

The cache is mutually exclusive with Exit popup storage. Other pages retain
the existing composition path so their title-before-body overlap and larger
panels are unchanged. No extra image allocation, atlas space, label slot or
resource limit is introduced. Differently sized valid titlebar overrides retain
the original composition fallback; geometry-only invalidation requires the
actual owner cache flag. This correction compiled in Actual14; the bounded
frontend milestones and overall startup failure are recorded below. No visual pass is claimed.

### Actual14 compiled result

The geometry-only title correction compiled in Actual14. Difficulty entry,
release readiness, accepted New Game notice and frontend retirement completed.
Overall startup remains a preserved failure: QMP response timeout after loading,
simulation1/presentation0. See HOURLY-20261015-1400.md. This result establishes
those bounded frontend milestones, not full menu functionality or visual parity.

### Wider panorama window, pending native compilation

Options/Difficulty now cache up to768x480 background pixels in the existing
canvas. The visible640x480 quad changes UV origin while contained in that window;
only a window miss/page change recomposes it. At15px/s, stock scrolling can reuse
128 additional source columns before refilling. Original strict reset and
unscaled640x478 overlay behavior remain. Main/Exit/body pages retain composition.

Three253/253/6 title crops occupy x770 at y1/67/133. Internal split gutters use
neighboring source texels; actual source edges clamp. Two scalar window fields
are included in sizeof-owner accounting; image, atlas and memory limits stay
unchanged. Read/format failure leaves readiness invalid. The title-dimension
fallback still uses ordinary composition. Source review is complete; no new
native build, visual capture or runtime pass is claimed for this extension.

### Full-source Main and Audio panel caching, pending native compilation

Main512x366 artwork and Audio512x182 body plus512x64 title now fit in fixed
pieces within the existing canvas right strip. Main's second piece is rotated
clockwise in storage and presented using independent per-corner UVs. Audio
retains background/title/backing/body order; its original translucent backing
moves as geometry. All source pixels and alpha are retained, with neighboring
source texels in internal gutters and clamping only at real source boundaries.

A single shared crop decoder feeds either ordinary composition or cache writes;
raw/RLE TGA and VBM orientation/validation remain shared. Valid other dimensions
use the prior composition path. Source/I/O errors leave readiness invalid.
Actual panel_cached ownership gates geometry-only motion. Stock panorama changes
refresh only the visible640x480 background, preserving those cache pieces.
Options/Difficulty retain their separate768-wide background window; Exit and
other body panels retain their prior layouts. No source-sized staging allocation,
new pixel owner, atlas enlargement, timer or resource cap change is introduced.

### Validated transparent-margin settings caches, pending native compilation

Game/Controls/Buttons use the same shared decoder and cache owner. Body source
starts at x74 and title at x104 through y38, retaining the original transparent
edge texel. Four guttered pieces fit within existing right-strip rows0..507;
the narrow body piece is rotated. Source offsets remain part of screen placement.

Admission decodes every source pixel and checks the union of retained interiors.
Any nonzero alpha outside that union makes this valid artwork ineligible for the
cache and retains ordinary composition. Gutter coverage does not count as retained
content. Decode/I/O/format failures propagate before fallback admission. No source
byte is edited and no opaque/partially transparent content is silently cropped.
Game draws title/body as four contiguous records; Controls/Buttons add the original
translucent backing before body. Other resource limits and independent tracks remain.
