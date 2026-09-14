# Opening mission: authored waypoint blocker

Performance work is deferred after the final basic-testing pass. Follow_Waypoints and linked-trigger enabling are now connected. The staged entrance-to-Riot-Stick walking sequence passes on PC; full confrontation choreography and the rest of the campaign remain incomplete.

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

The earlier waypoint-only replay does not cross player trigger9028, so9027 remains disabled and the Riot Stick grant does not occur. The extended replay below covers that missing route. Play_Animation/Look_At and full confrontation choreography remain incomplete; do not equate successful path traversal with retail mission parity.


Stock64MiB XEMU replay `replay-20260914-132101` passes2400 frames with exact PC movement, actor-position and route counters. NXDK build and normal-build restoration succeed. Reproduce with `python tools/xemu_replay_check.py artifacts/waypoints/inputs.bin --campaign-spawn --level L1S1.rfl --exit-start-uid 9646 --seconds 240`.


## Linked-trigger dispatch fix and opening handoff

The extended route crossed9028 and started both Goto events, but9027 stayed disabled. The shared `rf_trigger_links_dispatch` admitted only events(kind6) and movers(kind8); it dropped linked triggers(kind5). Original4c0320 dispatches kind5 at4c0378 to4c0200, which clears bit16 at trigger+2b0. It enables the target without firing it or resetting its activation count/cooldown. The shared dispatch now forwards kind5 to the scene, which performs that same flag change.

The older `verify_trigger_links.py` compared only recorded event/controller calls. It executed the original enable branch but never observed it, so its passing result did not establish complete trigger-link behavior. The expanded verifier records trigger-enable calls, executes the original enable unchanged, and checks that only bit16 clears. All1024 kind/order/suppression combinations match PC/NXDK callback traces. Other original dispatch families, including kind4 object actions and ambient fallback, remain outside this implementation.

`python tools/replay_opening_handoff.py` starts once outside9029, walks into the hall through9028, and approaches9869. It sends no setup events, forced grants/slays, or later position changes. Three PC controls pass:

- Hall-only2400 frames: confrontation starts, but the player never enters the handoff volume; stays unarmed.
- Full approach1750 frames: player is in the handoff volume but the scripted delay has not finished; stays unarmed.
- Full approach2400 frames: exactly one authored Riot Stick grant, selected weaponID2, loaded charge1, reserve0; no player death. Two authored Slay_Object actions also execute.

The final player position is(-75.388870,-8.095128,27.786995). The NPC-trigger chain and delayed player contact now deliver the weapon naturally within this staged entrance route. This is not a start-to-finish mission proof: Play_Animation/Look_At, confrontation presentation, full actor combat behavior and onward campaign traversal remain unfinished.


Xbox handoff verification remains incomplete: the240-second attempt timed out while frames were advancing; the600-second retry (`replay-20260914-133827`) lost its QMP connection. These are terminal harness results, not a successful native handoff. The user then paused tests and redirected work to structural Xbox optimization. The PC controls and original/PC/NXDK dispatch verifier above passed before that pause.
