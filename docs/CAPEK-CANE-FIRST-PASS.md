# Capek Cane: delayed retained-lock energy flight

Source staged against `c621714b`, 2026-10-10. No compilation, syntax checks, tests, XEMU/PC run, new gameplay fixture, route traversal, event firing or inventory grant was performed by this implementation worker. Parent owns integration and the scheduled Xbox batch. All attack, collision, save, audio, model/tag and stock-64-MiB runtime behavior remains unverified.

## Real missing selected-owner attack

Read-only original `levels2.vpp` decoding finds L11S3 Capek UID4938 and L8S4 Capek UID8359 explicitly selecting `Capek Cane`/`none`. Both begin hidden. Other Capek instances selecting `none` remain unarmed. This change requires actual selected ownership and does not activate actors or alter authored loadouts. Ordinary and opposed weapon admission previously rejected Cane before firing.

Original evidence: installed RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`; weapons.tbl1905–1935; entity.tbl588–657. The exact selected L11S3 record is at RFL-relative offset1311829; entry offset17197056, size1905210, SHA256 `86d24185992fb1380e53373b0b921481f70e1f602dbfc070b6c9b71310a999b8`.

## Exact admitted definition

Dedicated profile admission requires `NanoAttackMissile.vfx`, energy kind5/damage225, clipless power cell capacity200, speed20, lifetime2.5 seconds, collision radius0.5, damage radius0, AI range15, fire wait3 seconds and primary delay1.5 seconds. Mass0.2 is validated metadata; the existing finite direct-flight primitive does not add gravity, thrust, bounce or a mass-driven collision response. Homing must be true with Turn Time6, View Cone120 degrees, Scanning Range40 and Wakeup Time0. Sticky and Glow must be false. Unsupported clip, reload, burst, spread, piercing, alternate-delay or alternate weapon-type declarations are rejected.

The reset catalog contains authored descriptor264=0 and descriptor268=0x10. Original loader4c3ba7 adds homing264 bit0x800000, but the current reset catalog intentionally does not; separate exact Homing admission avoids demanding a synthesized bit from authored flags. Other primary/explosive parsers and selectors are unchanged.

## Paid delay and current held muzzle

Original426197–426273 begins action2, schedules cadence4b8 and delayed launch4c0, plays Launch, then calls4257c0;42580e–425827 debits the actual mapped reserve. The staged path reserves one of16 fixed paired windup/flight slots before paying one real cell, publishes cadence and a monotonic ticket, and requests the existing fire presentation exactly once. Presentation failure cannot refund or repeat a paid onset. A presentation callback cannot reuse an old reservation. Finite exhaustion is the existing port inventory policy.

The due service runs before ordinary target, pain, range, cadence, reserve/fallback and mode gates. Pending final-cell shots bypass ordinary reserve fallback. Hidden owners retain an overdue deadline, then release once on their visible update. Source identity, living state and selected ownership are requalified; removal, death, Disarm and successful replacement cancel pending ownership without refund. Released projectiles retain their immutable full source handle independently.

Original40939b/4093aa consumes the delayed deadline before factory40956b. At release, the adapter prepares the current actor pose and calls `rf_scene_npc_muzzle(handle, weapon, 0, -1, ...)`, preserving the held muzzle position, then supplies the owner's current full eye basis. This matches409526→41b040→41b25d/41b5a0 and409557. Capek lacks the drools-slime branch used by Snakes. No onset ray or intended target is frozen.

The existing model loader requests all actually owned NPC third-person models, including hidden actors. Admission additionally requires a retained Cane model, first primary hand and `muzzle_1` before payment and again at release. The generic helper's missing-tag eye+forward0.3 fallback is not silently used. Absence refuses a new onset; an already-paid release whose required resource disappears is consumed without a substitute shot or refund. Current pose/model/tag runtime remains unverified.

Independent read-only asset inspection confirms meshes.vpp `capekscane.v3m` at4034560 (1858 bytes), first-LOD attachment `muzzle_1` index0 at model-relative1186 and `grip_1` index1. `Capek.v3c` at3833856 (99896 bytes) has30 bones, no primary-tag bone shadow, and first-LOD attachment `primary_weapon_1` index2 at45650, parent12 (right hand), giving port tag32 and one primary hand. There is no `primary_weapon_2`; class flags omit the20000000 tag-initialization bypass. Local muzzle position is(-0.000923002662602812,0.00015391661145258695,0.44963252544403076), parent-1; local primary-hand attachment position is(0.05498308315873146,-0.006300478242337704,0.00883821677416563), parent12 `capk-bdbn-hand-r`. This proves source asset availability, not runtime residency or memory admission. capekscane.v3m SHA256 `579081930c0107c6fcdb744c2ad5abbe2dafb897cc54a125610b5b2b601edc24`; Capek.v3c SHA256 `76a931115af02090e0f07ef656239869ec1d111fc9f87ec1e548cdc395920ebe`.

## Original scan and retained guidance

Creation4c7c17→4fa360 stores target=-1 and a scan deadline with zero OFFSET from the current clock, not a literal deadline0. The first flight update can scan immediately. While unlocked,4c69cc–4c6a29 scans only when due and then schedules200ms. A new lock is not steered until the next update because the original scan and retained-steer branches are exclusive.

The bounded candidate set is living, visible, generation-qualified skeletal NPCs plus the current physical on-foot player. It excludes the exact source, dead810&1, hidden7c&0x4000, body bounds radius180 below0.5, and ambient class724&0x800000. It has no faction/hostility filter. Torpedo-only wet admission is inapplicable. Actual body radius and class flags are used. Extra finite/registration/removal guards are port safety policy.

- Eye distance uses original4faf30→4fa7a0 approximate magnitude: sorted absolute components a>=b>=c; p=c/8+b/4; distance=a+p+p/2, stored to binary32 before comparisons.
- Range is strictly less than40. Replacement is strictly nearer; equal distances retain this adapter's NPC-index-then-player enumeration. Exact original global-list order is not claimed.
- Cone4c6ef0 uses normalized eye delta dotted with projectile forward, at least cos(120 × float0.01745329238474369 ×0.5).
- LOS4c6e83 queries flags3 to the eye first and to the body only if blocked. Either clear path admits. Projectile no-world collision does not bypass acquisition visibility.

Retained4c6f60 resolves the stored full handle, then guides toward current BODY position. It does not rescan/recheck faction, death, hidden state, range, cone or LOS. A missing/reused target remains locked and ballistic, with no invented replacement. A dead/hidden but still retained identity can still supply a body point. Contact eligibility remains the existing shared live-body policy.

For speed=|current velocity| and u=normalize(target body−projectile position),4c6fca–4c7064 applies normalize(velocity + u × speed × dt ×30.00029945373535 /6) ×speed. This is the original vector-addition law, not angular clamping or the vehicle homing approximation. Zero/invalid-vector guards are explicit port safeguards; newly bit-exact x87 operation order is not claimed. Orientation4c7080→4fcea0 uses the existing reconstructed `rf_entity_navigation_basis` helper. Energy-kind5 bypasses the original generic near-target miss self-destruction4c6a8e–4c6aa4.

The gameplay loop's existing60-Hz step is retained. Lifetime2.5 seconds starts at projectile release, independently of the preceding1.5-second windup.

## no_world_collide and contacts

Factory4c79e0–4c79f2 clears physics bit0x10 while retaining object-pair bit0x20. Physics4878e0/49bba4 therefore skips49bb70, which owns moving solids64e6e0 and static world4df1c0/liquid416260. Level movers are type9, outside independent pair48be00's kind0..7 switch. Cane's scoped query skips static world, movers and liquid.

Independent object-pair48ca60 at487849 remains. Detached solid fragments are kind3 with a collision-solid pointer at294, not a class pointer; those remain eligible through48c39b. The shared port compositor therefore retains its detached-solid polygon sweep, actors/physical shields, retained clutter, vehicles and turrets. Other energy flights retain their existing zero-initialized query and unchanged environment path. Existing sphere/polygon approximations, source-related exclusions, ordering and small-fragment differences are not newly claimed as exact pair-loop parity.

Terminal flight state publishes before direct callbacks. Existing qualified physical/Nano shields consume the shot, including shield-breaking/stale contacts. Ordinary accepted direct contact carries immutable energy225; it does not produce a hitscan replacement, radial blast, terrain crater, ricochet or penetration. Expiry does not damage nearby actors. All update/presentation/service guards survive reset until their stacks unwind, and epoch changes prevent stale local flight publication.

## Save/lifecycle and presentation

Pending/reserved windups, live flights and active callback ownership are save-only transient blockers mirrored from TriBeam: player snapshot, projectile capture, world snapshot/save, Driller capture and direct export. RFNC/RFAP wire layouts are unchanged. Shared NPC-row capture used by load preparation remains admissible. Only successful load/world/life publication, teardown and fresh timeline reset clear runtime attack state; failed staging does not. Settled three-second cadence remains in the existing saved combat deadline.

`Capek Cane Launch` now maps through the existing accepted-onset positional Launch consumer. Foley declarations already exist, and optional missing PCM is reloaded through the existing bounded audio owner. Failure/exclusion/start counters retain their real meaning; empty action Foley is not reported as audible success. No new persistent audio owner or mandatory PCM allocation is added.

Optional projectile presentation is a separate bounded deliverable. Integration points are clearly marked after flight/lock publication and at attack reset. Warmup `NanoAttackSource.VFX`, flight audio `Capekcane_Fire_02.wav`, `NanoAttackHit` impact presentation and impact audio are not implemented by this mechanics patch. No speculative replacement mesh is drawn.

## Explicit remaining limits

Separately retained vehicle acquisition and mounted-player acquisition are deferred, even though original entity-list scans could include those owners. Vehicle/turret impact damage remains available through shared contact. The physical player-eye cache avoids cinematic-camera acquisition; when no qualified on-foot eye exists, shared shield collision retains its existing camera fallback. Scripted point/once and vehicle-directed onset are deliberately excluded; actual ordinary/opposed actor-directed selection is admitted. Optional presentation/resource admission, natural firing, final-cell release, scan/lock/LOS behavior, wall/mover pass-through, object/shield contacts, source retirement, save refusal/restore and stock memory/playback remain unverified pending the parent’s scheduled Xbox batch. No campaign-completion or boss-encounter completion is claimed.

## Separate visual integration

The original composite flight visual is now source-integrated through the reserved reset and accepted-flight hooks; see CAPEK-CANE-FLIGHT-VISUAL.md. It remains optional and compilation/runtime-unverified. Warmup, impact effects and flight audio retain the exclusions above.

## Separate flight audio integration

Original flight-loop ownership is separately source-integrated in CAPEK-CANE-FLIGHT-AUDIO.md. This supersedes only the flight-audio exclusion above; actual playback remains unverified.

## Separate impact and expiry audio integration

Original dry contact and homing-expiry sound is now source-integrated in CAPEK-CANE-IMPACT-AUDIO.md. This supersedes the impact-audio exclusion above only; NanoAttackHit visuals and warmup remain deferred. Source-only after the preserved 08:00 failure and reviewed declaration-boundary repair; no successful rebuild or playback claim.
