# Campaign bolt emitters: data and state first pass

The installed v180 levels store bolt emitters in section `0xE00`. L1S2 has 4,
L7S1 has 15, and L17S1 has 16. Eleven authored `Bolt_State` events across
these levels refer to those emitter UIDs; the endpoint is a separate UID in
each emitter record. `rf_level_bolts_begin/next` now reads the bounded records
without loading textures or allocating per record. The scene owns only the
current level's records, initially honoring the authored enabled byte.

Original PC binary SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
The level section switch at `0x460820` sends `0xE00` to `0x460160`; its read
sequence matches the bounded parser, including both variable strings, source
position/basis, endpoint UID, color, bitmap and enabled byte. The event
factory at `0x4B69D0` uses its generic constructor for type 43. Generic ON
dispatch `0x4B9070` branches to `0x4B9490`, which visits linked bolt UIDs and
calls `0x48D720` to refresh the endpoint followed by `0x48D750` to enable the
bolt. Generic OFF dispatch `0x4B9F80` branches to `0x4BA220`, which calls
`0x48D770` to disable it. This supports the current UID-based ON/OFF callback.
The remaining numeric fields are retained without guessed gameplay labels.

Focused PC checks decode all records in the three affected levels and fire
L1S2 event UID 8636, confirming its four emitter UIDs arrive in authored
order. Both PC and NXDK builds pass. A 90-frame L1S2 XEMU run passed on stock
64 MiB with 5,461 free physical pages (21.33 MiB), using `--no-images`.
This run proves load and state plumbing only. No image was captured, and
visible arc content, endpoint tracking, contact/damage behavior, audio and
save/load state are not implemented or verified. The next step is to resolve
endpoint object placement and submit an actual bolt draw on both renderers,
then establish whether bolts interact with players or objects from binary
behavior rather than assuming they are damaging hazards.
