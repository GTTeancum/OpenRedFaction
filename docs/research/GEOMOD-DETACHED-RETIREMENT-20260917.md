# Live extracted-solid direct-hit retirement

Rocket contact now routes a tagged detached-piece hit through the shared kind3
health helper before ordinary radial damage. The first rocket detaches the post;
the second strikes its settled fragment, reducing birth health about23.73 to
-376.269562 and marking object flags00200002. No radial damage enumeration,
particle relaunch or speculative body impulse is added. This is whole-piece
retirement, not recursive fragment subdivision or a new disintegration effect.

Each batch owns eight bytes of life state per piece plus one array pointer.
Dead slots retain geometry and bodies to preserve history identity. Drawing,
body scheduling, ray/sphere collision and checkpoint player-clearance queries
all skip them. Memory remains counted inside the existing2MiB owner budget and
is reclaimed on batch close/reset. The live PC fixture retains133004 bytes.
Count/get deliberately include retained dead slots; callers must check alive.

RFPB version2 uses328-byte records: previous320-byte identity/body payload plus
little-endian health and flags. Version1's320-byte records still load and restore
birth health. RFDS framing/profile is unchanged; its layout reader checks the
exact stride for the recorded version. Decode validates all records before
publication, rejecting nonfinite/above-birth health, unknown flags and mismatch
between nonpositive health and retirement. Bodyless old saves retain birth state.

Tests cover dead-state reconstruction into a fresh owner, legacy conversion,
collision rejection of retired geometry, malformed later records without partial
publication, health/retirement consistency and both layout versions. All121
CTest cases pass; NXDK builds. The older malformed player-overlap save still
rejects at its specific clearance gate.

PC reproduction:

```
python tools/check_detached_rocket.py
python tools/check_authored_restart.py --case retired-piece
```

The second command compares550-frame save plus200-frame continuation against
750 uninterrupted frames. All checkpoint/history/publication bytes agree;
2760-byte RFCP SHA25679c6947fe3e1429afc70d1be567f79a982f30850d3c42ac8bae5575bbcca48f9.
The5320-pixel post region is identical after reload. Live telemetry reports zero
drawn pieces/vertices and zero scheduled bodies. The final PC framebuffer was
inspected: the tilted piece is absent and the remaining post stub is intact.

Stock64MiB XEMU run render-20260917-072729 completes750 frames and passes74
checks, including matching PC checkpoint output. Available pages4003. Native
framebuffer inspected for the absent chunk and retained room, post, weapon and
HUD. Harness restored the disc and closed its owned emulator. Audible impact
quality, interactive traversal through the removed piece and broad multi-piece
encounters are not claimed by this run.

Remaining: additional weapon direct-hit routing, nonlethal live health saves,
reclamation/compaction if owner pressure warrants it, timed retirement, and
independently evidenced later-blast wake/subdivision behavior.

Native reload follow-up render-20260917-072956 completes200 continuation frames
and passes74 checks with4050 available pages. Framebuffer inspected: fragment
remains absent. Its5320 post-region pixels exactly match the uninterrupted
native run (SHA256062305bcc4d28d1d65e6d1304da75ef483f9852f6255765c7c1eb0b840911686).
PC/native2760-byte checkpoint outputs match, including retired health/flags.
Both native harnesses restored the disc and exited their own processes.
