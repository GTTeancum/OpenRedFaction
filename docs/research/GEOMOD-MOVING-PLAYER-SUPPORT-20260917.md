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
