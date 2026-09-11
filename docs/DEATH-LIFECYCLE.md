# Death lifecycle reconstruction

The live campaign does not yet have a complete death-start, dying-update,
game-over or respawn owner. Negative health and the death-audio bit are not
substitutes for those transitions.

## Death-entry state prefix

`rf_entity_death_entry_sp` reconstructs the SP state prefix of original
41fdc0 through41fe59. It returns without mutation when entity810 bit1 is
already set. Otherwise it clears the three words at714, conditionally clears
144 when the resolved42a020 low byte is zero, clears150, sets810 bit1 and
clears1a8 bit8000. Other flag bits and falling144 bit patterns are preserved.
The vector names retain original offsets because their full ownership and
meaning have not yet been established here.

`python tools/verify_death_entry.py` passes4096 cases:2074 already dying and
2022 entering. The original executes from41fdc0 including its SEH setup and
actual4fad00 vector clearing, with SP network globals zero. Only42a020 is
supplied at a predicate boundary. The harness stops before48c9f0 collision
teardown or at the already-dying return; it compares exact owner bytes with
the PC probe and compiled NXDK helper. It also verifies unchanged original
actor bytes outside selected fields and the predicate call count. Random
vector words include arbitrary floating-point representations; supplied
predicate words cover zero low bytes with nonzero upper bytes.

The helper is intentionally not called by the live scene yet. Next work is
integration of the verified48c9f0 cleanup and remaining41fdc0 side effects, followed
by41ee40 dying-update and the player/camera handoff. This isolated instruction
comparison is not an XEMU gameplay or complete lifecycle claim.

Validation: PC Release and NXDK builds pass; all12 CTests pass. Original
RF.exe SHA256:b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

## Collision-pair retirement

`rf_collision_pairs_retire` reconstructs full48c9f0. The original active
list lives at73db28 and the available list at75db30. Each has a head pointer
and a32-bit count. Node offsets0/4/8 are next/first actor/second actor; the
remaining payload is untouched. The runtime supplies resolved actor identity,
not an authored UID or registry slot number.

Traversal saves next before removal. Matches on either endpoint decrement
the active count, repair the head or previous link, and prepend the node to
the available list while incrementing its count. Previous remains unchanged
after removal, preserving consecutive-removal behavior. Thus removed nodes
appear in reverse traversal order ahead of the existing available list.
48cc70 is a free-list push, not a heap free. No allocation occurs here.

`python tools/verify_collision_retire.py` passes4096 fixtures and43750
retirements. It executes the complete original48c9f0 with its unmodified
40a490/48ccf0/48cc90/48ccb0/48cc70 helpers, without substituted callees.
Fixtures include empty lists, arbitrary node order, either endpoint and both
endpoints matching, existing available lists, consecutive removals, and
32-bit counter wrap. PC and NXDK agree with the original on all resulting
links, identities and counters. Original and NXDK full node buffers also
match, including untouched payload. Both builds and all12 CTests pass.

The reconstructed API requires valid disjoint finite lists and exclusive
ownership. The64-byte fixture stride is test storage, not a recovered full
original pair size. Live pair creation, payload layout and ownership remain
to be connected; this component does not independently enable player death.
