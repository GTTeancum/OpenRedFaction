# Live wet debris placement

The ctf06 DEV two-rocket recording produces five accepted wet births. All five
PC fractions and XYZ placements match execution of original RF.exe routine
0x48fc10 and its real math helpers bit-for-bit. This closes the live placement
question for this fixture, not general liquid or destruction parity.

`tools/check_live_wet_debris.py` runs the default PC build against
`artifacts/authored-post-live/two-shot.bin`, logs raw float words at accepted
births, and executes the original instructions with those exact inputs. The
original executable is checked against SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Only the solid query at 0x4df1c0 is supplied as a miss, after the reconstructed
live world already reports a miss; query flags 5 and radius zero are asserted.
This does not execute the original full geometry query.

The historical formula is reverse interpolation, `start + (start-end)*fraction`,
using a rounded liquid height from raw room depth and bottom. Its surprising
placement above the water is confirmed by instructions, not corrected by eye.
Accepted input/output words are retained in `artifacts/wet-debris-live/report.json`.
Replay SHA256 is
`6a155c3c3899b4d9fd11f3a9ad858ebbec85b70ab85a8129762f7851a34cf168`.

## Stock 64 MiB Xbox evidence

Run `artifacts/xemu/render-20260916-233551` completed 550 frames and passed all
59 selected PC/Xbox state comparisons. Both builds report wet state
`[30,30,5,3,1063505677,3083683289,0,1]`: 30 solid misses/tests, five accepted
placements, room 3, last accepted fraction and point hash, success, liquid present.
Xbox proves these aggregate counters and last placement; the five individual
binary comparisons above use the PC trace.

The 3884-byte player/destruction checkpoint matches exactly, SHA256
`7e6900b99a39ce38dc0154b70e95b601b9bd8816fe837cf1198ccb4857d5fb94`.
The endpoint has 4160 free physical pages (16.25 MiB). All 34 staged disc entries
were restored and the owned emulator process exited.

Native invocation:

```text
python tools/xemu_render_check.py --dev-room --player-checkpoint --spawn --level ctf06.rfl --archive levelsm.vpp --input artifacts/authored-post-live/two-shot.bin --geomod-checkpoint-out --seconds 240
```

The inspected native endpoint shows the room, damaged post, scattered debris,
rocket launcher and HUD. It does not establish every birth-frame appearance or
the full animation. Audio was disabled in this native run: audible splash and
debris sound remain open, along with broader liquid rooms and trajectories.
No screenshots were added to GitHub. This change adds diagnostic evidence and
regression comparisons; it does not change the existing placement algorithm.
