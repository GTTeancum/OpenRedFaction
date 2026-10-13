# Authored campaign credits

## Implementation status

Source-written against0454c8db62c187011e32007109802e953e0f6f0f. No compilation,
runtime, gameplay test, original-code execution or new fixture was performed
for this slice. Parent owns the next integrated hourly stock64 compile/package
and brief startup/running smoke; feature behavior belongs to testers.

The previous terminal branch rendered only a CREDITS label. `call_credits`,
including successful final-warhead defusal, now loads and scrolls the actual
installed English credits with the existing HUD font, accompanied by the
existing streamed music decoder. Fresh Use (default Xbox X; PC E) returns to
the normal frontend. The prompt expands the existing `$USE$` binding token.
After the final line leaves the viewport, a two-second tail also returns there.

## Configured music correction (October 12, 2026)

Source-only correction against41004d4756b6ae398f11af3f60594b09aeeb4725.
No compilation, test, runtime, fixture or image work was performed for this
correction. The existing completion roll, controls and normal return remain.

- `433fb3..433fce` parses `$Main Menu Music Filename:` (literal at `596710`)
  into the existing original string at `63c110`.
- Completion at `431610..431639` sets byte `637084` to1, then routes
  `call_credits` (literal at `596210`) through `43e9b0`.
- Credits entry `43dec6..43dee7` checks that byte, obtains the configured
  string from `63c110` and passes it with gain1.0 to `505c90`.
- Installed `game.tbl`, offset772096, size2424, line43, selects `music1.wav`.
  Its SHA256 is
  `dc896f30cfc89d26c79c6e41dbb018aeafe7399669a437c9e0ade444a783495a`.
- `music.vpp/music1.wav`, offset93870080, size7517378, is stereo MS ADPCM,
  22050Hz,1024-byte blocks,4-bit samples,1012 frames/block. It uses the
  existing decoder. SHA256:
  `23be5b71f2054cf03f7b09b26b1e34a00f48bc75a64698f1d83646d7ba4847d8`.
- Installed Bluebeard metadata names this track as looping with start0.
  Original `505d30..505d4a` selects the looped/nonlooped playback branch.
  Credits now apply the same existing whole-track loop policy as scripted
  `Music_Start`; missing loop metadata retains ordinary nonlooping playback.

The correction reuses `entity_assets.c`'s bounded lexer/string reader and
case-insensitive matching. It requires one nonempty quoted setting under
64 bytes and rejects duplicate, malformed and control-containing filenames;
reader output is published only on complete parse success. One temporary
`game.tbl` allocation is capped at64KiB (installed size2424) and freed before
music start. The entry-only filename is a64-byte local. No persistent state,
save fields, decoder, asset bank or retained table resource is added.

The tables archive is closed on every entry path. Missing/invalid config,
allocation/archive/read errors, absent tracks and decoder refusals leave the
prior stream stopped, record the existing optional-music failure status and
allow the credit roll and frontend return to continue. The existing music
revision, native borrowers, frame polling and full-session retirement retain
their owners. The filename is copied by the existing stream's VPP lookup;
no stack string is retained. This corrects completion credits only; it does
not reconstruct a separate frontend credits/menu-music state machine.

## Source evidence and practical choices

Static inspection of the installed RF.exe (SHA256
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836):

- `43dc80` opens `credits.tbl`; `43dce1` selects the language, using section
  markers at `598338` / `59834c` / `598344` for English / French / German.
- `43dd34` compares the terminator `#End`; `43dd4f` obtains the viewport width
  and subtracts64 before wrapping. Blank authored lines advance the roll.
- `43df61` subtracts frame delta times40.0 from the vertical offset;
  the40.0 float is at `589404`. `43df91` wraps back to the bottom when the
  original roll has left the screen. The port retains the40-pixel-per-second
  rate, but uses a documented natural frontend exit instead of repeating.
- `43e290` includes user-driven exit and scroll branches. The new port uses
  its already available Use action, with release/repress gating, and does not
  add keys or claim the original keyboard/controller mapping.
- The existing `43e9b0` / `434190(23,0)` terminal routing evidence remains in
  `research/FUTURE-CAMPAIGN-ENDGAME-20260915.md`.

Installed input metadata, inspected read-only:

- `tables.vpp`: `credits.tbl`, offset174080, size4787 bytes. Its302 physical
  lines include one full English section and French/German placeholder
  sections. The longest physical line is124 bytes. English is intentionally
  selected; placeholder translations are not presented as localized credits.
- The first implementation selected the installed `credits.wav` by name.
  The configured-music correction above replaces that choice with the authored
  `game.tbl` setting. No whole track is loaded or new backend introduced.

The black background, persistent title/exit hint, font-height-plus-four row
pitch,64–420-pixel text viewport, edge fade, two-second completion tail and
10-second missing-text fallback are practical first-playable presentation.
They are not claimed as recovered retail layout or artwork. Completion music
selection now follows the original configured menu-music branch, using the
existing stream/loop metadata. Original background-art behavior remains open.

## Bounded text and rendering

`scene_credits.inc` reads only the named VPP entry through
`rf_vpp_open/find/read/close`, caps it at16KiB, allocates size+1 and closes the
archive immediately. It rejects embedded NUL/control bytes and an absent,
empty or unterminated English section. Tabs are normalized to spaces. It
stops at the first English terminator rather than leaking into language stubs.

The renderer retains at most512 row spans, each consisting of a16-bit offset
and length, inside the loaded text. Glyph-width wrapping is performed once
using the existing TEXT font and a576-pixel width. Long words split safely;
blank lines retain their vertical space. Each drawn line uses a128-byte local
buffer. A fontless session uses the existing procedural glyphs with12-pixel
advances and14-pixel height. Overflow refuses the partial roll and presents
an explicit unavailable notice with the ordinary exit control.

The persistent row state is about2.1KiB, with a maximum16385-byte optional
text allocation. No credits-sized mesh, image, archive or decoded-track
allocation is introduced. Existing font assets are borrowed synchronously
inside the normal HUD sink; no pointer to them is retained. The underlying
world mesh is not rewritten: the existing frame sink receives an empty local
mesh descriptor, and the ordinary renderer/HUD fences still run. Gameplay
particles, glare, flashes, blackouts and combat HUD are not drawn under it.

## Modal input, audio and lifetime

The credits-only early branches in `actor_follow_view` and `scene_frame`
prevent camera/weapon/vehicle actions, physics, events and world presentation
from advancing at the next input/view/frame boundary. A credits request made
inside an event tick may finish that invoking frame's existing work; this
slice does not unwind an in-progress event callback. Blocking only the physics
tail would be insufficient because
several paid actions normally advance during view preparation. Player input
is zeroed, life-recovery handling is bypassed, and pending section exits
cannot take the scene away while credits own it. Failure fades, named failure
recovery and the active defuse input owner retain their existing branches.

On entry, Use starts held until an actual released sample is observed. A fresh
press or natural completion calls `rf_scene_session_end()` and returns the
existing normal input-stop sentinel. That cancels pending handoff/save/load
requests and suppresses departure capture through existing session ownership.
The normal Xbox session loop detaches input/audio, closes the resident world,
and then admits frontend reset. Sticky cleanup failures, storage ownership,
renderer admission and dashboard guards are unchanged. No direct framebuffer,
reset, dashboard launch or storage close bypass is added.

Start and controller-loss handling remain with the existing platform pause
owner. Restart is unavailable during credits; a paused/chord Save or Load is
rejected on return to the roll with an explicit notice. End Session keeps its
usual cleanup path. No save schema or ordinary failure-screen load behavior
is changed.

Entry requests stops for existing game voices without freeing PCM, then
replaces the current track through `rf_music_start` and the existing music
revision. The credits frame continues native music polling; the shared-code
fallback mixes only that stream into the existing audio buffer. Text or
music failure is optional and does not invalidate campaign completion.
A failed native stop retains borrowers according to the existing backend;
normal scene retirement and audio-close admission remain authoritative.

The text owner is freed during `campaign_close_movers` retirement and during
a genuine idle full-session reset. A subsequent credits request starts from
a fresh owner. Debug logs identify load/layout status and explicit versus
natural return, without embedding the original credit text in tracked source.

## Deferred implementation

Localized credits require real installed translations and an engine language
selector. Original background artwork, accelerated/manual scrolling and exact
retail fades remain presentation refinements. Saving a terminal modal is
still outside the checkpoint contract. No functional verification is claimed.
