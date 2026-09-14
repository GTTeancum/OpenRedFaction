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

## Authored goal-chain replay

`tools/replay_death_chain.py` loads L7S2 and applies fatal damage through the
shared NPC damage/death-entry/animation services to watcher5012's linked guards:
4887 at frame30,4907 at60. This fixture bypasses aiming and player ammunition;
it never sets watcher state, invokes Goal_Set or changes goal counters directly.
At59 frames the first guard is dead, watcher5012 is unfired and door1=0.
At90 frames both are dead, watcher5012 fired at1016ms and its authored link5011
incremented door1 to1. The other six watchers remain unfired. Both PC cases pass.
The authored Goal_Check5015 needs door1>=2, then targets Goto4994 (delay0.5s),
which targets NPC4952 (admin_male, Gryphon). It is not a direct door-controller
link. Watcher5013's separate pair must also be defeated; scripted Gryphon movement
and the remainder of the sequence remain open.

Per-watcher telemetry retains authored UID, one-shot state and firing time in a
bounded32-record snapshot (largest authored level currently has18 watchers).
XEMU `--watch-uid` uses the same scene-local damage fixture and compares every
watcher, fixture outcome and mission goal directly with PC. It saves/restores
campaign-watch.bin with its existing fixture cleanup; no host input is sent.

Native result: `artifacts/xemu/replay-20260914-062247/report.json` passes90
frames on stock64MiB. All seven watcher triples, damage fixture [2,4907,60,0],
and goals door1=1/door2=0/vator=0 exactly match PC. Native framebuffer inspected;
6057 free pages (23.66MiB) remain. Both builds and36 CTests pass; the strengthened
firing-time assertion also passes. Native first-death-only control has not been
run separately; that gating control currently has rendered PC evidence.

## Event outputs to movers

The runtime dispatcher now forwards on-links resolving to registered kind8
controllers to a borrowed activation service. The scene uses its existing
translation-controller activation, source/actor gates, pending requests and
sound handling. Subsequent simulation/render/collision updates remain owned by
the existing controller loop. Ordinary off-links do not activate movers.
Unsupported controller families report an unhandled target; backend errors
propagate. Exact special Invert/mover behavior remains a separate fidelity item.

Contained tests reuse L7S2's authored threshold2 check but explicitly redirect
its output to a test controller; they verify below-threshold suppression, source,
actor and clock forwarding, off suppression, missing backend and error propagation.
They do not imply that the authored output4994 is a mover. Both builds, all36
CTests and the rendered death-to-goal regression pass. Native event-driven mover
motion and direct authored mover-target integration remain unverified. There
are actual direct targets elsewhere, e.g. L17S1 Goal_Check20330 (door17) links
eight shutter keys, and L3S2 Delay7766 links four elevator-door keys.
