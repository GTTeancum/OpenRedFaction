# Ordinary NPC blind-pursuit expiry

Source-written on `7fd55bbc4959ef0fb499db2a26782c53c87d2843`, with subsequent parent-approved ownership refinements. External source review only: no new compile, runtime, gameplay fixture, campaign route or stock-memory measurement. Parent integrates with its later Stand Ground, NanoShield and player auto-reload changes, then owns the hourly Xbox compile/package and short startup smoke. Testers own functional coverage.

## Behavior and original evidence

Ordinary armed humanoids now consume the authored `$Blind Pursuit Time`: after a retained living target remains unseen longer than the class interval, the actor releases ordinary combat and returns through its existing Waiting/Waypoints default. This is disengagement, not a new last-known-position search or tactical system. Stand Ground still only suppresses pursuit. Explicit actor Attack uses the same unseen interval; fixed-point Shoot_At does not. Queued Shoot_Once/Fire_Weapon_No_Anim keeps its independent FIFO and accepted-shot policy.

Original RF.exe SHA256: `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- `41c4ff..41c510` reads the required class+718 duration in seconds.
- `409050`, especially `40905d..409063`, returns immediately for the same target. A newly accepted target seeds AI+298 at `4090e4..4090e9` and makes sight sampling due at `409123..40912b`. Repeated same-target Attack must not grant fresh grace.
- `402180` maintains target visibility independently of acquisition range/cone, shots and reloads. Two object-aware body/eye rays update visibility; only either result exactly1 refreshes AI+298 at `4022d8..4022f3`.
- `406dba` refreshes visibility before `406dbf..406e06` tests strict elapsed > class duration and calls target setter409050(-1). Equality remains live. A subsequent no-target service calls default selector407ee0 at `406d1e`.
- `409080..4090b4` retains action/AI80 target-release gates. Attack events at `4bcb32..4bcb48` and `4bcb6e..4bcb8a` use the ordinary target setter, not a permanent-target exemption.

The installed source scan covered94 RFLs and entity.tbl:952 ordinary holds-weapons source candidates across64 levels, comprising851 with30s,99 with20s and2 with5s. The broader958-row cohort includes97 Stand Ground placements. There are32 actor-linked Attack events with32 distinct owners across12 levels; six empty/unresolved records were excluded. The largest source level cohort is50 in L20S1. Examples include L1S1 env_guard8456, L3S1 guard1/3, miner1/789 and guard2/781, and L6S1 guard2/5125. These are authored candidates, not constructed/live actor counts or a stock64 fit measurement.

The existing `rf_entity_ai_blind_pursuit_expired` scalar helper and `rf_entity_blind_pursuit_read` reader are reused; their separate vehicle consumer is unchanged. The reader uses `sphere_number`, a bounded manual decimal/exponent parser (`entity_assets.c:370`), not NXDK's asserting strtof/strtod stubs. The class table is read from the existing charged constructor scratch, with no second table buffer or new archive open.

## Live ownership and service order

The admitted source is a registered living, standalone ground humanoid with walk/holds_weapons/sentient/humanoid class flags, positive mass, real body/model/pose ownership, authored Waiting/Waypoints default, no authored seat or held clutter, and initially a supported ordinary handheld weapon. Actual player, seated/linked owners and special classes are excluded. A mover `controller_handle` is an authenticated activation backlink, not exclusive AI control: ordinary door activation retains the receipt and existing RFNC UID/full-handle validation remains authoritative.

Initial admission and retention are distinct. Once present, the same qualified source/target receipt survives a valid weapon change, including Disarm; canonical inventory and selected/base motion mapping are still checked by ordinary RFNC capture/restore. An unsupported new or legacy target gains no receipt. If an absent alert first becomes fully eligible through weapon selection or target dismount, ordinary combat service seeds fresh grace. This bounded domain-entry adaptation never resets an existing same-target receipt and never mutates capture.

Each live NPC has independent `{present,target,last_seen}` state. No fire, navigation, reload, pain or noise deadline is reused. Accepted sight, opposed acquisition, hearing, damage and Attack mutations initialize only a genuinely new qualified target. Actual target cancellation, friendliness/AI/default changes, point-fire, seat/control transfer, retirement and scene teardown clear it at their existing committed transitions. Hidden living targets remain registered owners and age normally, but cannot refresh through geometry-only sight rays.

The corrected scheduler order is important: `animation.c:511` invokes `placement.prepare_view`, which runs actor_follow_view -> campaign_combat_tick -> campaign_enemy_tick; only later `animation.c:552` invokes the scene-frame sink and campaign_script_step movement. A single Blind sight/expiry service therefore runs in enemy combat **after paid delayed obligation service, before new enemy fire, and before subsequent movement**. The player eye receipt is already available. The movement pass performs only idempotent age maintenance, never a second sight query.

Age bookkeeping runs before per-owner suppression in both traversals. Unsigned frame subtraction is capped at floor(duration*60)+2 by rebasing the anchor; continued hiding/freeze cannot wrap a saturated receipt into fresh grace. Durations must be finite0..86400 seconds, keeping the retained interval below2^23 frames. Save/load rebases age to the current combat frame. This is the port's simulation-frame clock, not a claim that every pre-existing system's UINT32_MAX sentinel has been redesigned.

Temporary suppression retains age: hidden/frozen/scripted-physics actors, current nonground movement override, guard retreat (including qualified2 before entered flight), alarm/medic/noise, cutscene and endgame owners do not get tactically cancelled. Reload and Stand Ground are not sight-refresh exclusions. A due successful ray refreshes before expiry evaluation. Release honors the AI80/action policy and waits while existing Drone Smash/TriBeam/Cane/Spit order-coupled paid obligations remain; RFAP and paid attacks are unchanged.

On admitted expiry, only combat target/alert/order/burst/fire/navigation state and genuine combat-origin follow2 pursuit are released. Existing default-patrol return owns route resumption; it does not replay arrival events. Authored movement, once requests, reload/ammo, paid flights, support and velocity are retained.

## Bounded work and adaptation

Sight uses the existing30-frame stagger and at most two existing geometry/mover rays, body then eye if blocked. It deliberately does not reuse the acquisition20-unit/cone gate, weapon range, shot cadence or navigation success. This is an initial practical adaptation of original200–399ms randomized object-aware thick-ray polling, not original polling/occlusion fidelity.

For N live array slots the feature adds at most2*ceil(N/30) rays per combat frame (138 at the2048 format ceiling), with a single per-frame combat service. Age maintenance is bounded O(N), with no allocations. A live NPC target resolves through registry identity plus checked member-address span/stride, without an all-target scan per owner. Immutable bounded RFL source fields are checked during qualification; entity.tbl is not parsed per frame. Candidate/UID joins retain existing bounded row scans. The installed50-candidate peak does not replace the all-slot ray bound or imply an observed live count.

## RFNC37 and final publication

RFNC37 appends a4-byte presence word and, only when present, a4-byte unsigned age. No handle is serialized: the existing implicit player or typed target UID joins the actual prepared generation. In-memory `{blind_profile,blind_present,blind_age}` adds12 bytes; the explicit nonwire profile distinguishes legacy absence from strict new-profile absence.

- A present row preserves its exact bounded age, including saturated expired age and temporary suppression. Invalid source, target, age or identity refuses loading.
- RFNC<=36 qualified combat gets explicit fresh grace only after complete candidate admission. It does not invent elapsed history.
- RFNC37 absence rejects only after the source and target fully qualify in the composed incoming on-foot state. A genuinely excluded occupied/attached target remains absent; malformed identity does not become an exclusion.
- Real RFNS prepared seat state, retained constructor death bit810, full handles, registered target body and validated living player state are joined. Authored target seat_host alone cannot reject a valid dismounted target. The source actor's authored no-seat scope is unchanged.
- Existing RFNC already rejects arbitrary non-RFNS linked handles and attachment_75c. Blind does not create a broader attachment grammar or turn an unknown link into valid absence.

Before NPC resource demand, outgoing or incoming present receipts select the existing protected final-close transaction. Potential-only legacy/new-absent participants reserve its bookkeeping/budget without prematurely imposing the on-foot gate. After RFNS/player preparation and before coupled NPC publication, composed qualification either prepares legacy grace, refuses qualified RFNC37 absence, or leaves an excluded target absent. Actual admitted receipts then activate the protected lane; an unresolved stage fails closed through `blind_joined`.

The candidate stores `{blind_target,blind_last_seen}` separately. The publication seal covers profile/presence/age, prepared target/anchor, joined stage header, class metadata and whole outgoing owner. Full composed joins run before seal begin and again immediately before native close. The outgoing receipt survives failed close; final-success finish publishes the already validated pair or explicit absence by assignment only. No acquisition, LOS, expiry/default callback or mutable legacy promotion occurs during capture or publication. Existing earlier terrain/cache and parked-world policies remain unchanged; this is not a new rollback guarantee for all world mutations.

Ordinary saves and standalone capture/restore use matching protected-owner guards. RFCH13 retains its existing section policy: no new all-level blind-age history is introduced. Successful teardown discards this current combat owner; after expiry, existing route/default history suffices.

## Memory and limits

N = live NPC-body array slots; R = copied restore rows; A = present receipts. All figures below are source-derived Xbox32 layout estimates, not a new compiled sizeof or measured peak. Existing object layout was inspected read-only, and each allocation continues to use actual `sizeof` at build time.

| Object | Before | After | Delta |
|---|---:|---:|---:|
| Live campaign_npc_body |3,112|3,128|16N (12-byte payload plus alignment)|
| RFNC decoded record |2,336|2,348|12R|
| Restore entry (saved, before, owner_before and prepared pair) |10,496|10,544|48R =24R+16R+8R|
| Publication seal row |2,568|2,592|24R (20-byte fields plus alignment)|
| Restore stage header |56|64|8|
| Publication header |6,736|9,312|2,576 =2,568 class copy+8 stage-header copy|

Persistent class metadata is separately2,568 bytes (640 floats plus valid/count), explicitly charged to the constructor. The live receipt payload and metadata have compile-time size assertions. The approved loader-local potential flag adds4 bytes and is charged once at loader entry.

Additional overlapping allocation costs, including persistent live owners:

- Resource-inspection decode:16N+2,568+12R+4. These decoded rows are freed before world restore staging.
- World decode + restore preparation:16N+60R+2,580 (live class2,568 + stage header8 + loader flag4). This decoded row array is freed before publication sealing.
- Sealed restore:16N+72R+5,156 (both class copies5,136 + two header alignment deltas16 + loader flag4). There is no simultaneous decoded row array in this phase.

These are feature deltas, not complete existing loader memory totals. Every saved/before/owner/seal copy is included; fixed allocator metadata, compiler stack padding and unrelated existing resources are not represented as feature allocations. Existing sizeof-based2MiB world-stage admission and640KiB NPC-constructor admission charge the grown types; neither limit nor the1,280KiB motion cache is increased.

Existing local RFNC record copies grow12 bytes each (codec decode, capture, restore prepare/validate and world probe). Existing whole-NPC stack copies grow16 bytes each, including medic-flight candidate, passive-seat rollback and retained developer-only copies (four simultaneous copies add64 bytes). The largest new helper's declared locals are544 bytes on Xbox32:512-byte held-name scratch,16-byte AI record, pointer/scalars and scoped loop index. New qualification calls are nonrecursive; compiler spills/padding and nested existing-callee stacks remain unmeasured. No new large stack row or per-frame allocation is added.

Wire growth is4R+4A, at most16,384 bytes at2048 rows. The2,240-byte row cap,110,524-byte checkpoint-file cap and all actor/flight pools remain unchanged. Row-span/encoder checks reject overflow instead of increasing caps. Extra wire bytes use the existing fixed file buffer. No new asset residency is required by target-age admission.

## Review status and remaining implementation boundary

The source plan and subsequent memory/ownership refinements were parent-approved. This document accompanies the final external patch for exact independent source acceptance; it is not a build or gameplay result. Parent composition must preserve the later Stand Ground backlink correction, NanoShield hooks, automatic player reload and unsigned build fix. Broader sight fidelity and gameplay defects belong to tester feedback, not an expansion of this slice.
