# Vehicle homing targets

## Implemented behavior

Fighter rockets and submarine torpedoes now scan eligible passive vehicle owners
alongside NPCs. The shared vehicle admission requires a living, hostile,
non-destroyed owner, a current generation-bearing registry handle, no runtime
hidden/dormant flags, and exactly one visible authored seed. Missing or duplicate
seed identities fail closed. Initially hidden passive owners remain excluded
until a genuine passive visibility-activation path is implemented.

Line of sight accepts the target vehicle's hull only when the collision owner
tag and full target handle both match. Existing source/driver exclusions remain
in force. The existing guidance equations, wakeup, speed, scan cone/range,
tie-breaking and turn limits are unchanged. Submarine targets still require a
wet aim point; Fighter targets retain their existing dry-target allowance.

## Cloud Xbox verification, 2026-10-07

Both bounded checks were built and run on the Linux cloud host with NXDK,
Clang 19 and official XEMU 0.8.136. QMP verified exactly 67,108,864 bytes of guest
RAM. No PC gameplay, campaign-route traversal, image capture or host input was
used. Both checks used ordinary in-guest Use/fire replay, kept the pilot alive
and aboard, and restored their disc flags and staged original archive afterward.

- Fighter: 300 frames; one rocket launch and contact; 29 locks and 29 steering
  updates on the same full handle as Jeep UID 7629. Live health samples fell
  from 400.0 at frame 4 to 21.56668 at frame 220. Endpoint free memory: 5414 pages.
- Submarine: 240 frames; one torpedo launch, contact and detonation; 20 locks
  and 20 steering updates on the same full handle as submarine UID 8163. Live
  health samples fell from 700.0 at frame 2 to 380.4 at frame 162. Endpoint free
  memory: 5789 pages.

The evidence checks identity and actual health loss, rather than relying on
ray-query or damage-call counters. A read-only layout descriptor keeps the
owner snapshots tied to the compiled Xbox struct ABI. Both pre-fire and
post-impact snapshots are taken while the scene is live, before scene cleanup
releases its passive-owner bank.

The Fighter fixture uses the unmodified CTF06 parking-lot floor in room 0,
faces 6080/6081, and original Fighter/Jeep records with staged transforms and a
hostile target affiliation. The 21 m shot gives the unchanged 25 m/s rocket time
to pass its 0.25 s homing wakeup and keeps its 15 m blast away from the pilot.
The submarine fixture repositions original submarine UID 8163 near the authored
L5S3 submarine without changing weapon definitions or water rules.

Earlier short-range Fighter fixtures and a collision-skipped sky-room fixture
failed; those failures are retained as diagnostics and are not acceptance
results. The final parking-lot fixture resolved the test setup problem without
altering gameplay guidance or relaxing assertions.

## Reproduce

With the private game inputs, three prepared templates and isolated XEMU
firmware/HDD available, follow [Linux host setup](LINUX-XBOX-HARNESS.md), then:

```sh
bash tools/build-xbox.sh
python3 tools/xemu_vehicle_homing_target.py fighter
python3 tools/xemu_vehicle_homing_target.py submarine
```

Run the cases serially. The generated fixture archives, disc/ISO payloads and
private inputs remain ignored. Compact reports are written under
`artifacts/xemu/vehicle-homing-*`.

These checks establish the visible parked-owner first pass. Moving/occupied
target combinations, passive visibility activation, broader cover scenarios,
save continuation and retail presentation are not established by this batch.
The overall working-alpha estimate remains approximately 88%, with vehicles
approximately 95%; this small integration slice does not automatically change
those estimates.
