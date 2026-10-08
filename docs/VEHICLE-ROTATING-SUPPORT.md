# Riders on rotating passive chassis

## Integrated path

The player and NPC support paths now resolve an exact registered passive host
and reuse the existing mover interval transforms for off-center point carry.
The forward transform maps a bound rider from the old host frame to the new
frame. Accepted ground contacts and the NPC retention fallback instead use the
reverse transform to recover velocity at their already accepted end-frame point.

Player movement consumes that point velocity through its existing physics step;
there is no extra pretranslation. NPC carry replaces its center-only proposal
with the transformed point before its existing full-body clearance and position
publication. Accepted contacts replace, rather than add to, the cached support
velocity. Ordinary translating hosts retain their previous center-velocity
arithmetic and exact player-save admission behavior.

A per-physics-step freshness flag and the existing restore-invalidated pose
cache prevent replaying old intervals. Player jump/fall/stance loss clears the
passive support handle and moving-support body flag while retaining the last
additive point velocity. The old host cannot keep changing that inherited
velocity while the player is airborne. NPC loss retains its existing lifecycle.

The initial runtime slice did not change save formats or admission. NPC
continuation uses the existing support UID/velocity, passive pose and controller
state codecs, subject to complete-world placement/admission checks. Subsequent
[player support persistence](VEHICLE-PLAYER-SUPPORT-SAVE.md) adds RFEN7 for the
exact rotating-player point cache and verifies fresh continuation/jump behavior,
while preserving the old RFCP guard and older RFEN readers.

## Focused fixture

`tools/xemu_vehicle_rotating_support.py` preserves original L20S2 geometry and
the passive Fighter 4717 record; the NPC case also preserves original friendly,
unarmed Eos 4716 with health 1. The disposable controller fixture changes Hanger
Lift001 to a slow one-way +Y yaw around the Fighter's authored center. Normal
UnHide reveals the hull; the second process-local setup event starts rotation
at frame 60. A real living Fighter input prevents When_Dead from auto-firing
before that explicit start. These are explicit fixture edits, not a naturally
encountered campaign sequence.

Roof modes 8/9 seed a player/NPC support point derived from the actual sphere
geometry at frame 40, requiring a meaningful off-axis radius. Independent
assertions bind one interval's forward proposal, accepted point and live XZ,
then compare cumulative movement/radius against the initial seeded point.
The player case uses one ordinary replay jump at 125 and checks released
ownership, body flag and unchanged inherited velocity while rotation continues.
The optional NPC save case retains the existing formats rather than bypassing
admission. Actual prelaunch XBE/PE/map copies and hashes are retained.

## Stock 64 MiB cloud Xbox evidence

The corrected run `vehicle-rotating-support-20261007-125608/report.json`
passes all three phases on exactly 67,108,864 bytes of RAM:

- Player, 150 frames: the live rider follows the independently transformed yaw
  arc at a roughly 1.501 m radius. At frame 115 the live point differs from the
  independent cumulative XZ prediction by less than 0.0002 m. Forward target,
  accepted contact and live position agree for the same interval, and the
  accepted point velocity is retained bit-for-bit.
- The ordinary jump executes once at 125. Its last support velocity is
  approximately (0.10254, -0.00003, -0.37903) m/s. The support handle and
  moving-support bit clear, and the cache remains exactly unchanged while
  airborne through the endpoint despite continued host rotation. Health 100
  and the player's identity remain intact.
- NPC, 150 frames: original Eos 4716 remains supported by exact Fighter 4717
  through the same cumulative yaw/point-velocity checks, with health 1 and no
  scripted walking or combat. The ordinary 9,760-byte save retains its actual
  support UID, pose and velocity, plus the chassis basis and running controller.
- Fresh NPC load, 20 frames: existing RFNC10/RFVA2/RFMC1 restore successfully.
  With setup and roof-stimulus inputs removed, the rider continues from the
  saved pose along the resumed yaw arc, using current point velocity rather
  than replaying an old interval. Original health and support identity persist.
- Endpoint free pages are 6,037 for player source, 5,703 for NPC source and 5,511
  after NPC load. All requested endpoints and original disc restoration pass;
  exact tested prelaunch XBE/PE/map copies are retained for each phase.

The first player run `vehicle-rotating-support-20261007-124317` remains FAIL: its narrowed death
watcher had no living input and started the turn at startup, so it had stopped
before the intended mid-rotation jump. That fixture graph was corrected without
changing the gameplay implementation or relaxing timing/momentum assertions.

## Boundaries

The focused claim is neutral-input yaw carry and ordinary player release.
Natural landing, walking riders, pitch/roll terrain fit, crowded support,
retail scheduler parity remain separate. Rotating-player persistence is covered
by the linked save milestone. No visual,
audio, PC gameplay or campaign-route claim is made.
