# Opening mission: authored waypoint blocker

Performance work is deferred after the final basic-testing pass. Follow_Waypoints is now connected to shared actor movement; the full natural opening sequence remains unverified.

## Reproduced behavior

The local `artifacts/riot-trigger/entrance` replay crosses player trigger9028 without directly activating its events. It starts Goto8433/8434, both miners reach their destinations, and NPC8432 activates trigger9525. No Riot Stick grant occurs. That initial replay did not cross the upstream Follow_Waypoints trigger.

The authored chain is player trigger9029 -> Follow_Waypoints9646 -> actor8322 following `L1S1MinerBuddy` in `One way` mode. Player trigger9028 enables NPC-filtered trigger9027. Actor8322 entering9027 starts the confrontation, including Delay9871 (14.4seconds), which enables player trigger9869 (`give_riot_stick_trigger`); contact then calls Give_Item_To_Player9870.

## Installed level evidence

L1S1.rfl section0x10000 at offset2855626 has348 payload bytes. The payload parses exactly as a u32 record count (13), then records of u16 name length, name bytes, u32 node count, and u32 node indices. This layout is observed from installed data. The shared bounds-checked parser now passes all68 single-player path sections; the largest payload is510 bytes. A separate inventory scan also parses the multiplayer sections (94 total).

`L1S1MinerBuddy` contains indices18 and330. In the existing parsed navigation section0x20000:

- Index18 is UID8640, position(-80.531273,-5.681978,47.126621), tags[8322].
- Index330 is UID9673, position(-75.031525,-7.643442,27.972010), tags[].

The latter position lies within confrontation trigger9027. This supports interpreting the path entries as navigation-node indices; validate across other levels before generalizing the loader.

## Shared implementation and verification

The scene retains one bounded section payload (348 bytes in L1S1, hard cap64KiB) and each actor borrows its selected path. Node access decodes unaligned little-endian indices. Malformed lengths/counts, trailing data, embedded NULs and out-of-range indices fail validation without publishing output. Installed UINT32_MAX placeholder nodes are structurally accepted; requesting such an unresolved path returns NOT_FOUND without changing actor movement. Missing/empty paths do not start movement.

Event28 dispatches through the existing movement callback, including delayed activation and off/cancellation. One-way movement visits each authored destination through existing routing, ground support, collision and steering, then stops. Loop and Ping Pong have practical endpoint wrap/reversal behavior, but have not yet received an end-to-end authored replay. Exact original mode, speed, nearest-start and interruption semantics remain unverified. The normal actor-death/removal paths stop movement; new Goto/Follow commands replace the path.

`python tools/replay_waypoints.py` stages outside player trigger9029 and uses ordinary walking to activate9646. It supplies no direct setup/event dispatch. The2400-frame PC replay records one movement request,906 movement steps, both destination arrivals, zero blocked steps, and no active movement at completion. Miner8322 ends within0.25 horizontal units of node330 and contacts NPC trigger9672 near the first waypoint. All38 PC tests pass, including malformed/missing/placeholder paths and delayed on/off dispatch.

The replay does not cross player trigger9028, so9027 remains disabled and the Riot Stick grant does not occur. Extend the actual walking route through9028 and the handoff volume next. Play_Animation/Look_At and full confrontation choreography remain incomplete; do not equate successful path traversal with retail mission parity.


Stock64MiB XEMU replay `replay-20260914-132101` passes2400 frames with exact PC movement, actor-position and route counters. NXDK build and normal-build restoration succeed. Reproduce with `python tools/xemu_replay_check.py artifacts/waypoints/inputs.bin --campaign-spawn --level L1S1.rfl --exit-start-uid 9646 --seconds 240`.
