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

## Death-action selection

`rf_entity_death_select` reconstructs420c00 with a read-only420d00 clearance
callback and the explicit shared CRT random owner. It selects action16 when
flags810 bit400 is set or either138c/1390 equals13. Otherwise actions6/8/11
query direction1 and7/9/12 query direction0; a zero low-byte response forces
random fallback. Other predicate return bits do not affect the branch.

Action12 surviving clearance consumes one random draw: odd retains12, even
falls through to a second draw. An unset/rejected action consumes one draw
and selects5/14/15 using draw modulo3. A missing selected motion falls back
to5 if available, otherwise-1. Selection leaves the actor unchanged. The
caller still owns committing824, motion playback and subsequent death flags.
Inputs are limited to action-1 or0..44 and callbacks must not mutate the
actor or random stream. Arbitrary original out-of-range memory reads are
not supported by this API.

`python tools/verify_death_selection.py` passes8192 cases, comparing complete
original420c00 with real40a130/42a650 predicates and57312d CRT draws. Only
420d00 clearance and577eef thread-data lookup are supplied. PC/NXDK match
selection, random-state advancement and clearance callback order; owner
bytes remain unchanged. Cases consume zero/one/two draws5592/2296/304 times.
Both builds and all12 CTests pass. Geometry clearance and playback remain
unimplemented in this chain; no XEMU death animation is claimed.

The outer SP41fdc0 path skips this selector for an ordinary player detected
by4895d0 with globals6fc4d8/64ecb9 zero, storing824=-1. Flags810 bit80 also
bypasses its animation branch. Otherwise a supplied83c action precedes the
selector. Further outer work includes model overlay cleanup, motion mapping,
428c90 playback and the base class294->724 bit200000 behavior, which sets
entity810 bit02000000 instead of the usual post-play bit8. These details
are traced dependencies, not yet a reconstructed complete death owner.
