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
Both setup events must be sequenced to verify hangar-door traversal. The replay
fixture dispatches Shot1 at frame0, Shot3 at60 and Goto4994 at360. This is an
explicit setup fixture; automatic cutscene dispatch remains unimplemented.

`python tools/replay_npc_doors.py` runs three rendered1000-frame PC controls.
Hidden Gryphon and a disabled door both stop before the hangar planeX17.5.
With both prerequisites, Gryphon passesX20 with999 movement steps, no blocked
steps, two NPC trigger contacts, two controller starts and370 occupancy hits.
A longer1500-frame check reachesX27.471 and stops at a later obstruction; full
route completion, grounded movement and walking animation remain open.

The native comparison uses the same fixture on stock64MiB XEMU:

```text
python tools/xemu_replay_check.py artifacts/npc-doors/inputs-1000.bin --level L7S2.rfl --archive levels2.vpp --goto-uid 4994 --setup-uid 4953 4961 --capture --seconds 900
```

One setup UID retains the original frame0/300 schedule. Two use0/60/360;
the PC comma-separated environment value and Xbox4/8-byte setup file encode
the same sequence. These commands generate local assets and logs only.

Stock64MiB XEMU1000-frame replay passes in
`artifacts/xemu/replay-20260914-072409/report.json`: movement, route state,
trigger contacts and occupancy match PC exactly. Both builds and36 CTests
pass. Native capture was inspected but retains the starting-tunnel camera;
it does not visually demonstrate Gryphon walking through the doors.

Contained tests exercise actual authored UnHide4962 dispatch, deferred on/off
timing, missing backend and unresolved links. Scene tests check visibility
publication, door occupancy, no resurrection and separate controller backlinks.
