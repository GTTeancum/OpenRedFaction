# Runtime integration checkpoint

This source checkpoint collects the shared PC/Xbox implementation previously
validated in the working tree. It is not a full playable campaign milestone.

- Authored ctf06 post cuts use owned source geometry, visible-surface publication,
  complete-room collision composition, private lighting preparation, and
  clone-before-edit transactions. Safe reset/re-edit is implemented; authored
  destruction file saves remain unfinished.
- Swimming and authored liquid damage use the shared player state and installed
  rates. Underwater presentation remains open.
- Settled GlassHouse player/destruction saves use RFCP/RFPL and the two-slot HDD
  transport, with complete candidate placement checks and invalid-newer fallback.
- Grouped debris audio and a render-plane precision fix are retained. Audible
  quality and wet birth coverage remain open.
- Pure authored owner framing and clutter parsing/damage helpers have tests but
  are not yet connected to authored save restore or live prop combat.

Retained native evidence:

| Scenario | Artifact | Result |
| --- | --- | --- |
| Post reset then recut | xemu/render-20260916-114703 | 56 comparisons,840frames,16.5078125MiB free |
| Two post cuts | xemu/render-20260916-112913 | 56 comparisons,550frames |
| Player save restart | geomod-hdd/20260916-105023-985054 | exact fresh-process RFCP restart |
| Invalid-newer player save | geomod-hdd/fallback-20260916-105318-072255 | older slot recovery |
| Lava exposure / dry control | xemu/render-20260916-101651 and103712 | 44/45 comparisons |
| Dry debris sound dispatch | xemu/render-20260916-105615 | 55 comparisons; audible quality not verified |

Artifact paths are under local `artifacts/`; original assets and generated
outputs remain untracked. The native post captures were visually inspected.
These bounded runs do not establish all-level destruction or campaign parity.

Current installed source identity is
`4c74d0a864ac2f303620f5001c8f748a6a52b5e1bb7f65b510b2a7f40505c3b2`,
captured from two textures and19 source chart/reference records. Its standalone
probe verifies repeatability, rejected missing/duplicate ownership, and sensitivity
to changed texture pixels. The neutral executable name and explicit asInvoker
manifest avoid Windows installer detection; no elevation is needed.

The complete PC build and NXDK build succeed. All91 CTests pass after fixing
the stale synthetic NPC collision/pain fixture and re-establishing its explicit
attack order after an invalid-target scenario. Production combat behavior was
not weakened. Log: artifacts/authored-post-live/runtime-consolidation-tests.log.
