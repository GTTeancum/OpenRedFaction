# Final Spit + Cane composition review

Disposition: APPROVED. No blocking findings. Source-only approval for combined.patch SHA256 `baa52ab5aac59e8b62faeb0098db35112c307605e4f97aeabaf353318759d8a3`, based on `95bae7b64dbf226c75c393aba97bff9068139a00`.

## Exact composition

The entire three-file diff was independently regenerated and equals the submitted combined.patch byte-for-byte. All baseline bytes equal the named Git commit and repository files at inspection. The stage contains exactly the three manifest source members. Ordered composition of approved Spit `bc7cbdeba9d65276687274ee6dd89c9423e256dc60a89ed512b3b43ab4fc54ff` followed by approved Cane `a8f47c3bf829ac5b56ed418c3bbac782249c7cd642afc33c7338705c65fceb77` reconstructs every final file. Cane begins at the exact approved Spit resolver, not the older Laser/TriBeam/Sonar resolver.

Final SHA256 values:

- src/diagnostic/scene.c: `b05bee12aa11407e953b98d54c2173b356d1b5840f90e87d9fc85ace231f90c0`.
- src/diagnostic/scene_ai_laser.inc: `f8b7330a491ffd60e32b405e7759eb26c76145b9fb0f283bdc0995dfec7cad72`.
- src/diagnostic/scene_ai_spit_flight.inc: `3cc7e65fa40f0dde71dde848bf3bbd8872c865965cb9ff465f3f89fd96c56634`.

## Joint findings

1. **Definitions precede all new uses.** The unchanged current-player helper is at scene.c:12136. Exact Cane and Spit profile includes occur at12192–12193, before the shared direct consumer at16367. The existing radial policy at15892 precedes the Spit flight include at16371. Both family predicates are read-only exact admitted metadata lookups. No new signature, declaration or mutable shared state is introduced.

2. **Direct family selection is additive and bounded.** The shared final-player OR preserves Laser, TriBeam and Sonar and adds only exact accepted Spit and Cane. It uses copied shot.weapon, the captured actual player target and unchanged raw request. Physical player shields return before this gate, including selected stale/broken shields. The current-player helper remains immediately before the single player-damage API, with no intervening callback or new source/held-weapon/UID qualification. Default1 and existing error/feedback propagation remain intact. NPC physical shields/Nano, NPC damage, props, detached solids and vehicles are unchanged.

3. **Spit direct and Big radial remain separate requests.** The direct request retains raw80 and AP kind2; no scalar is written to the shot. After the single direct-result call, the existing status/consumed/epoch check still decides whether radial may run. Big's radius>0.1 branch now calls the already reviewed policy with the original contact point, raw damage, blast radius, source, independent blast kind3 and exact pool_epoch pointer, followed by1. Small radius0 never enters that branch. The original point is preserved without a missile normal offset, and the direct victim is not newly excluded from the independent radial population.

4. **Guard and whole-contact consumption are preserved.** Reversing that exact call replacement reconstructs the entire baseline Spit flight include. Physical shields and Nano still consume the entire contact, including breaking hits, suppressing Big radial. The pre/post callback epoch checks, guard pointer, audio order, ordinary expiry behavior and terminal publication remain unchanged. The shared radial implementation itself is byte-identical because scene.c changes only its descriptive comment. Its fresh falloff/cover requests, NPC/Nano paths, player full-identity checks and post-callback epoch guards therefore remain intact. Old radial wrappers still select policy0; no general blast widening occurs.

5. **Cane stays independent from Spit-specific behavior.** Cane's only executable delta is its canonical predicate in the shared direct OR. Its dedicated terminal still calls the direct consumer exactly once, with raw225/kind5/force0 and its own epoch. It acquires no radial, Spit wet-audio limitation, contact-point adjustment or new miss/expiry behavior. Copied shot/lock, retained homing ownership, actual muzzle, scoped world pass-through and presentation remain unchanged.

6. **One scalar multiplication per final player request.** The shared helper selects only a scalar. The unchanged SP dispatcher applies it once under existing minimum/force/kind/immunity rules before class/armor. Big's radial starts from raw shot damage rather than direct applied damage; Cane is direct-only. These independent pulses are not duplicate application to the same request. Current difficulty is read at each qualified final request, without launch-time caching or rescaling stored health/armor.

7. **Deadline, paid-flight and save boundaries do not change.** Source owners, exact profiles, Spit/Cane windup/release and finite-ammo code, Cane flight, checkpoint and world-snapshot dependencies equal their baseline Git blobs. No tickets, deadlines, reserves, RNG, snapshots, active-update guards, reset epochs, save-only vetoes, failed-load preservation or successful publication owners are changed. No structure, heap allocation or persistence field is added. Existing source-independent paid-flight semantics remain intact; this scalar correction is not a broader lifecycle repair.

## Limits and integration

Read both component design/exact-patch reviews and their integration notes. Original source qualification remains bounded to their existing documented owners and contracts; this review does not repeat asset discovery or claim runtime parity. Existing collision, natural-action, scheduling, homing, audio, shield and save limitations remain. The prior completed 03:00 batch does not compile or validate these new bytes.

The repository was clean at95bae7b6 at review inspection. The reviewer wrote only this external review directory. No code/stage edits, integration, builds, syntax checks, tests, fixtures, runtime/original execution, routes, grants, images or cleanup occurred. Parent may integrate exactly these three approved source files after rechecking baseline applicability. Full component joins, manifest hashes, unchanged dependency identities and source-boundary comparisons are in FINAL-SOURCE-IDENTITY.json.
