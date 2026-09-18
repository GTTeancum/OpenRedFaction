# Retained moving-rubble support

Player ground contacts already returned fragment body velocity, but the scene
only retained a registered mover handle for the next tick's support refresh.
Fragments are owned by separate GeoMod registries, so their velocity could remain
stale after they changed motion. Ground contacts now also carry a runtime-only
source/batch/piece reference and the publication revision plus registry identity.
No fake object-registry handle is introduced.

The sidecar travels with the ordinary ground/stance query through support commit.
The next player tick resolves the live body and refreshes the existing shared
support velocity. A rising fragment uses the shared moving-support commit path,
which permits an upward support correction. Settled zero-velocity support avoids
an unnecessary wake. Retirement, collection-owner replacement and changed
publication revision reject the borrowed reference before body dereference;
scene initialization clears it, and counterfactual route probes save/restore it.
Missing ownership does not invent a new momentum policy: the existing original
rf_physics_support_refresh NULL-resolution behavior remains intact. A subsequent
ordinary ground query determines falling or another support.

This is a port ownership adapter around the recovered support/locomotion math
in physics.c, including support_refresh (41e370 context) and support_commit. It
adds65 small reference records (1040 bytes on32-bit Xbox), no heap allocation,
no serialized pointer, and no change to the132-byte ground diagnostic layout.
Moving-support saves remain excluded by the existing settled-placement rules.
Angular point-velocity carry and a live moving-fragment sequence are not qualified.

Validation:

- Full PC build and all123 CTests pass (moving-support-build.log / tests.log).
- Expanded scene tests exercise actual ground-query selection from collection
  slot1, upward commit, changing velocity, stopping, retired/replaced identity,
  and ordinary run displacement versus a zero-support control. Delta matches
  support velocity times0.125 on all three axes. These moving bodies are synthetic
  registry fixtures, not an authored gameplay extraction sequence.
- Intermediate-rubble standing, saved continuation, walk-away and retired-support
  rejection pass with the new scene code (moving-support-standing.log).
- Stock64MiB run artifacts/xemu/render-20260917-212040 loads that saved standing
  state and walks away over201 input records. All75 checks pass; the Xbox save
  exactly matches uninterrupted PC retreat-control. Endpoint3975 free pages gives
  15.527MiB, not a worst-case peak guarantee. The native frame was inspected and
  retains the angled chunk, damaged post, room, weapon and HUD. It is a standing/
  walk-away regression, not native moving-support acceptance. Audio was disabled.
  Disc restored and emulator exited; no GitHub image added.

Focused logs: moving-support-query-{build,tests}.log and
moving-support-carry-{build,tests}.log. Native log: moving-support-native.log.
Next: a bounded process-local moving-fragment gameplay case that exercises the
full ground-query/update loop while support starts, stops or is removed.

## Full scene lift, stop and retirement fixture

`tools/check_moving_rubble_support.py` loads the actual radius0.530725 extracted
chunk from `intermediate-rubble-standing/saved.rfcp` with neutral player input.
The opt-in `scene_moving_support_test.inc` lifts that body0.25 units over60 ticks,
stops it for30 ticks, then damages/retires it through the existing source adapter.
This is a kinematic test stimulus on real extracted geometry, not a natural
upward debris trajectory or proof of rotating-support behavior. No host input.
Normal sessions leave the fixture disabled; PC requires a headless environment
flag, Xbox a harness-owned disc flag that is saved/restored with the disc.

PC results in `artifacts/moving-rubble-support/report.json`:

- Stationary control remains at playerY0.464791.
- First moving contact corrects the support position, reachingY0.478665.
  Afterwards player displacement tracks actual fragment displacement within
  0.000002 units; the contact correction is not counted as accumulated carry.
- End of lift isY0.724499; stopping clears carry velocity and leaves position
  unchanged. Neutral playerX/Z are bit-identical at all ten samples.
- Retirement clears support immediately and switches the player to falling;
  by frame150 the player has landed on the floor atY-0.368479.
- All123 CTests pass. Lifted and landed PC frames were inspected: intact room,
  broken post, launcher and HUD, with the camera lower after falling. The support
  is underfoot, so these images alone do not prove its motion; sampled body/player
  state supplies that evidence. Audio quality was not tested.

The native harness now reads all160 telemetry words for exact PC/Xbox comparison,
in addition to its existing player, destruction and checkpoint checks.

Stock64MiB run `artifacts/xemu/render-20260917-213136` passes76 checks. All160
sampled words match PC exactly, as does the2654-byte final player/destruction
checkpoint after retirement and landing. Endpoint3975 free pages is15.527MiB;
this is not a peak-memory guarantee. The native final frame was inspected and
shows the lowered camera, damaged post, room, launcher and HUD. It agrees with
the PC endpoint; intermediate native frames were not captured. Disc restoration
completed and the owned emulator exited. No GitHub images were added.

Next: naturally moving/rotating support and larger polygon-contact geometry.
Do not count the kinematic lift as natural debris or angular-carry acceptance.
