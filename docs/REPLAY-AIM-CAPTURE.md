# Capturing reproducible combat aim

The PC headless replay has an opt-in test controller:
`RF_REPLAY_AIM=2020:6980:7230` selects actor UID2020 during global frames
6980 inclusive through7230 exclusive. It reads actor/body-eye positions and
the last gameplay camera, then supplies bounded pitch/yaw inputs in[-1,1].
It leaves movement, fire, reload and other buttons from the input file intact.
It does not move actors, change health, force hits or send host input.
Normal PC gameplay and Xbox have no tracking controller enabled.

This is a test controller with privileged actor telemetry, not gameplay aim
assist or a claim about human aiming. It aims between body origin and eye,
does not predict motion or test visibility, and stops overriding look when
the target is dead. Bounds and the half-open frame interval are validated.
Its camera is from the preceding gameplay update; it does not advance look or
physics while reading telemetry. Extra shared scene storage is12bytes.

Each override logs `AIM_INPUT frame UID pitch yaw` with round-trippable float
precision. `tools/capture_replay_aim.py <source.bin> <run.log> <output.bin>`
copies only these look words into a new RFI6 file. The output is an ordinary
input recording usable unchanged by PC or Xbox; it has no target UID commands
or tracking dependency. The converter rejects invalid, duplicate, absent and
out-of-range records and preserves every non-look byte.

## First L2S3 encounter

The7356-frame local `artifacts/body-area3-tracked` run combines the25health
L2S3 arrival with the ledge approach. Tracking2020 during6980..7230 captures
122look records. It scores four40damage hits at local380,410,440,470, kills
the guard and survives two10damage return shots with5health. Seven rounds
are fired and the ordinary reload leaves16loaded rounds. Endpoint is
(58.187622,-1.141637,77.463722).

The captured input is `artifacts/body-area3-baked/input.bin`. Replaying with
RF_REPLAY_AIM absent passes: all77final body words, NPC rows, combat journal,
health, ammo, position and transitions match the tracked PC run. The converter
also preserves every non-look byte and rejects invalid/duplicate/absent records.
PC builds successfully. Stock64MiB Xbox verification now passes7356frames
and all33 comparisons in `artifacts/xemu/render-20260915-034047`, including
7shots,4hits,1kill and the full player-body state. Endpoint free memory is
4402pages (17.1953125MiB). All18 staged disc entries restore and the owned
emulator exits. This covers the first guard, not the maintenance encounter.
Further L2S3 traversal needs adjustment to this endpoint and orientation.

`tools/replay_l2s3_body_entry.py` now rebuilds the exact tracked input from
the corrected Area2 recipe and historical entry tail, runs tracking, captures
the resulting look words and verifies a second run with tracking disabled.
Its builder matches the executed source bytes; its verifier matches the
existing successful tracked and tracking-free captures. The actual reserve
count is118 after nearby pickup collection, not just starting reserve minus
shots; this is asserted alongside16loaded rounds.

The8402-frame `body-maintenance-track` probe adjusts heading, omits the old
backstep and reaches guard2047, but one return shot kills the5health player.
Neither an earlier first shot (`body-area3-early`) nor a more distant first
firing position (`body-area3-upper`) improves the first fight's final health.
Those alternatives are not accepted route improvements. See
LIVE-DAMAGE-REACTIONS.md for the confirmed missing gameplay feedback wiring
selected as the next system task.
