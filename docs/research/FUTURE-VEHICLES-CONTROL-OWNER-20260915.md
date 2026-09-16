# Vehicle per-frame input ownership

Priority P1: directly separates driver movement from jeep passenger aiming. Original RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

## Original execution

`tools/future_re/vehicle_control_owner.py` executes complete cdecl4a6060(player*) for96 cases: ordinary actor, driver, jeep gunner, missing actor; camera modes0/2/4; player input-lock flag on/off; player+f38 byte0/1; and in-vehicle predicate0/1. It supplies entity lookup, occupant predicates and input writer/final processor. Actual40d740 reads camera mode+8, actual430fc0 clears selected control state, and actual4fad00 zeros gunner movement. Result: control-owner.json. No host input, emulator, build or source implementation edit.

The fixture verifies the selected record, temporary gates as seen by the writer, final gates as seen by430c70, gunner direction clearing, and actor flag changes. Supplied writer writes direction(1,2,3), with untouched records initially(9,9,9). It does not execute hardware input or430c70's weapon/action processing.

## Ownership rule

Resolve player+14 through426fc0; default owner is actor+708 (null if missing). Camera comes from player+c4. Camera mode2 calls431030(player) instead of the ordinary writer and preserves that default owner for subsequent processing. Camera mode4 takes the object pointer at camera+0 and uses its+708 record, overriding vehicle selection.

In ordinary camera mode, actor+200 greater than-1 resolves its host.4290d0(actor) nonzero sets temporary player+14c and+168 to1. Crucially this predicate does not decide input ownership:42acd0(actor) (jeep gunner) keeps actor+708, otherwise owner becomes host+708. The same driver selection occurs even when4290d0 is supplied false. Stale host handling is not covered; the original pointer arithmetic is unsafe to copy without validation.

430720(player,chosen_record) receives these temporary gates. Afterwards player+14c is reset0 and player+168 becomes1 if player+f38 is zero, otherwise0. Player+10 bit8 invokes430fc0 on the nonnull selected record. Thus locked driver input must clear host controls, not merely the passenger's old control record.

Finally a jeep gunner forces chosen_record+c direction to zero with4fad00 and sets actor+7c bit100; a resolved non-gunner clears that bit.430c70(player,chosen_record) still runs in both cases. Gunner aiming/action processing therefore remains active while movement is suppressed. The direction clear even follows camera-mode4 ownership in this original composition; its normal reachable combinations need camera integration evidence.

The fixture proves zero direction after either locking or gunner classification, preserved(9,9,9) for unlocked camera-mode2 nongunner, and writer(1,2,3) for other unlocked nongunners. Missing actor ordinary mode forwards null to both supplied handlers; do not infer those handlers' null safety from this probe.

## Campaign and integration

L1S2 Driller01 UID8122 and L1S3 APC UID9627 need host-owned driver commands after boarding. L12S1 jeep entry event9694 and Follow_Waypoints9692 are the campaign worker's concrete authored jeep example; this report does not claim its gunner seat is player-used. Seat predicates are already recovered in ENTITY.md; ordinary movement composition4307a0 is already in COLLISION.md and must not be reimplemented as a separate vehicle-only direction formula.

The missing live integration is the control owner selection between src/diagnostic/scene.c input handling and existing shared movement processing. Keep actor and vehicle control records distinct. Select owner once, apply lock to that owner, preserve separate passenger aim, and route camera/action processing consistently. These are logical shared C/C++ ownership decisions and do not require raw original struct layouts.

Still open: real vehicle physics controller consumes+708/+714; gunner430c70 weapon routing; camera attachment position/orientation; validity/lifetime of host/camera pointers. Original dead/unattached actor special predicates42d8b0/42a0a0 were supplied false here and remain outside this vehicle-focused evidence.
