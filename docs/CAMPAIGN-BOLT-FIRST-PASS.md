# Campaign bolt emitters: data and state first pass

The installed v180 levels store bolt emitters in section `0xE00` and their
endpoints as Target records in section `0xF00`. L1S2 has 4 emitters, L7S1 has
15, and L17S1 has 16. Eleven authored `Bolt_State` events across these levels
refer to those emitter UIDs. Bounded readers decode both record types, and the
scene joins each emitter's endpoint UID to its authored position. It loads the
level's `bolt.tga` and `fatbolt.tga` through the existing map archives and
honors each emitter's enabled byte and scripted ON/OFF updates.

Original PC binary SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
The level section switch at `0x460820` sends `0xE00` to `0x460160` and `0xF00`
to `0x4604A0`. Their read sequences match the bounded parsers, including both
variable strings, positions/bases, endpoint UID, color, bitmap and enabled
byte. The event
factory at `0x4B69D0` uses its generic constructor for type 43. Generic ON
dispatch `0x4B9070` branches to `0x4B9490`, which visits linked bolt UIDs and
calls `0x48D720` to refresh the endpoint followed by `0x48D750` to enable the
bolt. Generic OFF dispatch `0x4B9F80` branches to `0x4BA220`, which calls
`0x48D770` to disable it. This supports the current UID-based ON/OFF callback.
The remaining numeric fields are retained without guessed gameplay labels.
The arc renderer is a first-pass reconstruction: four camera-facing textured
spans with deterministic per-frame lateral variation. Width and variation use
authored floats, but the exact original tessellation and field meanings remain
unverified.

The installed L1S2, L7S1 and L17S1 group membership lists contain none of the
35 emitter UIDs or their target UIDs. The authored endpoints can therefore be
treated as static for these three levels; a moving-endpoint adapter is not
needed to play their existing arcs. The original active tick at `0x48D300`
updates four segment slots and resolves optional endpoint owners. Its active
submission path at `0x48D7E0` calls `0x4D3560`, which queues a render callback
at `0x48D7B0`. Neither path contains an evident contact or damage dispatch.
This is scoped evidence, not proof that bolt damage is absent everywhere in
the original game.

Focused PC checks decode all emitters and targets in the three affected levels,
resolve every endpoint, and fire L1S2 event UID 8636, confirming its four
emitter UIDs arrive in authored order. Both PC and NXDK builds pass. A 90-frame
L1S2 XEMU run from authored item UID 9802 projected four bolt spans and 16
vertices on both PC and Xbox, with the same vertex hash `4192361634`. Xbox
retained 5,482 free physical pages (21.41 MiB). The `--no-images` run produced
no images. These values verify the draw submissions, not their visual appearance.
Contact/damage behavior, audio, save/load state, the original arc shape and
visual parity remain unimplemented or unverified. Revisit moving endpoint
attachment if additional level data or dynamic scripts require it. Contact
behavior needs a proven original call path before treating arcs as hazards.
