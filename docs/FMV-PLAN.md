# Pre-rendered movies (FMV) — plan

Status: planned (2026-11-09), playback timing, stream format and decoder choice settled; nothing implemented. In-level story scenes are
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
2. Done: the streams (read from the ISO, 2026-11-09). Each PSS is an
   MPEG-2 program stream with one video stream (`0xE0`, 640x448, 30 fps,
   about 8 Mbit/s for the intro: DVD-class) and audio in private stream 1
   (`0xBD`, substream `0xFF`, then three bytes before the payload). The
   audio starts with a Sony `SShd` header (24 bytes: format 1, 48000 Hz,
   2 channels, interleave 0x200, no loop) then `SSbd` and its byte count:
   uncompressed 16-bit little-endian PCM (checked: little-endian samples are
   smooth, big-endian are noise), channels in alternating 512-byte blocks.
   No audio decoder is needed; 48 kHz is the Xbox's native rate.
   Lengths from the audio sizes: THQ logo about 19 s, intro 109 s, ending
   81 s, Summoner trailer 61 s.
3. Installer: copy the four PSS files from the user's PS2 disc onto the Xbox
   disc unchanged (PS2 format kept; no re-encoding).
4. Xbox player. Decoder chosen (2026-11-09): a new intra-only MPEG-2
   decoder written from the standard (ISO/IEC 13818-2), no third-party code.
   Reason: every picture in all four movies is an I picture (THQ logo 568,
   intro 3,247, ending 2,424, trailer 1,829; the PS2 IPU decodes intra
   only), all frame pictures (structure 3), progressive, frame DCT only,
   intra DC precision 8 bits, linear quantiser scale, intra VLC table B.15,
   zig-zag scan, no concealment vectors. So no motion compensation, field
   handling or B/P picture code is needed: the decoder is start codes,
   sequence/extension/picture/slice headers, macroblock and block VLCs,
   inverse quantisation with mismatch control, an 8x8 IDCT and 4:2:0
   output. The project has no licence chosen yet; libmpeg2 (GPL-2) would
   decide it, and FFmpeg (LGPL) is large and awkward to relink inside an
   XBE. Cost: about 6,700 8x8 inverse transforms per frame (1,120
   macroblocks x 6 blocks), about 200,000 a second; the P3's SSE can help.
   Colour conversion can run on the GPU (Y, U, V as three textures). Frames
   are drawn full screen (4:3 inside 16:9 at 720p); audio through the
   existing stream path.
5. Hook the movies to the traced moments; skip with A/Start; memory stays
   within stock 64 MiB (stream from disc, two frame buffers).
