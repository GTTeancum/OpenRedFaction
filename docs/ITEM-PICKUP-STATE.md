# Scripted item pickup availability

The authored Item_Pickup_State event54 previously had no live callback. Its five installed SP uses in L7S1, L7S2 and L8S3 control individual placed pickups through switches.

Original ON4b9290 resolves item459a20 and clears item+2bc bit0; OFF4ba090 sets that bit. Common-propagation table4b8c40 admits this event through4b8c5e. These are pickup-acceptance changes; no visibility or retirement behavior is inferred from them.

The scene now holds one override byte per placed item:0 uses its class default,1 enables collection and2 disables collection. The typed event callback receives authored item UIDs because these pickups do not occupy actor registry slots. It updates only matching live scene items, preserves retirement, and reports other target types as not found. The shared class definition is never mutated. Ordinary proximity, obstruction, inventory capacity, finite quantity, pickup retirement, sound and message paths remain responsible for successful collection.

The callback is wired before startup events and its storage lasts for the scene. Explicit pickup-state persistence across ordinary saves/section revisits remains open; this slice does not claim it. Physical dropped weapons use their existing separate lifecycle. No presentation claim follows from counters, and no screenshots or campaign playthroughs are used.

Focused core verification: `rf_event_ai_mode_tests` passes ON/OFF, ordered unregistered item UIDs, duplicate/missing targets, ordinary delayed ON/OFF delivery, missing-backend resumption and error stopping. NXDK build also passes.

## Stock64MiB Xbox functional result

`artifacts/xemu/item-pickup-state-20261003-153922/report.json`: PASS,140 frames, 6389 free physical pages. Original L7S1 Item_Pickup_State4035 and Switch4036 are unchanged; both linked First Aid items4037/4038 retain quantity25 and all fields except staged positions. Ordinary Heal(-25/player) creates the deficit and Invert dispatches OFF at frame0. The frame70 memory probe sees both items disabled, health75 and 57 blocked nearby collection attempts for4037, without retirement or grant. The original switch requested at60 retains its0.4-second delay, enables both items, and the ordinary pickup loop collects only nearby4037 once, restoring25 health to100 and submitting its pickup sound once. Distant4038 remains available. No direct health mutation, script item grant, PC game run, images, host input or campaign walkthrough was used. Disc inputs were restored and the ordinary build rebuilt successfully.

This proves the item collection/state transition path in the bounded fixture. Visibility, audible output, save/revisit state and respawn are unverified.
