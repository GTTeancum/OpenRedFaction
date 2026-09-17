# Detached solid notification integration

Successful rocket terrain publication now passes its cleanup center/radius to the scene-owned piece registry. The shared original-derived kind3 notification decision wakes eligible bodies by setting physics bit80000000 and object bits06000000; it adds no impulse and changes no position. Retired pieces are skipped. Validation scans every live body before any mutation. Rejected terrain edits do not call the notification.

Evidence: GEOMOD-POSTEDIT-OWNER-CONTRACT-20260916.md and GEOMOD-POSTEDIT-RADIUS-CONTRACT-20260916.md describe executed original487370,40a420 and48ab40. The registry is scene-scoped with no parent and supplies room-bound eligibility explicitly. Full scene changed-box collection is still missing; this integration supplies only the existing rocket cleanup radius. Other edit producers remain open.

RFPB2 retains wake/transform bits06000000 alongside damage00200000 and retirement2. Positive-health retirement is accepted because generic geometry notification can retire without damage; nonpositive health still requires retirement. Existing version1 compatibility remains unchanged.

Validation: all121 PC CTest cases passed before the additional later-body atomicity case; the rebuilt terrain extraction test passes that added case too. Tests cover near/far notification, malformed center, repeated wake, no impulse/position change, snapshot roundtrip, and a malformed later body preserving earlier bodies/output. Stock64MiB NXDK build succeeds (existing linker warning remains). Native runtime acceptance of this change is pending.

Live PC investigation: artifacts/geomod-postedit-re/detached-support-cut contains ordinary550-frame two-rocket runs. First impact detaches the post chunk. Second aimY -1.6/-1.1 rejects with RF_FORMAT; -.95 rejects with RF_RANGE; -.8 directly retires the chunk; .4/.5/.6/.8 commit a second terrain edit but leave its settled pose unchanged and emit no DETACHED_WAKE. These are useful negative controls, not proof of support-loss motion. No original game or desktop input was used.

Next: publish the actual bounded changed-component boxes, then exercise a successful edit that removes a resting piece's support and verify its next physics steps and native checkpoint continuation. Do not enlarge the radius merely to force a visible result.
