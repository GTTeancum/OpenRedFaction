# Detached solid notification integration

Successful rocket terrain publication now passes its cleanup center/radius to the scene-owned piece registry. The shared original-derived kind3 notification decision wakes eligible bodies by setting physics bit80000000 and object bits06000000; it adds no impulse and changes no position. Retired pieces are skipped. Validation scans every live body before any mutation. Rejected terrain edits do not call the notification.

Evidence: GEOMOD-POSTEDIT-OWNER-CONTRACT-20260916.md and GEOMOD-POSTEDIT-RADIUS-CONTRACT-20260916.md describe executed original487370,40a420 and48ab40. The registry is scene-scoped with no parent and supplies room-bound eligibility explicitly. Full scene changed-box collection is still missing; this integration supplies only the existing rocket cleanup radius. Other edit producers remain open.

RFPB2 retains wake/transform bits06000000 alongside damage00200000 and retirement2. Positive-health retirement is accepted because generic geometry notification can retire without damage; nonpositive health still requires retirement. Existing version1 compatibility remains unchanged.

Validation: all121 PC CTest cases passed before the additional later-body atomicity case; the rebuilt terrain extraction test passes that added case too. Tests cover near/far notification, malformed center, repeated wake, no impulse/position change, snapshot roundtrip, and a malformed later body preserving earlier bodies/output. Stock64MiB NXDK build succeeds (existing linker warning remains). Native runtime acceptance of this change is pending.

Live PC investigation: artifacts/geomod-postedit-re/detached-support-cut contains ordinary550-frame two-rocket runs. First impact detaches the post chunk. Second aimY -1.6/-1.1 rejects with RF_FORMAT; -.95 rejects with RF_RANGE; -.8 directly retires the chunk; .4/.5/.6/.8 commit a second terrain edit but leave its settled pose unchanged and emit no DETACHED_WAKE. These are useful negative controls, not proof of support-loss motion. No original game or desktop input was used.

Next: publish the actual bounded changed-component boxes, then exercise a successful edit that removes a resting piece's support and verify its next physics steps and native checkpoint continuation. Do not enlarge the radius merely to force a visible result.

## Changed-component bounds integration

The registry now retains one original recentered/padded bounds record per extracted source component, before subdivision and mass-center adjustment. It uses rf_geomod_mesh_recenter followed by the binary-verified rf_geomod_notify_append_fragment_box; current-body bounds are not substituted. The original bounds/placement arithmetic is covered by GEOMOD-PIECE-PLACEMENT-20260917.md and GEOMOD-CHANGED-BOX-PUBLICATION-20260917.md. The existing component extraction adapter remains subject to its documented topology/owner limitations.

Each successful rocket edit takes only committed batches added since that edit began. History replay and already-moving bodies do not republish old component bounds. Rejected staging preserves the prior records. The registry's existing16-batch limit is below the original32-box limit; fixed retained metadata adds768bytes across active/pending slots, included in the registry budget. No new heap allocation is introduced. Generic changed-box retirement is enabled as in the original worker.

Tests verify no new boxes for edits without extraction, immutable source bounds after body movement/history replay, preservation after rejected staging, and exactly one new bounds record for the next extraction. All121 freshly rebuilt PC tests pass. Updated stock64MiB NXDK build succeeds. The preceding radial-only build passed native550-frame replay render-20260917-080427; its actual framebuffer shows the cut post, resting tilted chunk, floor, weapon and HUD. This validates that two-shot flow, not loss of support. Updated native acceptance follows separately.

Updated native run render-20260917-080757 passes the550-frame two-cut replay with matching selected PC state. The native framebuffer was inspected: tilted chunk, remaining post stub, room surfaces, rocket launcher and HUD remain present. No support-loss motion is inferred from this unchanged resting pose.

## Real support-loss fix

The lower aimY-.95 shot was rejected because extract_replay_components called the nonempty component graph API after CSG produced a valid empty retained mesh (zero corners, zero faces). No owner callback or budget failure was involved. Empty retained terrain now succeeds with zero extracted components; malformed partially empty data still follows existing validation. Both box/star extraction suites cover a final cut consuming all retained terrain.

`tools/check_detached_support.py` reproduces the ordinary550-frame sequence: firstrocket240 detaches the post, secondrocket420 contacts its remaining support, successful edit448 wakes one settled chunk. There are two committed edits, zero direct detached rocket contacts, and the live piece falls fromY-.558699608 toY-.825477123, then settles with zero velocity. X/Z are unchanged. This is normal gravity/contact following notification, not an added explosion impulse.

The stock-profile NXDK build and all121 freshly rebuilt PC tests pass. The separate direct-rocket retirement regression also passes. `tools/check_authored_restart.py --case support-loss` saves at550 and continues200 idle frames; it exactly matches the uninterrupted750-frame control in4044-byte RFCP,2508-byte terrain history and888-byte publication. The5320-pixel post region also matches. Existing retired-piece checkpoint expectations now retain the independently established06000000 notification flags.

This closes the previously recorded empty-post rejection only. AimY-1.1/-1.6 had distinct RF_FORMAT failures and have not been explained by this fix. Broader materials, moving supports, pushing/crush and fragment-fragment collision remain open.

Stock64MiB native run render-20260917-081511 passes the550-frame support-loss replay, including matched detached pose/motion and checkpoint output comparisons. The actual native framebuffer was inspected: the remaining post stub is gone and the fragment region is near floor level but partly obscured by smoke, with surrounding room/weapon/HUD intact; the exact lower pose is established by native body-state comparisons. Native reload remains a separate open check. PC retired-piece save continuation also passes after retaining notification flags.
