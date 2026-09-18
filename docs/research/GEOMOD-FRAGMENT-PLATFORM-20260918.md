# Visible developer platform and rubble replay

The explicit CTF06 fixture adds a 3 x 0.2 x 3 unit box mover (UID900001) centered at (9.449,0.55,2.5), beneath the source108 rocket test. It uses ordinary serialized mover geometry, resource ownership, rendering and collision. The RFL mover section is generated locally; original installed inputs stay unchanged. Its six faces reuse a texture already named by the level. This is a diagnostic platform, not an authored retail object or a visual-parity claim.

Run `python -B tools/check_fragment_platform.py --moving` from the repository root after the PC build. It regenerates the disposable archive and runs a fresh stationary control followed by the moving case. The generated single-level archive costs 4546560 bytes. Other required VPPs and bluebeard.bty are hardlinked to installed inputs, not copied; the builder refuses to overwrite a linked archive. Everything generated stays under artifacts/fragment-platform and is untracked. The tracked tools contain no game payload.

RF_REPLAY_FRAGMENT_PLATFORM_TEST is headless-only PC activation. The shared scene hook accepts only a DEV scene with exactly one mover, UID900001. It keeps the platform still for420 controller commits, then supplies60 kinematic steps totaling3 units along X and stops. This is explicit fixture motion; ordinary committed pose synchronization, retained intervals, support-loss wake, gravity, collision and rendering do the remaining work. The current25-word audit records commits, moving commits, admitted wakes, platform XYZ float bits, supported fragment bottom before/after withdrawal, wake commit, a wake marker, final fragment XYZ, flags, piece index and sampled update count, followed by the nine final mover-basis words. Production runs leave the hook disabled.

## Acceptance

The ordinary source108 replay fires a rocket and extracts three real pieces. At600 frames the stationary control has fragment mesh bottoms [-1.5,0.650000036,-1.5]. The second fragment rests on the platform top. With motion enabled, the audit is599 commits,60 movement commits and one admitted wake; the platform finishes at (12.449,0.55,2.5). All three fragment mesh bottoms reach-1.5, and all three settle. Thus the supported fragment drops2.15 units through the production scene path. Cut history remains [108,1,103,1,99,0]. These are asserted in the script, with reports/logs under artifacts/fragment-platform.

The native PC renderer's stationary.png and moving.png were individually inspected. The stationary image shows the slab and rubble resting above it; the moved image shows the slab withdrawn to the right and the former supported rubble down at floor level. This is a before/after visual check, not inspection of every intermediate frame or verification of audio quality. The fixture was not uploaded to GitHub.

The first load lacked the required bluebeard.bty local link; adding it fixed audio resource initialization. Enabling player/destruction checkpoint capture then correctly hit the existing mover-exclusion save guard. The fixture now intentionally disables checkpoint saving. An initial assertion selected piece0; the per-piece audit identified supported piece1 and the assertion was corrected to that actual identity.

All126 PC tests pass (artifacts/fragment-platform-ctest.log). The NXDK Xbox build succeeds (artifacts/fragment-platform-xbox-build.log). This initial acceptance was PC-only; the subsequent native acceptance below covers the same visible fixture. The prior82 native support-loss checks remain separate numerical evidence.

## Open

Extend the fixture to rotating supports and mixed replacement support. Carry/crush and rotating swept contacts are still incomplete. A complete checkpoint for movers must include their state rather than relaxing the existing guard. The ordinary campaign remains outside this developer fixture.

## Native fixture plumbing

The shared scene audit now captures the supported piece's actual mesh bottom at commit420 and its final bottom, wake commit, final position and flags. The PC control matches the support audit and reports one wake at commit462. Native flag fragment-platform-test.flag selects the same shared motion hook. The native level selector additionally accepts the fixed fixture basename fragment-platform.vpp.

The render harness option --fragment-platform-test requires the explicit source108/three-source CTF06 developer replay without checkpoints. It builds the disposable fixture, uses that archive for the PC reference, and stages a separate4.5MB fragment-platform.vpp on the disc. The normal levelsm.vpp is never replaced. The restoration manifest records the temporary archive as originally absent; cleanup removes it and the flag, verifies the normal archive hash and repacks the disc with the tested XBE. An unexpected existing fixture archive is rejected before mutation. No disk image clone or large hex backup is created.

Command: python -B tools/xemu_render_check.py --fragment-platform-test --dev-room --spawn --level ctf06.rfl --archive levelsm.vpp --authored-source 108 --authored-sources 3 --input artifacts/side-group108/shot.bin --seconds 240.

## Stock64MiB Xbox acceptance

artifacts/xemu/render-20260918-051405 passes78 checks over600 frames. All16 platform audit words match PC exactly:599 controller commits,60 movement commits,one wake at commit462,initial fragment mesh bottom0.6500000358 and final bottom-1.5 (drop2.1500000358). The final fragment is inactive/settled; the production detached-motion hash and final body checks also match PC. This live fixture suite excludes the separate numerical contact fixtures and checkpoints, so its check count is not directly comparable with the earlier82-check suite.

Native memory is67108864 bytes with zero plugged memory and3311 available pages (12.93MiB). The final native framebuffer was inspected: the platform is withdrawn to the right and the previously supported rubble rests on the floor, matching the PC after-state. Intermediate native frames and audio quality were not visually/audibly reviewed. The before-state has the numerical mesh-bottom evidence plus the inspected PC stationary control.

The normal levelsm.vpp SHA256 remains557bf43984206628d0961154285ad9f2e02a60ad5a1bd6f2fabc99a1a02b5e7f. Disc restoration succeeded, fragment-platform.vpp and its flag are absent afterward, and the owned emulator closed. No screenshots were uploaded. All126 PC tests pass again in artifacts/fragment-platform-native-ctest.log, and the optional inner-work profiling patch still applies while remaining disabled.

## Tipping regression and stranded sleepers

PC command: python -B tools/check_fragment_platform.py --tipping. Mode2 keeps the center fixed and prescribes a90-degree tip over the same60 movement commits. A rational quarter-circle parameterization (c=(1-t*t)/(1+t*t), s=2*t/(1+t*t)) avoids backend trigonometric differences. This is explicit kinematic diagnostic motion, not a recovered original angular controller. Collision and rendering matrices are updated together. The final25-word audit includes the complete final basis, which must be [0,-1,0,1,0,0,0,0,1]. Mode1 translation remains unchanged.

The first tipping run exposed a failure: the fragment woke three times but finished suspended with mesh bottom0.636345685 even after the support became vertical. Requiring an old endpoint contact within the0.005-unit probe band missed bodies that a prior collision had stopped just outside that band.

For a rotating mover, the support-loss policy now rechecks inactive pieces whose mesh bounds overlap the old or current mover bounds, without requiring the stale old contact. A current upward mover support, or the existing current world/other-mover support query, still prevents wake. Translating movers retain the previous-contact requirement. This is a deliberate port policy extending wake eligibility, not a claim of original wake semantics. A PC control includes a sleeper0.02units above the old face that would previously be missed.

The repaired PC replay reaches the floor at-1.5 and all three pieces settle. It records37 wake admissions, with the final wake at commit480. Repeated stopping/waking during tipping remains a contact-response limitation; this test resolves the stranded final state but does not establish smooth rolling/sliding, angular swept contact, carrying or crushing. Do not call rotating-platform gameplay complete from this result. The mode1 replay still records exactly one wake at462 and the same2.15unit drop. All126 PC tests pass (artifacts/fragment-platform-tip-ctest.log).

Native command: python -B tools/xemu_render_check.py --tip-platform-test --dev-room --spawn --level ctf06.rfl --archive levelsm.vpp --authored-source 108 --authored-sources 3 --input artifacts/side-group108/shot.bin --seconds 240. This implies the same temporary archive/flag restoration discipline as translation.

Tipping native acceptance: artifacts/xemu/render-20260918-052250 passes78 checks over600 frames on stock64MiB, with zero plugged memory and3311 free pages (12.93MiB). All25 audit words match PC, including37 wakes, the final wake at480, the full90-degree basis and the2.15-unit drop to a settled final body. The final native image was inspected and shows the vertical platform; it partially occludes debris, so the mesh-bottom and settled-state claims rely on the native memory checks rather than the image alone. Intermediate motion and audio quality remain unreviewed. The temporary archive and flag are absent after successful disc restoration, and the owned emulator closed. No new GitHub image was posted.
