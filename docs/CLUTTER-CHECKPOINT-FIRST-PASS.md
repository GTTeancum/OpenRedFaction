# Prop checkpoint component

RFPC1 encodes up to1024 UID-sorted24-byte prop records under a64-byte header, with explicit little-endian fields, authored identity32 and a checksum. It retains class identity, health, retired/hidden/hit flags, killing type and shared-class cooldown remaining. Positive-health GeoMod retirement is valid; nonpositive health without retirement is rejected. No process pointers or generation handles are serialized.

Encoding validates all input before writing. Decoding validates the complete stream and every row before publishing to caller-owned storage, including identity mismatch, truncation/corruption, UID order, cooldown consistency and capacity. It allocates nothing and uses one record of scan scratch; callers provide disjoint storage. The maximum wire component is24,640 bytes. The authored CTF06 population would require12,208 bytes.

Focused tests pass live/dead/GeoMod-retired roundtrip, identity and checksum failures, atomic output preservation, truncated input, capacity, duplicate UID, mismatched shared cooldown, missing retirement and nonfinite health. PC and NXDK compile the module.

This component is not yet an operational save feature. Scene capture still must reject pending break/effect work and mutable poses, generate an identity covering level/class definitions, and map UID/class identities to current registrations on restore. Full decode and ownership validation must finish before any mutation. Remaining cooldowns need rebasing to the restored simulation clock, and destroyed objects must suppress model collision/glare without replaying break effects.

RFCP4 now appends a mandatory RFPC1 section after the existing vehicle section, preserving all earlier section offsets and the32-byte header. Its length is the remaining envelope bytes and must agree with the validated RFPC header. New readers accept versions1–3 as explicitly prop-absent; old readers reject4. The RFSG maximum remains110,524 bytes: prop records reduce the available terrain budget rather than growing every save buffer. Existing saves must retain explicit compatibility rules. Keep scene_authored_clutter_baseline_check unchanged until the full state is captured and restored; damaged props must continue to reject legacy saves rather than silently reset. Live PC/Xbox save/reload verification remains open.

## Composed framing verification

Focused RFCP4 tests pass prop roundtrip, exact borrowed section locations, wrong identity, nested checksum corruption, atomic capacity failure and legacy prop absence. Existing RFCP framing and vehicle/remote checkpoint tests pass after factoring the common preflight path. All prop records validate without an allocated output array during outer preflight. PC and NXDK builds pass. Full RFDS semantic validation and UID/class scene remapping remain caller responsibilities; this framing change does not publish live state.
