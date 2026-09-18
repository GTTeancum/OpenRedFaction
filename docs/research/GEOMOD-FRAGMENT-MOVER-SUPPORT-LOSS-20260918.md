# Fragment wake after translating support leaves

The scene now checks inactive extracted fragments when an enabled mover has a valid translating-only interval. A short vertical mesh sweep from 0.005 units above to 0.005 units below the settled pose identifies an old upward support (normal Y >= 0.5). The previous mover pose is reconstructed locally from the retained interval; current global geometry is never changed. If that mover no longer supplies support, the production scene query checks current world and mover geometry before admitting wake.

This is an explicit port policy, not a recovered original fragment wake producer. The original activation and scheduler evidence in GEOMOD-MOVER-WAKE-BOUNDARY-20260918.md still applies. The narrow support band and normal threshold are policy choices.

A newly admitted body receives only its active flag and the ordinary gravity/contact step against the current committed scene. It does not replay the departed support interval, inherit mover velocity, or gain a synthetic impulse. State is staged per piece and published only after query and step success; this is not a transaction over all pieces. Existing object/lifetime flags and save format are unchanged. Active bodies retain the translating relative-contact route. Static shape sweeps are shared with that route without adding per-query heap allocation.

## Validation

All 126 PC tests pass (artifacts/fragment-support-loss-ctest.log). Targeted controls exercise a departing support, replacement-floor protection, query-error output/body preservation, and a subsequent ordinary falling step. The shared native fixture verifies the same numerical behavior; it does not install a persistent gameplay fixture.

artifacts/xemu/render-20260918-044333 passes 82 checks over 600 frames on stock 64 MiB with no plugged memory and 3312 available pages (12.94 MiB). The 16 support audit words match PC exactly, alongside the prior contact/edge/moving audits. Wake is admitted without replacement support, rejected with replacement support, and errors preserve the sentinel and input body. The first gravity step lowers the body with negative vertical velocity.

The 5300-byte checkpoint remains exact to PC and the prior accepted state (SHA256 d27cfa99dae3f1595e695e85f32b777a8f8b856017c85b944cab34c241601aba). The framebuffer SHA256 ad18ceadb48c89b8c98548d169a7e9bc51b785197a4ac223c326bcc9bab41ca3 matches the previously inspected render-20260918-020011 output. No duplicate image was posted. The harness restored the disc and its emulator closed. Mean active debris time was 37.37 ms in the unchanged regression; this is not an optimization claim.

## Remaining limits

An authored visible mover/rubble sequence, lateral support withdrawal, multiple simultaneous contacts, rotating swept support, approaching-surface wake, carrying and crushing remain unverified or unimplemented. The current-world query selects its earliest contact; broader mixed-normal support cases need coverage. These numerical fixtures and unchanged room replay do not establish complete moving-platform gameplay or visual behavior.

The optional inner-work profiler patch still applies and remains disabled.
