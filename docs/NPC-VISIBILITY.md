# NPC visibility in mission events

Type50 (`UnHide`) now queues on/off requests in the shared runtime and services
them with `rf_unhide_tick`. The existing reconstruction of original `4bcdf0`
provides on-before-off ordering and a shared500ms cooldown. Authored base delay
is retained before queuing; exact original activation-prefix parity is open.
See `tools/verify_unhide_deferred.py` for the existing original-code evidence.

The first-pass scene callback changes object flag0x4000 on registered living
NPCs and publishes it to the entity view and room owner. Dead/retired NPCs are
not resurrected. This flag originates in creation flag2 and is already consumed
by rendering, occupancy and combat filters. Proximity/appearance eligibility,
non-NPC objects and the full original visibility lifecycle remain incomplete.
Missing callback preserves pending work; unresolved links are consumed without
mutating another object, and callback errors propagate.

L7S2 Gryphon4952 starts with flags0x0202c000 and health75. The earlier Goto-only
fixture moved him while hidden; its route telemetry is simulation evidence,
not proof of visible campaign play. Authored Shot1 Delay4953 links UnHide4962
to Gryphon. Shot3 Delay4961 enables NPC-only trigger4963 and issues later Gotos.
Both setup events must be sequenced to verify hangar-door traversal; automatic
cutscene dispatch and the complete visible route are still unverified.

Contained tests exercise actual authored UnHide4962 dispatch, deferred on/off
timing, missing backend and unresolved links. Scene tests check visibility
publication, door occupancy, no resurrection and separate controller backlinks.
