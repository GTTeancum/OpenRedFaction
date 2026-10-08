# Selected vehicle visibility

The selected vehicle owner now initializes object flag `0x4000` from authored
creation flag 2 and accepts the existing deferred `UnHide` event callback.
The concrete authored input is L1S3 APC UID 9627, initially hidden, targeted by
UnHide UID 9628. Previously the selected constructor discarded that flag and
the visibility callback recognized only passive vehicles, NPCs and clutter.

Live visibility gates chassis and attached gun/bit mesh submission, player and
projectile hull queries, flame contact, the active hull in NPC push clearance,
ordinary boarding, target admission and engine audio. Hidden selected hosts do
not compete with a visible candidate during ordinary vehicle switching.
Visibility changes retain the full registered owner, health, armor, team and
unrelated flags; dead owners are not revived. Rigid simulation and routes keep
their existing scheduling.

Occupied Hide remains unsupported and leaves ownership unchanged. The retained
original-code evidence in `tools/verify_unhide_deferred.py` establishes the
deferred scheduler at `4bcdf0`, but intercepts downstream hide/unhide routines;
it does not establish an occupied ejection policy. Generic Switch object
routing is still separate from the ordinary UnHide ON/OFF path.

## Persistence

RFPV2 adds `0x4000` to the existing vehicle-physics row mask, retaining the
16-byte header and 72-byte rows. It is written when the selected owner is
currently hidden or was authored hidden, including a revealed originally
hidden owner. Other saves retain their prior RFPV1 or unwrapped representation.
Passive visibility remains owned by RFVA2 and must agree with a present RFPV2
row. Selected hidden state is assigned only at the existing admitted world
publication boundary.

RFPV1 and saves without this wrapper restore selected hosts visible, matching
their historical behavior. Hidden player/NPC occupancy is rejected before
publication. Hidden hosts have no collision volume to admit, while identity
and rigid-state validation remain required; visible hosts retain normal world,
player and NPC clearance. Legacy RFCP capture rejects visibility-bearing
selected owners because that separate format cannot retain the state.

## Validation

Stock64MiB cloud Xbox PASS:
`artifacts/xemu/active-vehicle-visibility-20261007-214029/report.json`.
The isolated fixture retains every original APC9627 non-transform byte and
the byte-exact original UnHide9628 event, using CTF06 geometry and neutral
process-contained replay except for ordinary Use. It does not traverse a
campaign route.

The first real fixture run also exposed a settled APC boarding defect: the
authored seat body origin was below standing floor height, so a standing-body
transit rejected entry. The older moving-exit fixture boarded before the APC
finished settling. APC entry, exit transit and saved seated placement now use
the same actual highest player sphere as the existing Jeep seated-head policy.
Full standing exit floor, chassis exclusion and final body clearance remain
required. This is a practical shared seating policy, not exact original APC
collision parity. The acceptance fixture keeps the original staging transform
and lets the APC settle; it does not freeze or elevate the host to avoid this
defect. A real compiled-world floor blocker must still reject the head sweep.

- Visible case: 300 source frames, hidden Use30 rejected, original UnHide120
  reveals the same owner, settled boarding180 succeeds, then an occupied
  ordinary save. Fresh load runs120 frames, restores the exact saved pose and
  visibility without setup callbacks, exits30 and reboards80 normally.
- Hidden case: 300 source frames, the same reveal120, ordinary Invert sends
  UnHide OFF210, Use240 is rejected, then a hidden unoccupied save. Fresh load
  runs120 frames and rejects Use30/80 while staying hidden.
- Actual hull queries bind the vehicle contact tag and full handle; hidden
  owners contribute neither player/projectile hull hits nor chassis mesh
  submissions. Visible unoccupied samples submit geometry and answer both
  queries. The occupied player retains its own-host collision exclusion.
- Both sources execute14 native codec checks, including exact RFPV2 rows,
  RFPV1 decoding, malformed v1 hidden bits, wrong UID and hidden occupancy
  rejection. Local scratch stages never publish and all close. This verifies
  decoder compatibility, not a separate legacy full gameplay reload.
- The actual highest player sphere, radius0.15, hits compiled floor face171
  during a downward query, while the real player-to-seat head path clears.
  Full player/chassis state, flags and ownership remain unchanged by these
  read-only checks. Full standing exit admission runs during the real exit.

Both18,316-byte saves retain health5000, armor0 and ammo999/15. Full host/player
handles33686017/33620480 remain stable within each run; identity is checked
against both registries. Endpoint free pages are2900 for each source and2740
for each load. All39 original staged disc inputs were restored byte-for-byte.
The four actual prelaunch XBE/PE/map copies are retained with individual hashes;
all tested PEs are `b0f599622fb244046246a638a452d5acf91c7db3003cf6539110700287abbf07`.
Packaging gives the XBE copies distinct hashes, recorded in the report.

Two earlier FAIL runs remain preserved: `212955` omitted the scene-preview
startup flag and never loaded this scene; `213043` exposed the unused NPC-slot
observation guard, incorrect fixture delay assumption, and genuine settled APC
boarding rejection. No failure was reclassified as a pass. The older APC moving
exit harness now activates original UnHide9628 and supplies its startup flag;
preparation confirms the new visibility fixture and all four replays remain
byte-identical, but that older moving replay was not rerun in this batch. The
existing APC/Jeep saved-seat test expectation now uses the caller-owned head
sphere; no PC runtime test was performed.

Run serially with the private inputs and Linux Xbox setup:

```sh
bash tools/build-xbox.sh
python3 tools/xemu_active_vehicle_visibility.py
```

Occupied Hide, generic Switch routing, other vehicle classes, moving/crowded
boarding, NPC passengers, death/resurrection, audio appearance and exact original
seating parity remain outside this bounded result. Overall implementation stays
approximately88%, vehicles approximately96%; these are rough first-playable
estimates, not measured coverage or a claim that multi-owner AI is small work.
