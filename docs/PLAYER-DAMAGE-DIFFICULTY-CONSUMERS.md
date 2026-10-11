# Current-player difficulty at qualified damage consumers

## Status

Independently source-reviewed successor to 2ad2ee843d2730574721fa5ebc2866742ea9fadc, integrated only after its successful 03:00 UTC build. Compilation and actual damage behavior of this successor have not been verified. The predecessor build cannot validate these new bytes.

The existing campaign countdown owns difficulty index 0–3. Original 4892c0 selects .1, .25, .4 or .55 for ordinary single-player player damage before class factors and armor. The existing damage dispatcher remains the only multiplication owner. No scalar is stored in a projectile, actor, death receipt or save row.

## Qualified consumers

- Liquid: original lava and acid requests use force 0, kinds 4 and 7. The current on-foot player row is selected only after the existing request has been emitted. Liquid query, material, rate, timing and eligibility are unchanged.
- Tankbot Missile: actual L7S4 UID10696 selects the admitted Missile definition. The accepted definition identity is copied before contact callbacks. Direct damage and radial damage each select the current row independently; the blast starts from fresh raw damage. Source death or weapon changes do not revoke an already accepted flight.
- Real defuse failure: original failure submits ordinary raw 1000, kind -1 and force 0. Current-player difficulty is supplied to that request; immunity and armor still decide the outcome. It is not an unconditional kill. The existing explicit fixed-1 helper and kill-fixture consumer are unchanged.
- Tankbot and Vauss primary penetration: the two separately qualified exact wrappers each supply policy 1 to the shared adapter. Only the final selected player body receives the current scalar, after penetration. Raw shield/Nano amounts, body power and finite fire ownership are unchanged.
- Authored robot death: Tankbot and Drone retain raw 1000 and 400 death receipts. The removal-first consumer supplies policy 1 while retaining its existing death epoch pointer. The source is already retired; current target difficulty does not depend on source registration. Spike's zero damage still exits before victim/scalar lookup.

- Drone Smash: only the copied accepted exact Smash weapon selects the current row at final player damage. The already consumed pending deadline, ticket, source/target identity and geometry requalification stay unchanged. The separately qualified Reeper, Baby Reeper, Mutant Attack 1 and Mutant Attack 2 use their existing admitted metadata predicate for the same final-player row. Their existing one- or two-impact schedules and no-ammo semantics are unchanged.
- Drone Missile: the existing canonical accepted definition joins the same copied family policy as Tankbot Missile. Both direct and radial pulses remain independent. The finite secondary deadline and unsupported active-flight save veto remain unchanged, including under RFAP5.

- Meca Laser: only the accepted exact Laser weapon selects current difficulty at final player-body dispatch after physical shields. Other shared energy families remain unchanged. Finite reserve, raw80, speed20, flight epochs and active-flight save veto retain their existing owners.

The ordinary synchronous NPC player-body correction was compiled at 02:00 UTC. Outside kind-10 scaling is integrated in the 03:00 predecessor. Those existing owners are unchanged by this successor.

## Boundaries

Current-row selection is limited to the exact registered ordinary on-foot player. Unsupported mounted, linked, cutscene or other owner paths retain their prior scalar. Invalid difficulty on a qualified path returns the existing format error instead of inventing a default. The saved difficulty field and its legacy default policy are unchanged.

NPCs, props, physical shields, Nano shields and vehicles retain their existing damage amounts and ordering. Old blast wrappers preserve policy 0. No callback, source-liveness veto, damage timeline or save migration is added. Existing projectile and blast lifetime limitations remain documented in their owner implementations.

The Chaingun and Vauss authored speeds 250 and 275 do not inherit the speed-below-100 exemption established for Tankbot Missile's original miss helper. This correction covers already accepted synchronous player contacts. Original miss scheduling, continuous-flight parity, world-metal traversal and vehicle occupant parity are not established by it.

## Verification

Each component has independent original-source and exact-patch review. Parent composition review binds the combined source to its predecessor and component hashes. No source test, syntax build, synthetic damage call, changed spawn, inventory grant, forced event, route or new save was used to create coverage.

Subsequent scheduled Xbox compilation is required. The existing midnight stock-64-MiB neutral startup remains evidence only for its exact source. It does not prove these damage, armor, death or persistence actions. The exact October 9 saved HDD remains unavailable; the base HDD is not a substitute.

Final eight-file patch SHA256: 87804eaa15512d4564f23900961f3ae1ad97b357c682016a306d21977e5ce7bb. Independent final review SHA256: 8619fb027d9d39ddabce69677213f8ff8fef1d4b10a6dece11ef78bd6f9e1a40. Exact source/review packet: Library libfile_f7af2bfd862c8191924c476eaa1bad68 version0.

## Subsequent exact TriBeam and Sonar extension

Independently reviewed patch6cc9e23615b8a4406781172ace4e2f6d57fe40659ff763399078f626c466c808 extends only the shared final-player accepted-weapon gate to TriBeam Laser and Sea Creature Sonar Attack. Actual known Spike L7S4 UID10810 and Sea Creature L5S3 UID4645 establish force0/kind5, ordinary source factor1, raw225/80 and speeds30/20 below the original pre-body miss threshold. The existing dispatcher still multiplies once. Physical shields and Nano remain raw; prepaid TriBeam windup/current-basis release, Sonar immediate finite/liquid flight, full identities/epochs and active-save veto remain unchanged. No new state or format. These bytes await scheduled Xbox compilation and action validation. Exact evidence/review packet: Library libfile_b04c7bdc6738819190151a58edc4a93d version0.

## Spit and Cane successor

The exact three-file Spit/Cane composition is integrated after independent review. Small/Big Spit direct and independent Big Spit radial requests, and Capek Cane direct requests, select current on-foot player difficulty. Raw shield/Nano handling, paid-flight epochs, ammo, deadlines and save vetoes remain unchanged. See SPIT-CANE-DIFFICULTY-JOINT-REVIEW.md and SPIT-CANE-DIFFICULTY-IDENTITY.json. These bytes await the next Xbox compilation; no action validation is claimed. Durable joint packet: libfile_656939892824819185ae61ed857e3e05 version 0.
