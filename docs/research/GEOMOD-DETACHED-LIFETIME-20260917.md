# Terminal terrain fragments do not expire automatically

`python tools/probe_detached_lifetime.py` passes five original factory cases and60 complete lifecycle cases. The probe verifies the installed RF.exe SHA256 and uses Unicorn without launching the original game.

Full466440 executes the descriptor constructor413700 and actual vector/basis helpers. The descriptor constructor defaults lifetime+48 to1000, but the terrain-fragment factory overrides it to-1 at46649a. Tested radii are0.1,1.5,3,3.0001 and10, spanning the optional texture lookup threshold. Object allocation4130b0 and optional string/texture lookup are supplied boundaries; they are not claimed reconstructed by this probe.

The actual constructor span4131db..413215 reads the negative lifetime and invokes4fa3e0 to store disabled deadlineFFFFFFFF at object+2a4. Full412ad0 then executes real4fa3f0 expiry and48ab40 retirement across clock0/1000/60000/1072800000 and health50/0/-1. Every object byte is compared: positive-health fragments remain unchanged at every clock; nonpositive health sets only retirement bit2. This excludes a fixed lifetime for this producer; generic kind3 objects from other factories may still use timers.

Implementation consequence: the current persistent terrain chunks should not receive an invented expiry timer. Keep their bounded ownership and existing damage/geometry retirement. Reducing memory pressure requires explicit resource reclamation with stable save/history identities, not deleting healthy settled rubble after an arbitrary delay. Current retired storage remains retained until reset/close; reclaiming it safely remains open. This finding does not prove fragment-fragment collision, recursive subdivision on damage, or rendering parity.

Evidence: tools/probe_detached_lifetime.py and ignored artifacts/geomod-postedit-re/detached-lifetime.json. No production code, build or emulator change was needed for this correction.
