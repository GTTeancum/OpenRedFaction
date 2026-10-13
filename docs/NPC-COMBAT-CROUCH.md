# Bounded ordinary combat crouch

Status: parent source review completed October 13, 2026, 05:04 UTC; UNCOMPILED pending the next authorized stock64 rollup. No functional test claim.

## Source and scope

The read-only installed RF.exe is the source of the reconstructed policy. Addresses 4068f4–406978 toggle intent after tactical repositioning: difficulty 0/1 can request crouch with 1500 ms switch/attention/tactical clocks; leaving crouch chooses 3000–4000 ms. The existing independent 500 ms primary-fire recovery is preserved. Constructor clocks at 402cee/402cfa start due. The private retained RNG is separate from unrelated gameplay RNG.

The 406146–4061fb attention branch uses an independent clock, difficulty 0/1 refresh of 600–1200 ms, behavior other than 11, player target, body or eye visibility, and a strict player-facing dot product above .95. This implementation uses fresh world and mover rays. The original cached 200–399 ms actor/clutter/corpse visibility bank is not reconstructed here; Blind Pursuit age is not substituted for it.

Motion follows 429ae0 eligibility and 41f61d–41f7c1 selection. Special state 9 versus fallback 8 is selected by the original combat predicate, not by walking versus standing. Ordinary action 3 is the admitted consumer; existing higher-priority motion owners retain control. The portable actor's independent 740 override remains constructor-zero, and scripted action 12 is outside this lane. Existing class/model construction supplies a single retained stance cache and eye-offset binding; no alternate-render-class identity is invented.

## Live behavior

A canonical standing, ordinary source-qualified actor is admitted only with authored states 8 and 9 and valid existing shape ownership. Seven future clip identities (states 0, 2, 8, 9 and actions 2, 4, 39) are preflighted under the existing animation budget. Missing optional mappings remain optional. Resource pressure defers admission or motion transition without raising caps or evicting an earlier prepared endpoint.

Each simulation frame cheaply services existing receipt clocks. One deterministic actor slot, frame modulo actor count, receives admission, attention and stance work. A pending blend is preserved between that actor's visits. Physical crouch follows completion of the selected animation transition. Standing uses the real overhead clearance query; a low ceiling can retain physical crouch after combat intent ends. Ground refresh recomputes enclosing sphere bounds after either stance changes its centers.

Physical-crouch firing selects authored action 4 with action 2 fallback. Existing damage, ammunition, firing cadence, paid obligations and independent recovery remain owned by their original consumers. A residual action-4 clip may outlive physical crouch or its target. Death and temporary stronger ownership retain shape state; terminal body retirement clears it.

## Saved continuation

RFNC39 adds a conditional 20-byte present tail (4-byte absent tail): flags, switch and attention time remaining, private RNG and stance timestamp age. Header size and row/file limits are unchanged. Older mutating readers do not silently consume the new version. A float-seconds timestamp is rebased from elapsed age independently of the unsigned frame origin.

Restore prepares old and proposed sphere contents, exact class-cache inputs, policy and resource demand privately. Bounds, world placement and actor/prop pair checks use the same candidate sphere view with explicit UID, full handle and slot binding. Physical shape is independent of current target, weapon or combat intent. Composed player, seat and attachment owners retain their existing restrictions.

The existing early-close publication lane receives no new fallible calls after close. A separate late-close lane preserves vehicle-compatible ordering, authenticates assigned owner/pose state and deterministically rebases resource references before final storage close. Outgoing typed bodies retain registration and storage through assignment and are unregistered/freed only on successful finish. This does not claim general rollback for the complete world transaction.

## Stock64 accounting

These are source bounds, not measured compiled sizes. The live NPC allowance grows by 32 bytes. Decoded rows grow by 24 bytes. Restore-entry and shared-seal bounds are 11456 and 3360 bytes; each retains 632 bytes of old/proposed/cache geometry. Fourteen additional unique resource receipts conservatively contribute 4480 bytes per row. The conservative shared peak delta is 32N + 6032R bytes plus headers; the independent late lane adds at most 6736C + 40 bytes. N is allocated NPC count, R is restore-row count, and C is the number of shape rows in the late lane.

The callback guard has a 928-byte Xbox32 source allowance. Admission overlaps that guard, a 428-byte candidate and 632-byte seal before nested helper/local costs; this is not a measured whole-call-stack maximum. Existing allocation sizes and compile-time assertions remain authoritative.

The 2240-byte row ceiling, 2048 campaign actor-slot count, 110524-byte checkpoint, 640-KiB NPC restore, 2-MiB world restore and 1280-KiB motion limits remain unchanged. Exhaustive behavior, combat and save/load coverage belong to testers. Only the authorized short stock64 startup smoke will be run by the parent.
