# Authored checkpoint framing prerequisite

The shared RFCP envelope now accepts only defined profile/version pairs:
profile1 with RFDS1, and profile2 with RFDS2. Authored framing requires at least
416bytes, header size416, extension size128, and extension policy2. Unknown
profiles, crossed versions and malformed lengths reject before output writes.
The RFSG transport limit and RFPL player layout are unchanged.

This is structural admission only. RFDS2 identity, geometry, bindings, retained
noise maps, complete-room player placement and transactional publication still
require scene integration. Accepting an envelope does not claim its contents
are semantically safe or that authored saves work yet.

Validation:

- `composed_checkpoint_codec` passes version pairing, header/truncation,
  unsupported-profile, player validation, size bounds and output rollback cases.
- Existing cavity save/load still passes exact fresh-process restart plus nine
  malformed-candidate cases at `artifacts/rfcp-gameplay/20260916-115331-312401`.
- The updated shared code builds successfully through NXDK into the Xbox XBE.
  No new native save/restart claim is made from this compile check.
- The separate authored placement probe passes against all798 pending faces:
  a destroyed opening accepts the actual miner sphere union, and the original
  790-face room rejects the same inside-post location. All786 untouched faces
  and82 liquid faces remain preserved. This is a prerequisite, not live restore.
