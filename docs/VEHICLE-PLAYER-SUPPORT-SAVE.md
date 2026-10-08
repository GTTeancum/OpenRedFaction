# Player saves on rotating passive support

## Contract

An off-center rider carries point velocity, which can differ from the chassis
center velocity. The existing player save guard admits only the latter. The
ordinary world-save path now admits the actual accepted rotating support point
through an exact living, visible, attached passive host and current registry
identity. Its velocity must match reverse interval math at the final player
position, and its accepted support epoch must match the latest chassis update.
The legacy RFCP admission helper remains unchanged.

RFEN7 adds an optional 12-byte raw point-velocity tail after RFEN6's sight/RNG
fields. It is emitted only when that cache is needed. Ordinary output otherwise
remains RFEN6, and RFEN1 through RFEN6 remain readable with their previous
zero-cache passive-player restore behavior. No runtime handle is serialized.
Velocity components must be finite and within the existing NPC-support bound
of 10,000 units per second.

Restoration admits the saved UID only against a loaded, living, visible,
attached passive row and its exact current registry owner. It retains normal
world, body, host support and NPC clearance. The cache is copied into the local
player stage and published with the final world transaction; environment
assignment does not write the player cache. Restoration clears the transient
accepted epoch, so no old interval can authorize a new capture.

## Stock 64 MiB cloud Xbox evidence

`tools/xemu_player_rotating_support_save.py` passes all four bounded phases in
`artifacts/xemu/player-rotating-support-save-20261007-154636/report.json`.
The source retains original L20S2 geometry/Fighter 4717, the existing explicit
off-center roof seed at 40, and slow +Y yaw starting at 60. It remains grounded
through the ordinary save; no source jump occurs.

- Source, 150 frames: the 8,932-byte save contains unchanged RFPL1, RFVA2 and
  RFMC1 plus a 244-byte RFEN7 section. Authored UID 4717 and all three cache
  words match the actual accepted source point exactly:
  `[1041260544,3085959167,3200020479]`, representing approximately
  `(0.140991211,-0.000028610228,-0.368041962)` units/s. Player body velocity
  remains zero; the additive support cache is a distinct state.
- Grounded fresh load, 20 frames: observation immediately after frame-zero
  restore matches saved player position/body velocity, cache bits and chassis
  pose exactly. The same player/host handles remain grounded, current point
  velocity refreshes normally, and the player travels about 0.11784m in XZ
  along the resumed yaw. No setup or roof seed is replayed.
- Immediate-jump fresh load, 40 frames: ordinary input jumps at frame 1 before
  grounded refresh can replace the restored cache. One release clears the host
  handle and moving-support bit while retaining all saved cache bits. First
  XZ displacement is `(0.002349853516,-0.006134033203)` m, matching cached
  velocity divided by 60; upward body velocity is 4.94235. The additive cache
  stays unchanged through frame 38 despite continued host rotation.
- Genuine RFEN6 compatibility load, 20 frames: the unchanged 9,760-byte NPC
  save from accepted run `vehicle-rotating-support-20261007-125608` loads and
  continues on the same regenerated fixture. Payload SHA256 is
  `aadd7ab693ff2dfc35366218e232cfb1641371a14d3278117f06dca35f92755e`;
  the fixture archive matches its original hash exactly. Only the existing
  RFSG optical transport wraps the old payload; no save migration is applied.

All phases run with 67,108,864 bytes of RAM. Endpoint free pages are
6,024 / 5,832 / 5,832 / 5,497 in that order. The original 38 disc inputs are
restored and hashed. Exact prelaunch XBE/PE/map copies are preserved privately
for each phase; their common tested PE SHA256 is
`1a884cfea8a5a3f572d599805729b4867e607fd30e5ce952094f326449fb1adf`.

The source also passes 16 read-only native checks: exact point admission,
different full-handle generation, nonfinite/out-of-range cache, unchanged
legacy RFCP rejection, RFEN7 encode/roundtrip, checksum-refreshed malformed
RFEN7 payloads, and eligible versus wrong-UID/detached/hidden/dead staged hosts.
Rejected decodes leave output stages null; temporary stages close normally.
The differing-handle argument is rejected before lookup. Epoch freshness and
complete restore publication ordering are source-reviewed; the audit does not
simulate registry reuse or fault-inject a stale epoch.

The final NXDK build, Python parsing and stack audit pass. The compiler retains
a 128KiB reserve and 45,212-byte listed scene subtotal; this is not a measured
runtime stack high-water mark. The initial fixture-only empty-loop warning was
corrected before the first runtime batch, with the original build log retained.

## Boundaries

The runtime claim is an explicitly seeded, idle, off-center rider on one passive
Fighter under gentle yaw. Natural landing, walking riders, pitch/roll fit, crush,
active-vehicle support and legacy RFCP support expansion remain outside this
change. RFEN1–5 retain their existing decoder branches but are not separate
runtime cases in this batch. Older RFEN6 center-only passive-player cache
semantics remain unchanged. No images, host input automation, PC gameplay or
campaign route is used.
