# Authored player look integration rate

The normal scene passed the literal1.0 to `rf_look_update_pose`. Read-only
inspection of original RF.exe SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`
shows49de6f and49de90 multiply pitch/yaw commands by class+64 and frame
seconds, before adding pending mouse/recoil deltas. Loader41beb9..41becc
places `$Rot Acceleration` at+64; `$Max Rot Vel` is the separate+60 field.
The original uses the acceleration-named scalar directly on this player path.

Installed miner1, parker_suit and parker_sci all declare2.0 for that field.
The scene now reads the actual values once during its existing table-loading
phase and selects the current player form's value each frame. It adds12 bytes
to the scene, with no per-frame table lookup or allocation. Source metadata
is reloaded at startup; ordinary saved form identity selects the same value.

This restores the original field relationship rather than choosing a new
sensitivity. Xbox stick normalization, deadzone, scope scaling, pitch clamp,
fixed timestep and existing controller sampling policy remain separate.
Noncampaign diagnostic rigs retain their existing1.0 probe input.

At normalized unscoped command1, the written code now integrates2 radians
per simulated second for the installed player forms. This is a source/math
statement, not a measured controller-feel or wall-clock turn-speed result.
No builds, tests, gameplay runs or screenshots were performed; the parent
owns the15:00 UTC consolidated Xbox pass.
