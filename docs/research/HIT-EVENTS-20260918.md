# Hit-triggered gameplay events

When_Hit52 now polls current hit flags through a non-consuming runtime callback. Registered player, NPC and clutter owners expose object flag0x200000. Every event observer runs before the scene clears this bit; NPC view/room copies and player damage flags are cleared consistently. Other flags are preserved. Damage after that event pass remains available for the next poll.

The existing damage admission sets the signal before immunity, so an invulnerable object can still drive a hit event. Pending common deadlines delay polling; own disabled state does not suppress the recovered observer. A hit dispatches linked events/movers using sentinel source/actor IDs. Trigger enabling is not substituted.

Focused event_hit, runtime_hit and scene_hit_events checks pass: multiple observers see the same signal, delayed propagation is respected, registered player/NPC/clutter queries repeat without consumption, stale handles are rejected and clearing preserves unrelated flags. PC and Xbox builds pass. A live authored shootable-button scenario remains unverified; this batch establishes runtime/scene routing, not every campaign puzzle.

## Remote explosion correction
Installed table loading confirms rocket_impact and charge_explode share the same rocket hit particle recipe and Medium Explosion foley. Their visual radii differ: rocket1.5, remote2. The shared emitter start now accepts the authored radius, and remote detonation invokes the existing impact sound service. PC replay remote-audio.log shows one launch/attachment/detonation, one successful terrain cut, and IMPACT_AUDIO one request/load/play with status0. Endpoint raster inspected for visible enlarged blast and held detonator. This proves sound dispatch, not an independently heard audio recording. Xbox build passes; prior save/load native runs predate this radius/audio correction.
