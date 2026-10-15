# Authored Xbox menus and optional PS2 menu overlay

Status: authored menu integration passed stock64 native Xbox compile/link/package
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

## Remaining presentation scope

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
