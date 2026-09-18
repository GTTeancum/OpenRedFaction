# Visible developer platform and rubble replay

The explicit CTF06 fixture adds a 3 x 0.2 x 3 unit box mover (UID900001) centered at (9.449,0.55,2.5), beneath the source108 rocket test. It uses ordinary serialized mover geometry, resource ownership, rendering and collision. The RFL mover section is generated locally; original installed inputs stay unchanged. Its six faces reuse a texture already named by the level. This is a diagnostic platform, not an authored retail object or a visual-parity claim.

Run `python -B tools/check_fragment_platform.py --moving` from the repository root after the PC build. It regenerates the disposable archive and runs a fresh stationary control followed by the moving case. The generated single-level archive costs 4546560 bytes. Other required VPPs and bluebeard.bty are hardlinked to installed inputs, not copied; the builder refuses to overwrite a linked archive. Everything generated stays under artifacts/fragment-platform and is untracked. The tracked tools contain no game payload.

RF_REPLAY_FRAGMENT_PLATFORM_TEST is headless-only PC activation. The shared scene hook accepts only a DEV scene with exactly one mover, UID900001. It keeps the platform still for420 controller commits, then supplies60 kinematic steps totaling3 units along X and stops. This is explicit fixture motion; ordinary committed pose synchronization, retained intervals, support-loss wake, gravity, collision and rendering do the remaining work. The eight-word audit records commits, moving commits, admitted wakes, final XYZ float bits and two reserved words. Production runs leave the hook disabled.

## Acceptance

The ordinary source108 replay fires a rocket and extracts three real pieces. At600 frames the stationary control has fragment mesh bottoms [-1.5,0.650000036,-1.5]. The second fragment rests on the platform top. With motion enabled, the audit is599 commits,60 movement commits and one admitted wake; the platform finishes at (12.449,0.55,2.5). All three fragment mesh bottoms reach-1.5, and all three settle. Thus the supported fragment drops2.15 units through the production scene path. Cut history remains [108,1,103,1,99,0]. These are asserted in the script, with reports/logs under artifacts/fragment-platform.

The native PC renderer's stationary.png and moving.png were individually inspected. The stationary image shows the slab and rubble resting above it; the moved image shows the slab withdrawn to the right and the former supported rubble down at floor level. This is a before/after visual check, not inspection of every intermediate frame or verification of audio quality. The fixture was not uploaded to GitHub.

The first load lacked the required bluebeard.bty local link; adding it fixed audio resource initialization. Enabling player/destruction checkpoint capture then correctly hit the existing mover-exclusion save guard. The fixture now intentionally disables checkpoint saving. An initial assertion selected piece0; the per-piece audit identified supported piece1 and the assertion was corrected to that actual identity.

All126 PC tests pass (artifacts/fragment-platform-ctest.log). The NXDK Xbox build succeeds (artifacts/fragment-platform-xbox-build.log). The new fixture has not yet run on Xbox: native flag plumbing and temporary fixture archive staging/restoration are still required. The prior82 native support-loss checks remain separate numerical evidence. No Xbox visual claim is made here.

## Open

Execute this same fixture on stock64MiB XEMU, preserving the normal disc archive afterward. Extend the fixture to rotating supports and mixed replacement support. Carry/crush and rotating swept contacts are still incomplete. A complete checkpoint for movers must include their state rather than relaxing the existing guard. The ordinary campaign remains outside this developer fixture.
