# Death-triggered mission events

The shared mission tick now polls type16 (`When_Dead`) and dispatches its linked
mission events/triggers once per scene. A borrowed scene query supplies known NPC
presence and living state, including retained defeated actors on section revisit.
Unknown object classes defer the watcher instead of being treated as deaths.

Original RF.exe evidence (the repository's recorded SHA256): factory4b69d0 maps
16 through4b6c95 to constructor4be6c0 and vtable5899ec. Its update method4bb3a0
calls base update4b8ce0, skips while timer298 is active or byte2b8 is already1,
and iterates links29c. Lookup40a0e0 supplies object presence;48b450 tests flag7c
bit0x400000. If none are living, fire. Otherwise byte2b9 permits firing when any
linked object is missing. Loader4624fb..462505 passes authored flags[0] to4b7fe0,
which clears one-shot byte2b8 and stores the low byte at2b9. Thus the optional
condition is specifically any *missing* object, not merely any zero-health actor.

At4bb43c the fired byte is set before outgoing effects. Event targets activate
through4b8b70 with source/actor=-1 and mode1. Movers and an auxiliary family also
have separate original effects at4bb474..4bb4bb; these remain unsupported in the
port's existing target dispatcher. The watcher's own authored delay is not applied
to automatic detection; downstream events retain their own delay scheduling.

First-pass differences: the scene uses registered NPC health>0 as living state,
rather than reproducing the original object's live-bit lifetime. Corpse cleanup
can therefore affect the exact firing frame. Known retired NPCs are missing;
unsupported classes, clutter and other unknown UID families defer evaluation.
This is an explicit gameplay implementation, not a claim of exact timing parity.
An unattached query leaves polling disabled for standalone dispatcher clients.

Contained C tests cover all-linked death gating, optional missing-object gating,
one-shot behavior, pending timers, downstream trigger activation and unknown
objects. The existing rendered rifle round trip remains a regression check;
it is not proof of a complete authored death-triggered mission chain. Native
XEMU gameplay verification and wider object-family coverage remain open.

Validation: PC and NXDK builds pass, all36 CTests pass (including expanded
mission_goal_dispatch), and the rendered240-frame rifle section round trip
retains its defeated actor and collected pickup. Logs: `artifacts/death-watch-*`.
