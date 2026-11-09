# Pre-rendered movies (FMV) — plan

Status: planned (2026-11-09), nothing implemented. In-level story scenes are
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

## Steps

1. Trace when the PS2 plays each movie (SLUS_200.73 string references for
   `thqlogo.pss`, `intro.pss`, `ending.pss`, `sumtrail.pss`): start-up order,
   skipping with a button, the ending after the last level, and whether the
   Summoner trailer is the main menu's Extras > Trailer entry. Record addresses.
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
