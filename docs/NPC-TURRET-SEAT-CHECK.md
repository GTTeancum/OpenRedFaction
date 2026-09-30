# Bounded Xbox NPC turret seat check

`tools/xemu_npc_turret_seat.py` prepares a local CTF06 fixture containing the complete authored L3S2 `Stationary Turret` UID935 and `guard1` UID936 records. Only their transforms change; the NPC's original seat-host UID935, class, equipment, vitals and other authored fields remain intact. Original CTF06 entities are absent, scripts/triggers are removed, and ordinary pickups remain. The two hostile authored actors are the only added actors.

The host faces the unchanged player spawn from four metres away. The NPC starts near the authored `interface_1` location, but runtime seating must publish the actual model-tag pose. The fixture sends 120 neutral process-local replay frames, without scripted orders, forced damage, campaign traversal, images or host input. It deliberately omits the synthetic catatonic turret damage fixture flag.

A live probe at or after frame30 checks the seat counters, control association, NPC action13, generation-bearing actor/host handles, physical linked-host identity, positive NPC health and repeated pose publication. Reading live state avoids mistaking normal teardown detach/reset for a bind failure. A retained pose sample at the end must retain the same actor/host identity and NPC vitals. The harness checks suppressed handheld attack attempts and zero actual handheld shots, then requires ordinary turret acquisition/fire, attributed damaging hits and a net player health decrease without healing. Authored armor pickups are reported and permitted. Player death is permitted only in this attack harness; the generic native gate is unchanged.

Expected telemetry includes `rf_scene_npc_seats[10]`, `rf_scene_turret_operator[8]`, combat/shot counters and `rf_scene_npc_seat_probe[12]`: actor UID, host UID, actor handle, host handle, health bits, armor bits, actor action, linked host handle, actor world XYZ bits and seat tag. Pose telemetry proves runtime state publication, not visual animation correctness.

Native execution is pending parent integration/serial validation. No save/load, dismount, Jeep control, animation appearance or audible-output claim is made by this fixture. Input/disc flags and the staged archive are restored in `finally`, followed by a restoration rebuild. Asset recipes and native artifacts stay outside tracked source.

## Native result

`artifacts/xemu/npc-turret-seat-20260930-131904/report.json` passes120 neutral
frames on stock64MiB. The live probe reports the guard alive at75 health/40
armor, action13 and the correct host link,34 pose updates and33 handheld
suppressions. The turret fires two damaging shots; player health falls from100
to80.800003 with no NPC handheld shots. Endpoint free pages3,093 (~12.08MiB).
Disc inputs restored. Seat position changes as the turret aims; this records
pose following but does not inspect animation appearance or audible output.
Occupied saves and Jeep driver route/detach behavior remain separate checks.
