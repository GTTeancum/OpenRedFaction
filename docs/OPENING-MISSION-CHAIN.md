# Opening mission: authored waypoint blocker

Performance work is deferred after the final basic-testing pass. This investigation selects the next critical scripted-event implementation; it does not claim a working natural opening sequence.

## Reproduced behavior

The local `artifacts/riot-trigger/entrance` replay crosses player trigger9028 without directly activating its events. It starts Goto8433/8434, both miners reach their destinations, and NPC8432 activates trigger9525. No Riot Stick grant occurs. Follow_Waypoints event9646 remains unsupported.

The authored chain is player trigger9029 -> Follow_Waypoints9646 -> actor8322 following `L1S1MinerBuddy` in `One way` mode. Player trigger9028 enables NPC-filtered trigger9027. Actor8322 entering9027 starts the confrontation, including Delay9871 (14.4seconds), which enables player trigger9869 (`give_riot_stick_trigger`); contact then calls Give_Item_To_Player9870.

## Installed level evidence

L1S1.rfl section0x10000 at offset2855626 has348 payload bytes. The payload parses exactly as a u32 record count (13), then records of u16 name length, name bytes, u32 node count, and u32 node indices. This layout is observed from the installed level, not yet a validated general runtime format.

`L1S1MinerBuddy` contains indices18 and330. In the existing parsed navigation section0x20000:

- Index18 is UID8640, position(-80.531273,-5.681978,47.126621), tags[8322].
- Index330 is UID9673, position(-75.031525,-7.643442,27.972010), tags[].

The latter position lies within confrontation trigger9027. This supports interpreting the path entries as navigation-node indices; validate across other levels before generalizing the loader.

## Remaining implementation

Load bounded named paths into the shared runtime, dispatch event28 to its linked actors, and advance through authored destinations with existing navigation/movement. Verify stop/cancellation and missing-path behavior. Reproduce the opening route through actual trigger contacts on PC, then stock64MiB XEMU. Play_Animation/Look_At and full confrontation choreography remain incomplete; do not equate successful item delivery with retail mission parity.
