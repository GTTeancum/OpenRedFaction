# Pre-rendered movies (FMV) — plan

Status: planned (2026-11-09), playback timing traced; nothing implemented. In-level story scenes are
engine-driven on both platforms and are not part of this.

## Which movies

Only movies both releases have (user decision, November 9). The two
PS2-only trailers (`RFTRAIL.PSS`, `MXTRAIL.PSS`) are left out.

| Movie | PS2 disc (played from here) | PC name (RF.exe) | PS2 size |
|---|---|---|---|
| THQ logo | `THQLOGO.PSS` | `thqlogo.bik` | 23 MB |
| Intro | `INTRO.PSS` | `intro.bik` (`$Intro Movie Filename`, tables.vpp) | 133 MB |
| Ending | `ENDING.PSS` | `end.bik` | 99 MB |
| Summoner trailer | `SUMTRAIL.PSS` | `sumtrailer.bik` | 75 MB |

The PC install in `Installed_Game/` has no movie files (only `binkw32.dll`),
so the PS2 disc is the source. Every PSS checked is an MPEG-2 program stream,
640x448 at 30 frames per second (sequence header frame-rate code 5).

The PC release uses Bink (`.bik`, RF.exe names above), not PSS. Playing the
PS2 files directly is preferred over converting: XMV needs Microsoft's
XDK tools and has no NXDK player; Bink would need RAD's licensed tools (not
redistributable to players) for encoding, so both add a converter without
removing the need for an Xbox decoder.

## When the PS2 plays them (SLUS_200.73, traced 2026-11-09)

Player: `204a90(path, mode)` streams `data/movies/<name>`. After 60 frames
(`0x3c`) a movie can be skipped unless mode bit 2 is set: mode bit 1 clear,
a press of pad buttons 0xe, 3 or 0xc on either pad stops it; mode bit 1 set
(trailers), it stops when pad input 1 is no longer held/changes.

| Movie | When | Mode | Evidence |
|---|---|---|---|
| THQ logo | Start-up sequence `1b1000`: stage 0 waits at least 5 s (`0x1389` ms), stage 1 plays the logo, stage 2 onward draws the loading bar | 0 (skippable after 60 frames) | `1b1000` -> `204a90("data\movies\thqlogo.pss",0)` |
| Intro | New game: `1ecc80` sets the intro flag (`27d790`, `2acd74`) and enters state `0x1e`; `27d3e0` selects `intro.pss`, `27d4c0` plays it and continues | 4 (skippable) | `1ecc80`, `27d3e0`, `27d4c0` |
| Ending | Game-end event (state handler case `0xc`, single player only: `2ac530` clear, `2ac0d0` set): `27d7a0` sets the ending flag (`2acd78`), state `0x1e` plays `ending.pss` | 4 (skippable) | `1b2b20` case `0xc` |
| Summoner trailer | Main menu handler `1e3c20` (`27d760("sumtrail.pss")`, state `0x1e`), reached from the main-menu slot handler `1e3d40` region (`1e3f9c`, `1e3fac`): the Extras > Trailer entry | 5 (input-held stop) | `1e3c20`, `1e3d40` |

Not used: `RFTrail.pss` plays after 30 s idle on the main menu
(`1e3d40` neighbour, `2ac600` > 30.0) — PS2-only, left out. The same
main-menu handler `1e3d40` case 3 (TRAINING) loads `train01.rfl`, which the
port's TRAINING already starts.

## Steps

1. Done: when each movie plays (above).
2. Identify the PSS audio stream (PS2 ADPCM or PCM in private stream 1,
   channels, rate) and how it interleaves with the video.
3. Installer: copy the four PSS files from the user's PS2 disc onto the Xbox
   disc unchanged (PS2 format kept; no re-encoding).
4. Xbox player: MPEG-2 video decode on the CPU (733 MHz is enough for
   640x448 at 30 fps, as Xbox media players did DVD), frames uploaded to a
   texture and drawn full screen (4:3 inside 16:9 at 720p), audio through
   the existing stream path. The decoder must be licence-compatible with
   the project (candidates: libmpeg2, GPL-2; FFmpeg's MPEG-2 decoder, LGPL);
   record provenance and keep its notice.
5. Hook the movies to the traced moments; skip with A/Start; memory stays
   within stock 64 MiB (stream from disc, two frame buffers).
