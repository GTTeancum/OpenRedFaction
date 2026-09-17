# Exact star-cutter corners

## Problem and change

Strict closed-boundary validation correctly rejected the earlier authored-post
pieces. Independent intersections of rounded supporting planes reconstructed
one shared cutter vertex at slightly different positions. Expanding provenance
repair alone inserted tiny duplicate segments and caused invalid polygons.

The shared core now recognizes a unique exact vertex shared by three support
triangles of the same star cutter, including its kernel. It preserves the stored
vertex rather than solving its rounded planes independently. Two common points
remain ambiguous and do not identify a corner. Final corner normalization and
provenance repair now also run for outward extraction replay, with support IDs
validated against each cutter's actual face count. No proximity weld or relaxed
closure tolerance was introduced; UVs remain owned by each face.

## Executed evidence

- Installed ctf06 UID94 probe: six cuts, six follow-up cuts and six checkpoint
  reloads pass. Four initial cuts emit one closed piece each (17,17,18,13 faces).
  The two other cuts emit none; this does not establish that none should exist.
  Each reload reproduces terrain bytes and piece/body digests, counts and RNG.
- Core peak remains 1105564 PC bytes; accepted batch peak is 296472 bytes.
- New exact-corner controls exercise all six support permutations for both an
  outer vertex and the kernel, plus an ambiguous shared-edge rejection.
- PC two-shot and reset-zero process-local continuation reports both pass.
  Continued and uninterrupted RFCP/RGCH/publication bytes match.
- The recorded live two-shot sequence still reports zero accepted pieces.
  Its inspected control frame renders the room, remaining post, rocket/HUD and
  smoke. It does not demonstrate detached chunks or motion.
- Stock NXDK build passes. No XEMU runtime was launched for this change.

Artifacts under artifacts/geomod-postedit-re: exact-corners-authored.txt,
exact-corners-tests.log, exact-corners-xbox.log, exact-corners-continuation/,
and exact-corners-reset/.

The full suite initially passed119/120; scene_authored_digest_capture failed
before editing because its fixture omitted the newly required material table.
The fixture now reads installed materials.tbl and participates in piece-registry
begin/commit/reset/cleanup, and the final full suite passes120/120 (exact-corners-tests-final.log). This also exercises
checkpoint reconstruction with the production extraction policy enabled.

## Remaining work

Repair recorded non-axis-aligned blast topology, verify accepted chunks in the
scene, add body motion/response and movement/weapon registration, preserve active
poses in saves, and verify stock64MiB Xbox runtime cost and visuals. This is a
geometry milestone, not complete detached-body gameplay or GeoMod acceptance.
